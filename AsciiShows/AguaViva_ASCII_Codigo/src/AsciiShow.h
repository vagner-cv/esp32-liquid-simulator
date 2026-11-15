#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>
#include <font/glcdfont.h>
#include "Scene.h"
#include "CodeLines.h"

namespace ascii_show {
// Same LCD pins, panel offsets and canvas rotation as LiquidCube_ESP32S3.
constexpr int LCD_DC=38, LCD_CS=39, LCD_SCK=40, LCD_MOSI=41, LCD_RST=42, LCD_BL=20;
constexpr uint8_t DISPLAY_ROTATION=1;
constexpr uint32_t FRAME_MS=40; // Target ceiling: 25 FPS, not a measured device result.
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC,LCD_CS,LCD_SCK,LCD_MOSI,GFX_NOT_DEFINED);
Arduino_GFX *lcd = new Arduino_ST7789(bus,LCD_RST,0,true,240,240,0,0,0,0);
Arduino_Canvas *canvas = new Arduino_Canvas(240,240,lcd,0,0,DISPLAY_ROTATION);
static Point *storage = nullptr;
static Scene *scene = nullptr;
static bool ready=false, paused=false;
static uint32_t lastTick=0,lastDraw=0,lastReport=0,frames=0,clockMs=0;
static uint8_t coverage[128][12];

inline void prepareGlyphs() {
  // An 80x60 character grid on a virtual 480x480 surface, filtered down to the
  // real 240x240 LCD. Store only coverage, not a second large framebuffer.
  for(int ch=0;ch<128;++ch) for(int y=0;y<4;++y) for(int x=0;x<3;++x) {
    uint8_t n=0;
    for(int dy=0;dy<2;++dy) for(int dx=0;dx<2;++dx) {
      int sx=2*x+dx, sy=2*y+dy;
      if(sx<5 && (font[ch*5+sx] & (1<<sy))) ++n;
    }
    coverage[ch][y*3+x]=n;
  }
}

