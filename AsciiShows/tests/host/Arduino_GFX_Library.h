#pragma once
#include "Arduino.h"
#include <algorithm>
#include <array>
#include <font/glcdfont.h> // Supplied by Arduino_GFX; no substitute text font.
constexpr int GFX_NOT_DEFINED=-1;
struct Arduino_DataBus { virtual ~Arduino_DataBus()=default; };
struct Arduino_ESP32SPI : Arduino_DataBus { Arduino_ESP32SPI(int,int,int,int,int) {} };
struct Arduino_GFX {
  std::array<uint16_t,240*240> pixels{};
  int cursorX=0,cursorY=0;uint16_t textColor=0xffff;
  virtual ~Arduino_GFX()=default;
  bool begin(int=0) { return true; }
  void fillScreen(uint16_t color) { pixels.fill(color); }
  void drawPixel(int x,int y,uint16_t color) { if(x>=0 && x<240 && y>=0 && y<240) pixels[y*240+x]=color; }
  void fillRect(int x,int y,int w,int h,uint16_t color) { for(int j=y;j<y+h;++j) for(int i=x;i<x+w;++i) drawPixel(i,j,color); }
  void drawFastHLine(int x,int y,int w,uint16_t color) { fillRect(x,y,w,1,color); }
  void setTextSize(int) {}
  void setTextWrap(bool) {}
  void setTextColor(uint16_t color) { textColor=color; }
  void setCursor(int x,int y) { cursorX=x;cursorY=y; }
  void write(uint8_t c) {
    for(int x=0;x<5;++x) for(int y=0;y<8;++y)
      if(font[int(c)*5+x] & (1<<y)) drawPixel(cursorX+x,cursorY+y,textColor);
    cursorX+=6;
  }
  void print(const char *s) { while(*s) write(uint8_t(*s++)); }
  void flush() {}
};
struct Arduino_ST7789 : Arduino_GFX { Arduino_ST7789(Arduino_DataBus*,int,int,bool,int,int,int,int,int,int) {} };
struct Arduino_Canvas : Arduino_GFX { Arduino_Canvas(int,int,Arduino_GFX*,int,int,uint8_t) {} };
