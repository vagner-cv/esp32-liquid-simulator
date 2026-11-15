// Waveshare ESP32-S3-LCD-1.3 | ST7789 240x240
// Requer GFX Library for Arduino (Moon On Our Nation).
// Gerado por tools/generate.py; fonte original em ../engine/.
// Serial: p pausa/retoma, r reinicia; 115200 baud.
#define ASCII_TREE 1
#define ASCII_CODE 1
#define ASCII_HIGH_DETAIL 1 // 80x60 caracteres suavizados; 0 = 60x40 nitidos.
#include "src/AsciiShow.h"
void setup() { ascii_show::begin(); }
void loop() { ascii_show::tick(); }