inline void miniGlyph(int x,int y,char ch,uint16_t col) {
  if(ASCII_HIGH_DETAIL) {
    // Gamma-aware coverage keeps fine single strokes visible on the small LCD.
    uint16_t levels[]={0,shade(col,0.5f),shade(col,0.7071f),shade(col,0.8660f),col};
    for(int row=0;row<4;++row) for(int column=0;column<3;++column) {
      uint8_t alpha=coverage[uint8_t(ch)&127][row*3+column];
      if(alpha) canvas->drawPixel(x+column,y+row,additive(levels[alpha],scene->glowAt(x+column,y+row)));
    }
    return;
  }
  uint16_t bits=glyphBits(ch);
  for(int row=0;row<5;++row) for(int column=0;column<3;++column)
    if(bits & (uint16_t(1) << (14-row*3-column))) canvas->drawPixel(x+column,y+row,additive(col,scene->glowAt(x+column,y+row)));
}
inline bool letter(char c) { return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_'; }
inline bool digit(char c) { return c>='0' && c<='9'; }
inline bool keyword(const char *p,size_t n) {
  static const char *words[]={"for","if","else","float","int","void","return","bool","const","char","uint16_t","uint32_t"};
  for(auto w:words) if(strlen(w)==n && strncmp(p,w,n)==0) return true;
  return false;
}
inline void codeLine(const char *line, int y) {
  // Draw at most 40 columns. Short clipped edge rows allow smooth vertical scroll.
  uint16_t base=rgb(183,192,209),col=base,tokenColor=base;
  bool comment=false,string=false; size_t tokenEnd=0;
  for(size_t i=0;line[i] && i<40;++i) {
    char c=line[i];
    if(c=='/' && line[i+1]=='/' && !string) comment=true;
    if(c=='"' && (i==0 || line[i-1]!='\\') && !comment) string=!string;
    if(comment) col=rgb(97,120,137);
    else if(string || c=='"') col=rgb(217,182,129);
    else if(i<tokenEnd) col=tokenColor;
    else if(letter(c)) {
      size_t end=i+1; while(letter(line[end]) || digit(line[end])) ++end;
      tokenEnd=end;
      tokenColor=keyword(line+i,end-i)?rgb(232,126,172):(line[end]=='('?rgb(116,192,237):base);
      col=tokenColor;
    } else if(digit(c)) col=rgb(235,191,126);
    else col=base;
    canvas->setTextColor(col);canvas->setCursor(i*6,y);canvas->write(uint8_t(c));
  }
}
inline void codePanel(float time) {
  constexpr int TOP=156, HEADER=12, BODY=TOP+HEADER, LINE_H=9;
  canvas->fillRect(0,TOP,240,84,rgb(6,9,17));
  canvas->fillRect(0,TOP,240,HEADER,rgb(20,27,42));
  canvas->drawFastHLine(0,TOP,240,rgb(64,79,102));
  canvas->setTextColor(rgb(157,186,211));canvas->setCursor(4,TOP+3);
  canvas->print(ASCII_TREE?"christmas_tree.cpp":"jellyfish.cpp");
  canvas->setTextColor(rgb(85,224,191));canvas->setCursor(192,TOP+3);
  canvas->print(paused?"PAUSADO":"CODIGO");
  const char *const *lines=ASCII_TREE?TREE_CODE:JELLY_CODE;
  int length=ASCII_TREE?TREE_CODE_COUNT:JELLY_CODE_COUNT;
  uint32_t scroll=uint32_t(time*12);
  int first=(scroll/LINE_H)%length, offset=scroll%LINE_H;
  canvas->fillRect(0,BODY+3*LINE_H,240,LINE_H,rgb(19,31,48));
  for(int row=0;row<10;++row) {
    int y=BODY+row*LINE_H-offset;
    // Avoid code glyphs overwriting the header while an edge line scrolls out.
    if(y>=BODY && y+7<240) codeLine(lines[(first+row)%length],y);
  }
}
inline void renderFrame(float time) {
  scene->render(time,ASCII_CODE);
  canvas->fillScreen(0);
  if(scene->hasGlow) for(int y=0;y<scene->rows*CELL_H;++y) for(int x=0;x<240;++x) {
    uint16_t bg=scene->glowAt(x,y);if(bg) canvas->drawPixel(x,y,bg);
  }
  for(int y=0;y<scene->rows;++y) for(int x=0;x<COLS;++x) {
    const Cell &c=scene->cells[y*COLS+x];
    if(c.glyph!=' ') miniGlyph(x*CELL_W,y*CELL_H,c.glyph,c.color);
  }
  if(ASCII_TREE && fmodf(time,40)>13) {
    canvas->setCursor(87,scene->rows*CELL_H-8);canvas->setTextColor(rgb(222,187,91));
    canvas->print("FELIZ NATAL");
  }
  if(ASCII_CODE) codePanel(time);
  canvas->flush();
}
inline void begin() {
  Serial.begin(115200);
  pinMode(LCD_BL,OUTPUT);digitalWrite(LCD_BL,LOW);
  // Canvas begin allocates the framebuffer and initializes the real ST7789.
  if(!canvas->begin(40000000)) { Serial.println("ERRO: falha ao iniciar display/canvas"); return; }
  canvas->fillScreen(0);canvas->flush();digitalWrite(LCD_BL,HIGH);
  canvas->setTextSize(1);canvas->setTextWrap(false);prepareGlyphs();
  if(ASCII_TREE) {
    size_t bytes=sizeof(Point)*MAX_POINTS;
    storage=static_cast<Point *>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!storage) storage=static_cast<Point *>(heap_caps_malloc(bytes,MALLOC_CAP_8BIT));
    if(!storage) {
      Serial.println("ERRO: sem memoria para arvore. Ative OPI PSRAM.");
      canvas->setTextColor(rgb(255,90,90));canvas->setCursor(6,100);
      canvas->print("Ative OPI PSRAM");canvas->flush();return;
    }
  }
  static Scene instance(ASCII_TREE,storage);scene=&instance;
  if(ASCII_TREE) scene->buildTree();
  if(scene->overflow) { Serial.println("ERRO: capacidade de pontos excedida"); return; }
  Serial.printf("ASCII %s / %s | pontos=%d | grade=%dx%d\n",ASCII_TREE?"arvore":"agua-viva",ASCII_CODE?"com codigo":"tela cheia",scene->count,COLS,ASCII_CODE?SPLIT_ROWS:MAX_ROWS);
  Serial.println("Serial: p=pausar/retomar, r=reiniciar. 115200 baud.");
  ready=true;lastTick=millis();lastReport=lastTick;
  renderFrame(0);
}
inline void tick() {
  if(!ready) { delay(20); return; }
  uint32_t now=millis(),dt=now-lastTick;lastTick=now;
  if(!paused) clockMs+=dt;
  while(Serial.available()) {
    char c=Serial.read();
    if(c=='p' || c=='P') paused=!paused;
    if(c=='r' || c=='R') clockMs=0;
  }
  if(uint32_t(now-lastDraw)<FRAME_MS) { delay(1); return; }
  lastDraw=now;
  renderFrame(clockMs/1000.0f);++frames;
  uint32_t elapsed=now-lastReport;
  if(elapsed>=5000) {
    Serial.printf("FPS medido: %.1f | PSRAM livre: %u bytes\n",frames*1000.0f/elapsed,
                  unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    frames=0;lastReport=now;
  }
}
} // namespace ascii_show
