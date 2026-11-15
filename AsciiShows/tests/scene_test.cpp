#include "../engine/Scene.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <set>
using namespace ascii_show;
static uint64_t checksum(const Scene &s) {
  uint64_t v=1469598103934665603ULL;
  for(int i=0;i<COLS*s.rows;++i) { v^=s.cells[i].color;v*=1099511628211ULL;v^=s.cells[i].glyph; }
  return v;
}
static int occupied(const Scene &s) {
  int n=0;for(int i=0;i<COLS*s.rows;++i) if(s.cells[i].depth<50 && s.cells[i].glyph!=' ') ++n;return n;
}
int main() {
  std::unique_ptr<Point[]> storage(new Point[MAX_POINTS]);
  Scene tree(true,storage.get());tree.buildTree();
  assert(!tree.overflow && tree.count>6000 && tree.count<=MAX_POINTS);
  std::set<int> kinds;
  for(int i=0;i<tree.count;++i) { const Point &p=tree.points[i];
    assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
    assert(p.born<=12000);assert(glyphBits(p.glyph)!=0);kinds.insert(p.kind);
    float normalLength=sqrtf(float(p.nx*p.nx+p.ny*p.ny+p.nz*p.nz))/127;
    assert(normalLength>0.97f && normalLength<1.02f);
  }
  assert(kinds.size()==7);
  tree.render(0,false);int start=occupied(tree);
  tree.render(15,false);assert(occupied(tree)>start+250);
  assert(diffuse(-1,0,0)>diffuse(1,0,0)+0.5f);
  std::set<uint16_t> greens;int greenMin=255,greenMax=0;
  for(int i=0;i<COLS*tree.rows;++i) {
    uint16_t c=tree.cells[i].color;
    int r=(c>>11)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
    if(g>r*1.5f && g>b*1.5f && tree.cells[i].depth<50) {
      greens.insert(c);if(g<greenMin)greenMin=g;if(g>greenMax)greenMax=g;
    }
  }
  assert(greens.size()>12 && greenMax>100 && greenMin*4<greenMax);
  assert(tree.hasGlow);
  for(int y=0;y<240;++y) for(int x=0;x<240;++x) (void)tree.glowAt(x,y);
  auto full=checksum(tree);tree.render(18,false);assert(checksum(tree)!=full);
  tree.render(40,false);assert(occupied(tree)==start);
  Scene jelly(false,nullptr);
  for(bool split:{false,true}) {
    for(Scene *s:{&tree,&jelly}) {
      for(int frame=0;frame<320;++frame) {
        s->render(frame*0.17f,split);
        assert(s->rows==(split?SPLIT_ROWS:MAX_ROWS));
        for(int i=0;i<COLS*MAX_ROWS;++i) {
          assert(std::isfinite(s->cells[i].depth));
          if(i>=COLS*s->rows) assert(s->cells[i].glyph==' ');
        }
      }
      s->render(15,split);assert(occupied(*s)>100);
    }
    jelly.render(1,split);auto a=checksum(jelly);
    jelly.render(2,split);assert(checksum(jelly)!=a);
    jelly.render(3000000.0f,split);int backgroundCells=0;
    for(int i=0;i<COLS*jelly.rows;++i) if(jelly.cells[i].depth==100) ++backgroundCells;
    assert(backgroundCells>20); // Upward particles keep wrapping after weeks of uptime.
  }
  // Depth, clipping and invalid projections cannot corrupt neighbouring cells.
  jelly.render(0,false);jelly.put(10,10,-1,'@',rgb(255,0,0));
  jelly.put(10,10,2,'x',0);assert(jelly.cells[10*COLS+10].glyph=='@');
  auto original=checksum(jelly);
  jelly.put(-1,0,0,'#',1);jelly.put(COLS,0,0,'#',1);jelly.put(0,MAX_ROWS,0,'#',1);
  jelly.project(0,0,-100,'#',1,false);assert(checksum(jelly)==original);
  Scene empty(true,nullptr);empty.buildTree();assert(empty.overflow && empty.count==0);
  std::printf("PASS: %d tree points (%zu bytes), 1280 animated frames, both layouts, depth/clipping/restart.\n",tree.count,sizeof(Point)*MAX_POINTS);
}
