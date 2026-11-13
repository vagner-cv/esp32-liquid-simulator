/*
  LIQUID CUBE - Waveshare ESP32-S3-LCD-1.3
  Tela: ST7789 240x240, sensor: QMI8658 (I2C)
  Requer: GFX Library for Arduino (Arduino_GFX)
  Simulacao 2D de liquido, inclinacao, ondas, inercia e bolhas.

  Este sketch SUBSTITUI o firmware antigo quando enviado pela Arduino IDE.
  Nao altera o conteudo do microSD.
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <math.h>

// ----------------------- PINAGEM DA SUA PLACA -----------------------
static constexpr int PIN_LCD_DC   = 38;
static constexpr int PIN_LCD_CS   = 39;
static constexpr int PIN_LCD_SCK  = 40;
static constexpr int PIN_LCD_MOSI = 41;
static constexpr int PIN_LCD_RST  = 42;
static constexpr int PIN_LCD_BL   = 20;
static constexpr int PIN_IMU_SDA  = 47;
static constexpr int PIN_IMU_SCL  = 48;

// Ajustes de orientacao. Se a agua for para o lado errado, troque o sinal.
// Se inclinar para direita fizer a agua subir/descer, ative SWAP_XY.
static constexpr bool SWAP_XY = false;
static constexpr float AXIS_X_SIGN = -1.0f;
static constexpr float AXIS_Y_SIGN = -1.0f;

static constexpr int SCREEN_W = 240;
static constexpr int SCREEN_H = 240;
static constexpr int L = 31;    // limite interno esquerdo
static constexpr int R = 208;   // limite interno direito
static constexpr int T = 43;    // limite interno superior
static constexpr int B = 204;   // limite interno inferior
static constexpr float WATER_FRACTION = 0.57f;
static constexpr int BUBBLE_COUNT = 17;

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *lcd = new Arduino_ST7789(bus, PIN_LCD_RST, 0, true, 240, 240, 0, 0, 0, 0);
Arduino_Canvas *gfx = new Arduino_Canvas(SCREEN_W, SCREEN_H, lcd);

static uint8_t imuAddress = 0;
static bool imuReady = false;
static uint16_t waterPalette[48];

struct Bubble { float x, y, speed; int radius; };
static Bubble bubbles[BUBBLE_COUNT];

static float downX = 0.0f;
static float downY = 1.0f;
// Filtro independente: permite girar completamente 180 graus sem travar.
static float filteredX = 0.0f;
static float filteredY = 1.0f;
static float lastRawX = 0.0f;
static float lastRawY = 1.0f;
static float wave = 0.0f;
static float waveSpeed = 0.0f;
static float wavePhase = 0.0f;
static float disturbance = 0.0f;
static float levelThreshold = 0.0f;
static unsigned long previousFrame = 0;

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) |
         ((uint16_t)(g & 0xFC) << 3) | (uint16_t)(b >> 3);
}

static bool i2cWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(imuAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
static bool i2cRead(uint8_t reg, uint8_t *out, size_t n) {
  Wire.beginTransmission(imuAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)imuAddress, (int)n) != (int)n) return false;
  for (size_t i = 0; i < n; ++i) out[i] = Wire.read();
  return true;
}

static bool startIMU() {
  // O QMI8658 pode usar 0x6A ou 0x6B conforme o hardware.
  const uint8_t addresses[] = {0x6B, 0x6A};
  for (uint8_t address : addresses) {
    imuAddress = address;
    uint8_t id = 0;
    if (i2cRead(0x00, &id, 1) && id == 0x05) {
      // CTRL7=0 -> reconfigura; CTRL1=0x40 -> auto-incremento.
      // CTRL2=0x06 -> acelerometro +/-2g a 125Hz.
      // CTRL7=0x01 -> habilita acelerometro.
      bool ok = i2cWrite(0x08, 0x00);
      ok &= i2cWrite(0x02, 0x40);
      ok &= i2cWrite(0x03, 0x06);
      ok &= i2cWrite(0x08, 0x01);
      delay(40);
      Serial.printf("QMI8658: endereco 0x%02X, id 0x%02X, ok=%d\n", imuAddress, id, ok);
      return ok;
    }
  }
  Serial.println("QMI8658 nao encontrado nos enderecos 0x6B / 0x6A.");
  return false;
}

static bool readAcceleration(float &ax, float &ay, float &az) {
  uint8_t raw[6];
  if (!i2cRead(0x35, raw, sizeof(raw))) return false;
  int16_t x = (int16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
  int16_t y = (int16_t)((uint16_t)raw[2] | ((uint16_t)raw[3] << 8));
  int16_t z = (int16_t)((uint16_t)raw[4] | ((uint16_t)raw[5] << 8));
  // Escala +/-2g (16384 unidades por g).
  ax = x / 16384.0f;
  ay = y / 16384.0f;
  az = z / 16384.0f;
  return true;
}

struct Point { float x, y; };

// Recorta o retangulo do vidro pelo semiplano ocupado pela agua.
// O calculo da area mantem o volume aparente durante a rotacao.
static float areaUnderSurface(float threshold, float nx, float ny) {
  Point a[8] = {{(float)L,(float)T}, {(float)R,(float)T},
                {(float)R,(float)B}, {(float)L,(float)B}};
  Point b[8];
  int count = 4, outCount = 0;
  const float cx = (L + R) * 0.5f, cy = (T + B) * 0.5f;
  for (int i = 0; i < count; ++i) {
    Point p = a[i], q = a[(i + 1) % count];
    float fp = nx*(p.x-cx) + ny*(p.y-cy) - threshold;
    float fq = nx*(q.x-cx) + ny*(q.y-cy) - threshold;
    bool inP = fp >= 0.0f, inQ = fq >= 0.0f;
    if (inP) b[outCount++] = p;
    if (inP != inQ && outCount < 8) {
      float t = fp / (fp - fq);
      b[outCount++] = {p.x + t*(q.x-p.x), p.y + t*(q.y-p.y)};
    }
  }
  if (outCount < 3) return 0;
  float twiceArea = 0;
  for (int i = 0; i < outCount; ++i) {
    Point p = b[i], q = b[(i+1) % outCount];
    twiceArea += p.x*q.y - q.x*p.y;
  }
  return fabsf(twiceArea)*0.5f;
}

static void computeLevel(float nx, float ny) {
  const float targetArea = (R-L)*(B-T)*WATER_FRACTION;
  float lo = -300.0f, hi = 300.0f;
  for (int i = 0; i < 15; ++i) {
    float mid = (lo + hi)*0.5f;
    if (areaUnderSurface(mid, nx, ny) > targetArea) lo = mid;
    else hi = mid;
  }
  levelThreshold = (lo + hi)*0.5f;
}

static float liquidDepth(float x, float y) {
  const float cx = (L+R)*0.5f, cy = (T+B)*0.5f;
  float rx = x-cx, ry = y-cy;
  float along = -downY*rx + downX*ry;
  float ripple = wave*sinf(along*0.045f+wavePhase);
  ripple += wave*0.36f*sinf(along*0.089f-wavePhase*1.7f);
  return downX*rx + downY*ry - levelThreshold - ripple;
}

static void spawnBubble(Bubble &bubble, int index) {
  bubble.radius = random(1, 4);
  bubble.speed = (float)random(30, 74); // pixels / segundo
  for (int attempt = 0; attempt < 32; ++attempt) {
    float x = random(L+10, R-10);
    float y = random(T+10, B-10);
    if (liquidDepth(x, y) > 15.0f) {
      bubble.x = x; bubble.y = y; return;
    }
  }
  // Fallback raro, caso o volume do liquido esteja muito lateralizado.
  bubble.x = (L+R)*0.5f + downX*48.0f;
  bubble.y = (T+B)*0.5f + downY*48.0f;
}

static void advancePhysics(float dt) {
  float ax, ay, az;
  if (imuReady && readAcceleration(ax, ay, az)) {
    float sx = SWAP_XY ? ay : ax;
    float sy = SWAP_XY ? ax : ay;
    sx *= AXIS_X_SIGN;
    sy *= AXIS_Y_SIGN;
    float planar = sqrtf(sx*sx + sy*sy);
    float targetX = 0.0f, targetY = 1.0f;
    if (planar > 0.14f) {
      targetX = sx / planar;
      targetY = sy / planar;
    }

    // Filtro da direcao da gravidade, evitando tremedeira de leitura.
    float oldX = downX, oldY = downY;
    float blend = clampf(dt*8.0f, 0.0f, 1.0f);
    filteredX += (targetX - filteredX)*blend;
    filteredY += (targetY - filteredY)*blend;
    float mag = sqrtf(filteredX*filteredX + filteredY*filteredY);
    if (mag > 0.02f) {
      downX = filteredX / mag;
      downY = filteredY / mag;
    }

    // Mudancas de direcao excitam uma onda amortecida (efeito de inercia).
    float angularChange = oldX*downY - oldY*downX;
    waveSpeed += clampf(angularChange*165.0f, -9.0f, 9.0f);

    // Sacudidas geram perturbacao mesmo se a orientacao nao mudar muito.
    float g = sqrtf(ax*ax + ay*ay + az*az);
    float shock = clampf(fabsf(g-1.0f), 0.0f, 1.0f);
    disturbance = fmaxf(disturbance, shock*10.0f);
    waveSpeed += clampf(shock*4.0f*sinf(wavePhase*3.0f), -3.0f, 3.0f);

    // Mudanca brusca de forca tambem produz um pouco de onda.
    waveSpeed += clampf((sx-lastRawX + sy-lastRawY)*1.3f, -1.5f, 1.5f);
    lastRawX = sx;
    lastRawY = sy;
  }

  // Oscilador massa-mola amortecido: liquido balanca antes de parar.
  waveSpeed += (-wave*27.0f - waveSpeed*6.5f)*dt;
  wave += waveSpeed*dt;
  wave = clampf(wave, -17.0f, 17.0f);
  disturbance *= expf(-dt*2.4f);
  wavePhase += dt*(2.2f + disturbance*0.6f);

  computeLevel(downX, downY);

  for (int i = 0; i < BUBBLE_COUNT; ++i) {
    Bubble &bubble = bubbles[i];
    bubble.x += (-downX*bubble.speed + sinf(wavePhase+i*2.7f)*7.0f)*dt;
    bubble.y += (-downY*bubble.speed + cosf(wavePhase+i*1.3f)*5.0f)*dt;
    if (bubble.x < L+4 || bubble.x > R-4 ||
        bubble.y < T+4 || bubble.y > B-4 ||
        liquidDepth(bubble.x, bubble.y) < 3.0f) {
      spawnBubble(bubble, i);
    }
  }
}

static void drawGlass() {
  const uint16_t edge = rgb565(112, 192, 222);
  const uint16_t dim = rgb565(45, 87, 110);
  const uint16_t glass = rgb565(10, 26, 39);

  gfx->fillScreen(rgb565(4, 11, 22));
  // Texto discreto da interface.
  gfx->setTextSize(1);
  gfx->setTextColor(rgb565(135, 205, 227));
  gfx->setCursor(24, 11);
  gfx->print("LIQUID / CUBE");
  gfx->setCursor(180, 11);
  gfx->print("v1.0");

  // Sombra e estrutura de vidro em perspectiva leve.
  gfx->fillRoundRect(18, 28, 204, 191, 15, rgb565(6, 16, 28));
  gfx->drawRoundRect(19, 28, 202, 189, 14, dim);
  gfx->fillRoundRect(25, 36, 190, 174, 7, glass);

  // Marcacoes discretas ficam por tras da agua.
  for (int y = 67; y <= 190; y += 31) {
    gfx->drawFastHLine(34, y, 7, rgb565(29, 67, 84));
    gfx->drawFastHLine(199, y, 6, rgb565(29, 67, 84));
  }

  // Agua: sombreado por profundidade, superficie com ondas.
  for (int y = T; y <= B; ++y) {
    for (int x = L; x <= R; ++x) {
      float depth = liquidDepth(x, y);
      if (depth >= 0.0f) {
        int pi = (int)(depth*0.31f);
        if (pi > 47) pi = 47;
        gfx->drawPixel(x, y, waterPalette[pi]);
        if (depth < 1.9f) gfx->drawPixel(x, y, rgb565(153, 239, 249));
      }
    }
  }

  // Bolhas transparentes que sobem contra a gravidade.
  for (int i = 0; i < BUBBLE_COUNT; ++i) {
    const Bubble &b = bubbles[i];
    if (liquidDepth(b.x, b.y) > b.radius+2) {
      gfx->drawCircle((int)b.x, (int)b.y, b.radius, rgb565(167, 236, 251));
      gfx->drawPixel((int)b.x-1, (int)b.y-1, rgb565(223, 250, 253));
    }
  }

  // Reflexos e aros frontais reforcam a ideia de um recipiente de vidro.
  gfx->drawRoundRect(24, 35, 192, 176, 8, edge);
  gfx->drawRoundRect(27, 38, 186, 170, 6, rgb565(51, 108, 140));
  gfx->drawFastVLine(29, 47, 151, rgb565(92, 159, 188));
  gfx->drawFastVLine(211, 47, 151, rgb565(35, 80, 105));
  gfx->drawLine(35, 41, 202, 41, rgb565(107, 165, 191));
  gfx->drawFastHLine(41, 208, 157, rgb565(52, 104, 129));
  gfx->drawLine(40, 48, 40, 65, rgb565(169, 222, 239));
  gfx->drawLine(41, 48, 42, 47, rgb565(169, 222, 239));

  gfx->setTextColor(imuReady ? rgb565(77, 146, 170) : rgb565(255, 143, 120));
  gfx->setCursor(51, 226);
  gfx->print(imuReady ? "INCLINE PARA MOVER" : "SENSOR NAO ENCONTRADO");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);

  if (!gfx->begin(40000000)) {
    Serial.println("Falha ao inicializar LCD/Canvas");
    while (true) delay(1000);
  }
  gfx->fillScreen(rgb565(4, 11, 22));
  gfx->flush();

  Wire.begin(PIN_IMU_SDA, PIN_IMU_SCL);
  Wire.setClock(400000);
  imuReady = startIMU();

  for (int i = 0; i < 48; ++i) {
    float t = i/47.0f;
    waterPalette[i] = rgb565((uint8_t)(18 - 10*t),
                             (uint8_t)(184 - 92*t),
                             (uint8_t)(251 - 45*t));
  }

  randomSeed(esp_random());
  computeLevel(downX, downY);
  for (int i = 0; i < BUBBLE_COUNT; ++i) spawnBubble(bubbles[i], i);
  previousFrame = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - previousFrame < 33) {
    delay(1);  // cerca de 30 quadros por segundo
    return;
  }
  float dt = clampf((now-previousFrame)*0.001f, 0.005f, 0.06f);
  previousFrame = now;

  advancePhysics(dt);
  drawGlass();
  gfx->flush();  // exibe o quadro completo sem piscadas
}