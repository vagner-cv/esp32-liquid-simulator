// Compile four times with -DASCII_TREE=0/1 -DASCII_CODE=0/1.
#include "../engine/AsciiShow.h"
#include <cassert>
#include <filesystem>
#include <string>
using namespace ascii_show;
static void ppm(const std::string &path) {
  FILE *f=fopen(path.c_str(),"wb");assert(f);
  fprintf(f,"P6\n240 240\n255\n");
  for(auto c:canvas->pixels) {
    unsigned char rgb8[]={static_cast<unsigned char>((c>>11)*255/31),
      static_cast<unsigned char>(((c>>5)&63)*255/63),static_cast<unsigned char>((c&31)*255/31)};
    fwrite(rgb8,1,3,f);
  }
  fclose(f);
}
int main(int argc,char **argv) {
  assert(argc==2 || argc==3);
  if(argc==3) {
    hostAllocationFails=true;begin();
    assert(ASCII_TREE && !ready && storage==nullptr);
    puts("PASS: memory allocation failure is reported without starting the renderer.");return 0;
  }
  std::filesystem::create_directories(argv[1]);
  begin();assert(ready);
  // Inspect timeline controls through the same Serial path as the device.
  hostMillis=1000;tick();assert(clockMs==1000);
  Serial.input="p";hostMillis=1040;tick();assert(paused);
  hostMillis=2040;tick();assert(clockMs==1040);
  Serial.input="r";hostMillis=2080;tick();assert(clockMs==0);
  Serial.input="p";hostMillis=2120;tick();assert(!paused);
  hostMillis=2160;tick();assert(clockMs==40);
  // Verify 32-bit millis rollover preserves the animation step.
  lastTick=UINT32_MAX-10;lastDraw=lastTick;hostMillis=20;
  uint32_t before=clockMs;tick();assert(clockMs==before+31);
  for(int i=0;i<48;++i) {
    renderFrame(14+i*0.125f);
    char file[32];snprintf(file,sizeof(file),"/frame-%03d.ppm",i);
    ppm(std::string(argv[1])+file);
  }
  renderFrame(0);ppm(std::string(argv[1])+"/growth-0.ppm");
  renderFrame(3);ppm(std::string(argv[1])+"/growth-3.ppm");
  renderFrame(8);ppm(std::string(argv[1])+"/growth-8.ppm");
  printf("PASS: exact renderer preview and Serial pause/restart/timer wrap (%d/%d).\n",ASCII_TREE,ASCII_CODE);
}
