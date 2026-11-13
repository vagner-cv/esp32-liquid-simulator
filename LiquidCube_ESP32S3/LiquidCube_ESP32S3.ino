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
// A tela inteira e a face frontal z=-1; a camera olha para dentro.
struct Vec3 { float x, y, z; };
struct Bubble { Vec3 position; float speed; int radius; };
static Bubble bubbles[BUBBLE_COUNT];
static Vec3 gravity = {0, 1, 0};
static Vec3 filteredGravity = {0, 1, 0};
static Vec3 surfaceNormal = {0, 1, 0};
static const Vec3 CAMERA_EYE = {0, 0, -4};
static constexpr float SCREEN_SCALE = 119.5f;
static constexpr float BUBBLE_SPEED_SCALE = 55.0f;
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
  bubble.speed = random(30, 74) / BUBBLE_SPEED_SCALE;
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

// O raio atravessa a propria tela (z=-1) e termina em uma parede
// interna. Frente preenche todos os pixels; fundo aparece em perspectiva.
static Vec3 screenRay(float x, float y) {
  return {(x-SCREEN_SCALE)/SCREEN_SCALE, (y-SCREEN_SCALE)/SCREEN_SCALE, 3};
}
static float backWallDistance(const Vec3 &ray) {
  float farT = 5.0f/3.0f;
  if (fabsf(ray.x) > 0.00001f) farT = fminf(farT, 1.0f/fabsf(ray.x));
  if (fabsf(ray.y) > 0.00001f) farT = fminf(farT, 1.0f/fabsf(ray.y));
  return farT;
}
static void projectPoint(const Vec3 &p, int &x, int &y) {
  float perspective = 3.0f/(p.z+4.0f);
  x = lroundf(SCREEN_SCALE + SCREEN_SCALE*p.x*perspective);
  y = lroundf(SCREEN_SCALE + SCREEN_SCALE*p.y*perspective);
}
static void drawContainerEdges(uint16_t color) {
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
// Contorno da superficie nas paredes, incluindo o contato com a tela.
static void drawWaterOutline() {
  Vec3 points[6];
  float angles[6];
  int count = 0;
  for (int corner = 0; corner < 8; ++corner) {
    Vec3 a = {(corner & 1) ? 1.0f : -1.0f, (corner & 2) ? 1.0f : -1.0f,
              (corner & 4) ? 1.0f : -1.0f};
    for (int axis = 0; axis < 3; ++axis) {
      if (corner & (1 << axis)) continue;
      Vec3 b = a;
      if (axis == 0) b.x = 1;
      if (axis == 1) b.y = 1;
      if (axis == 2) b.z = 1;
      float fa = liquidDepth(a), fb = liquidDepth(b);
      // Convencao semiaberta evita duplicar vertices quando o plano os toca.
      if ((fa >= 0) == (fb >= 0) || count >= 6) continue;
      float t = fa/(fa-fb);
      points[count++] = {a.x+t*(b.x-a.x), a.y+t*(b.y-a.y), a.z+t*(b.z-a.z)};
    }
  }
  if (count < 3) return;
  float cx = 0, cy = 0;
  int px[6], py[6];
  for (int i = 0; i < count; ++i) {
    projectPoint(points[i], px[i], py[i]); cx += px[i]; cy += py[i];
  }
  cx /= count; cy /= count;
  for (int i = 0; i < count; ++i) angles[i] = atan2f(py[i]-cy, px[i]-cx);
  for (int i = 1; i < count; ++i) {
    for (int j = i; j > 0 && angles[j] < angles[j-1]; --j) {
      float a = angles[j]; angles[j] = angles[j-1]; angles[j-1] = a;
      int x = px[j]; px[j] = px[j-1]; px[j-1] = x;
      int y = py[j]; py[j] = py[j-1]; py[j-1] = y;
    }
  }
  for (int i = 0; i < count; ++i) {
    int next = (i+1)%count;
    gfx->drawLine(px[i],py[i],px[next],py[next],rgb565(164,247,255));
  }
}
static void drawGlass() {
  const float offset = dot3(surfaceNormal, CAMERA_EYE)-levelThreshold;
  // 120x120 raios, cada um cobre um bloco de 2x2 pixels da tela inteira.
  for (int y = 0; y < SCREEN_H; y += 2) {
    for (int x = 0; x < SCREEN_W; x += 2) {
      Vec3 ray = screenRay(x+0.5f,y+0.5f);
      float farT = backWallDistance(ray);
      float start = 1, end = farT;
      float slope = dot3(surfaceNormal, ray);
      bool surface = false;
      if (fabsf(slope) < 0.00001f) {
        if (offset < 0) end = start;
      } else {
        float crossing = -offset/slope;
        if (slope > 0) { start = fmaxf(start, crossing); surface = crossing > 1 && crossing < farT; }
        else end = fminf(end, crossing);
      }
      bool rearWall = farT > 1.6666f;
      float baseR = rearWall ? 24 : 38;
      float baseG = rearWall ? 36 : 54;
      float baseB = rearWall ? 43 : 61;
      if (end > start) {
        float thickness = (end-start)*sqrtf(dot3(ray,ray));
        float alpha = thickness/(0.7f+thickness);
        float r = surface ? 26 : 6, g = surface ? 173 : 110, b = surface ? 204 : 163;
        baseR += (r-baseR)*alpha; baseG += (g-baseG)*alpha; baseB += (b-baseB)*alpha;
      }
      gfx->fillRect(x,y,2,2,rgb565((uint8_t)baseR,(uint8_t)baseG,(uint8_t)baseB));
    }
  }
  drawContainerEdges(rgb565(61,100,112));
  drawWaterOutline();
  for (int i = 0; i < BUBBLE_COUNT; ++i) {
    int x, y;
    projectPoint(bubbles[i].position, x, y);
    gfx->drawCircle(x,y,bubbles[i].radius,rgb565(167,236,251));
  }
  // Sem titulo/margens: a imagem inteira funciona como vidro frontal.
  if (!imuReady) {
    gfx->setTextSize(1); gfx->setTextColor(rgb565(255,143,120));
    gfx->setCursor(51,226); gfx->print("SENSOR AUSENTE");
  }
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