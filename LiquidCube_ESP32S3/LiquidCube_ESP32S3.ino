/*
  LIQUID CUBE - Waveshare ESP32-S3-LCD-1.3
  Tela: ST7789 240x240, sensor: QMI8658 (I2C)
  Requer: GFX Library for Arduino (Arduino_GFX)
  Simulacao volumetrica 3D de liquido, inclinacao, ondas, inercia e bolhas.

  Este sketch SUBSTITUI o firmware antigo quando enviado pela Arduino IDE.
  Nao altera o conteudo do microSD.
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <math.h>
#include <esp_random.h>

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
static constexpr float AXIS_Z_SIGN = -1.0f;
// Canvas: (x,y) -> (239-y,x), 90 graus no sentido horario.
static constexpr uint8_t DISPLAY_ROTATION = 1;

static constexpr int SCREEN_W = 240;
static constexpr int SCREEN_H = 240;
static constexpr float WATER_FRACTION = 0.57f;
static constexpr int BUBBLE_COUNT = 17;

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *lcd = new Arduino_ST7789(bus, PIN_LCD_RST, 0, true, 240, 240, 0, 0, 0, 0);
Arduino_Canvas *gfx = new Arduino_Canvas(SCREEN_W, SCREEN_H, lcd, 0, 0, DISPLAY_ROTATION);

static uint8_t imuAddress = 0;
static bool imuReady = false;
// Coordenadas do recipiente: x=direita, y=baixo, z=fundo da tela.
// A camera obliqua revela frente, lateral e profundidade simultaneamente.
struct Vec3 { float x, y, z; };
struct Bubble { Vec3 position; float speed; int radius; };
static Bubble bubbles[BUBBLE_COUNT];
static Vec3 gravity = {0, 1, 0};
static Vec3 filteredGravity = {0, 1, 0};
static Vec3 surfaceNormal = {0, 1, 0};
static const Vec3 CAMERA_RIGHT = {0.8660254f, 0, -0.5f};
static const Vec3 CAMERA_DOWN = {0.25f, 0.8660254f, 0.4330127f};
static const Vec3 CAMERA_RAY = {0.4330127f, -0.5f, 0.75f};
static constexpr float CUBE_SCALE = 55.0f;
static float levelThreshold = 0;
static float wave = 0, waveSpeed = 0, wavePhase = 0;
static unsigned long previousFrame = 0;

static float dot3(const Vec3 &a, const Vec3 &b) {
  return a.x*b.x + a.y*b.y + a.z*b.z;
}
static Vec3 normalized(const Vec3 &v) {
  float m = sqrtf(dot3(v, v));
  if (m < 0.0001f) return {0, 1, 0};
  return {v.x/m, v.y/m, v.z/m};
}

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

// Fracao exata do cubo [-1,1]^3 abaixo do plano (convolucao de
// uniformes). Usa double para evitar cancelamento perto dos eixos.
static float volumeBelow(float threshold, const Vec3 &normal) {
  double widths[3];
  int dimensions = 0;
  double span = 0, denominator = 1;
  const float components[] = {normal.x, normal.y, normal.z};
  for (float component : components) {
    double width = 2.0 * fabsf(component);
    if (width < 0.002) continue;
    widths[dimensions++] = width;
    span += width * 0.5;
    denominator *= width;
  }
  if (!dimensions) return threshold >= 0 ? 1 : 0;
  for (int i = 2; i <= dimensions; ++i) denominator *= i;
  double sum = 0;
  for (int mask = 0; mask < (1 << dimensions); ++mask) {
    double t = threshold + span;
    int parity = 0;
    for (int axis = 0; axis < dimensions; ++axis) {
      if (mask & (1 << axis)) { t -= widths[axis]; ++parity; }
    }
    if (t > 0) {
      double term = 1;
      for (int i = 0; i < dimensions; ++i) term *= t;
      sum += (parity & 1) ? -term : term;
    }
  }
  return clampf((float)(sum / denominator), 0, 1);
}

static void computeLevel() {
  float span = fabsf(surfaceNormal.x) + fabsf(surfaceNormal.y) + fabsf(surfaceNormal.z);
  float lo = -span, hi = span;
  for (int i = 0; i < 18; ++i) {
    float mid = (lo + hi)*0.5f;
    if (volumeBelow(mid, surfaceNormal) < 1.0f-WATER_FRACTION) lo = mid;
    else hi = mid;
  }
  levelThreshold = (lo+hi)*0.5f;
}

static float liquidDepth(const Vec3 &p) {
  return dot3(surfaceNormal, p) - levelThreshold;
}

static void spawnBubble(Bubble &bubble) {
  bubble.radius = random(1, 3);
  bubble.speed = random(30, 74) / CUBE_SCALE;
  for (int attempt = 0; attempt < 64; ++attempt) {
    Vec3 p = {random(-900, 901)/1000.0f, random(-900, 901)/1000.0f,
              random(-900, 901)/1000.0f};
    if (liquidDepth(p) > 0.15f) { bubble.position = p; return; }
  }
  bubble.position = {surfaceNormal.x*0.8f, surfaceNormal.y*0.8f, surfaceNormal.z*0.8f};
}

static void advancePhysics(float dt) {
  float ax, ay, az;
  if (imuReady && readAcceleration(ax, ay, az)) {
    float sx = (SWAP_XY ? ay : ax)*AXIS_X_SIGN;
    float sy = (SWAP_XY ? ax : ay)*AXIS_Y_SIGN;
    // Inversa da rotacao visual horaria: os eixos acompanham o texto.
    Vec3 measured = {sy, -sx, az*AXIS_Z_SIGN};
    float magnitude = sqrtf(dot3(measured, measured));
    // Em queda livre, conserva a ultima direcao em vez de inventar gravidade.
    if (magnitude > 0.14f) {
      Vec3 target = normalized(measured);
      float blend = clampf(dt*8, 0, 1);
      filteredGravity.x += (target.x-filteredGravity.x)*blend;
      filteredGravity.y += (target.y-filteredGravity.y)*blend;
      filteredGravity.z += (target.z-filteredGravity.z)*blend;
      if (dot3(filteredGravity, filteredGravity) > 0.0004f) {
        Vec3 next = normalized(filteredGravity);
        waveSpeed += clampf((1-dot3(gravity, next))*40, 0, 2);
        gravity = next;
      }
      waveSpeed += clampf(fabsf(magnitude-1)*2, 0, 2)*sinf(wavePhase*3);
    }
  }
  waveSpeed += (-wave*27-waveSpeed*6.5f)*dt;
  wave = clampf(wave+waveSpeed*dt, -0.25f, 0.25f);
  wavePhase += dt*2.2f;
  // Balanco em um plano tangente a gravidade, sem criar/perder volume.
  Vec3 reference = fabsf(gravity.z) < 0.9f ? Vec3{0,0,1} : Vec3{1,0,0};
  float projection = dot3(reference, gravity);
  Vec3 tangent = normalized({reference.x-projection*gravity.x,
                            reference.y-projection*gravity.y,
                            reference.z-projection*gravity.z});
  surfaceNormal = normalized({gravity.x+wave*tangent.x,
                              gravity.y+wave*tangent.y,
                              gravity.z+wave*tangent.z});
  computeLevel();
  for (int i = 0; i < BUBBLE_COUNT; ++i) {
    Bubble &b = bubbles[i];
    b.position.x -= gravity.x*b.speed*dt;
    b.position.y -= gravity.y*b.speed*dt;
    b.position.z -= gravity.z*b.speed*dt;
    if (fabsf(b.position.x) > 0.94f || fabsf(b.position.y) > 0.94f ||
        fabsf(b.position.z) > 0.94f || liquidDepth(b.position) < 0.05f) spawnBubble(b);
  }
}

// Intersecao de raio com as seis faces do recipiente (slabs).
static bool cubeRay(const Vec3 &origin, float &nearT, float &farT) {
  nearT = -10; farT = 10;
  const float o[] = {origin.x, origin.y, origin.z};
  const float d[] = {CAMERA_RAY.x, CAMERA_RAY.y, CAMERA_RAY.z};
  for (int axis = 0; axis < 3; ++axis) {
    float a = (-1-o[axis])/d[axis], b = (1-o[axis])/d[axis];
    if (a > b) { float tmp = a; a = b; b = tmp; }
    nearT = fmaxf(nearT, a); farT = fminf(farT, b);
  }
  return farT > nearT;
}

static void projectPoint(const Vec3 &p, int &x, int &y) {
  x = lroundf(120 + CUBE_SCALE*dot3(p, CAMERA_RIGHT));
  y = lroundf(124 + CUBE_SCALE*dot3(p, CAMERA_DOWN));
}

static void drawCubeEdges(uint16_t color) {
  for (int corner = 0; corner < 8; ++corner) {
    Vec3 a = {(corner & 1) ? 1.0f : -1.0f, (corner & 2) ? 1.0f : -1.0f,
              (corner & 4) ? 1.0f : -1.0f};
    for (int axis = 0; axis < 3; ++axis) {
      if (corner & (1 << axis)) continue;
      Vec3 b = a;
      if (axis == 0) b.x = 1;
      if (axis == 1) b.y = 1;
      if (axis == 2) b.z = 1;
      int x0, y0, x1, y1;
      projectPoint(a, x0, y0); projectPoint(b, x1, y1);
      gfx->drawLine(x0, y0, x1, y1, color);
    }
  }
}

static void drawGlass() {
  gfx->fillScreen(rgb565(4, 11, 22));
  gfx->setTextSize(1);
  gfx->setTextColor(rgb565(135, 205, 227));
  gfx->setCursor(24, 11); gfx->print("LIQUID / CUBE 3D");
  // Raio ortografico por bloco 2x2: profundidade real com custo limitado.
  float slope = dot3(surfaceNormal, CAMERA_RAY);
  for (int y = 34; y < 216; y += 2) {
    for (int x = 42; x < 198; x += 2) {
      float u = (x+0.5f-120)/CUBE_SCALE, v = (y+0.5f-124)/CUBE_SCALE;
      Vec3 origin = {u*CAMERA_RIGHT.x+v*CAMERA_DOWN.x,
                     u*CAMERA_RIGHT.y+v*CAMERA_DOWN.y,
                     u*CAMERA_RIGHT.z+v*CAMERA_DOWN.z};
      float nearT, farT;
      if (!cubeRay(origin, nearT, farT)) continue;
      float start = nearT, end = farT;
      float offset = dot3(surfaceNormal, origin)-levelThreshold;
      bool surface = false;
      if (fabsf(slope) < 0.00001f) {
        if (offset < 0) { gfx->fillRect(x,y,2,2,rgb565(10,26,39)); continue; }
      } else {
        float crossing = -offset/slope;
        if (slope > 0) { start = fmaxf(start, crossing); surface = crossing > nearT && crossing < farT; }
        else end = fminf(end, crossing);
      }
      uint16_t color = rgb565(10,26,39);
      if (end > start) {
        float thickness = clampf((end-start)*0.35f, 0, 1);
        if (surface) color = rgb565(76-30*thickness, 205-35*thickness, 237-20*thickness);
        else color = rgb565(18-10*thickness, 153-70*thickness, 218-50*thickness);
      }
      gfx->fillRect(x,y,2,2,color);
    }
  }
  for (int i = 0; i < BUBBLE_COUNT; ++i) {
    int x, y;
    projectPoint(bubbles[i].position, x, y);
    gfx->drawCircle(x,y,bubbles[i].radius,rgb565(167,236,251));
  }
  drawCubeEdges(rgb565(112,192,222));
  gfx->setTextColor(imuReady ? rgb565(77,146,170) : rgb565(255,143,120));
  gfx->setCursor(33,226);
  gfx->print(imuReady ? "INCLINE EM 3 EIXOS" : "SENSOR NAO ENCONTRADO");
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

  randomSeed(esp_random());
  computeLevel();
  for (int i = 0; i < BUBBLE_COUNT; ++i) spawnBubble(bubbles[i]);
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