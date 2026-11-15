#pragma once
// Original embedded implementation inspired by the supplied ASCII animation references.
// No desktop Python source is bundled or executed.
#include <math.h>
#include <stdint.h>
#include <stddef.h>

namespace ascii_show {
#ifndef ASCII_HIGH_DETAIL
#define ASCII_HIGH_DETAIL 1
#endif
constexpr float PI_F = 3.14159265358979323846f;
constexpr float TAU = 2.0f * PI_F;
constexpr int CELL_W = ASCII_HIGH_DETAIL?3:4, CELL_H = ASCII_HIGH_DETAIL?4:6;
constexpr int COLS = 240/CELL_W, MAX_ROWS = 240/CELL_H;
constexpr int SPLIT_ROWS = 156/CELL_H;
constexpr int MAX_POINTS = 18000;
inline float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float smooth(float v) { v = clamp(v, 0, 1); return v*v*(3-2*v); }
inline uint32_t hash(uint32_t n) { n ^= n >> 16; n *= 0x7feb352dU; n ^= n >> 15; n *= 0x846ca68bU; return n ^ (n >> 16); }
inline float noise(uint32_t n) { return (hash(n) & 0xffffU) / 65535.0f; }
inline uint16_t rgb(int r, int g, int b) { return ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3); }
inline uint16_t shade(uint16_t c, float gain) {
  int r = (c >> 11) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
  return rgb(int(clamp(r*gain,0,255)), int(clamp(g*gain,0,255)), int(clamp(b*gain,0,255)));
}
inline uint16_t highlight(uint16_t c,float spec) {
  int r=(c>>11)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
  spec=clamp(spec,0,1);
  return rgb(r+int((255-r)*spec),g+int((248-g)*spec),b+int((224-b)*spec));
}
inline uint16_t additive(uint16_t a,uint16_t b) {
  int r=(a>>11)+(b>>11),g=((a>>5)&63)+((b>>5)&63),bl=(a&31)+(b&31);
  return (uint16_t(r>31?31:r)<<11)|(uint16_t(g>63?63:g)<<5)|uint16_t(bl>31?31:bl);
}
// Camera-space key light from the upper left; the normal determines the shade.
inline float diffuse(float nx,float ny,float nz) { return clamp(-0.57f*nx+0.58f*ny-0.58f*nz,0,1); }
inline float specular(float nx,float ny,float nz) {
  float h=clamp(-0.30f*nx+0.30f*ny-0.905f*nz,0,1);
  float h2=h*h,h4=h2*h2;return h4*h4*h4;
}
inline char toneGlyph(float light,bool foliage=false) {
  const char *ramp=foliage?".:=+xv%&#W":".:-=+x%&#@";
  return ramp[int(clamp(light,0,0.999f)*10)];
}
inline uint16_t rainbow(float h, float gain = 1) {
  h -= floorf(h); float p = h*6; int i = int(p); float f = p-i;
  float r=0,g=0,b=0;
  switch (i) {
    case 0: r=1; g=f; break; case 1: r=1-f; g=1; break;
    case 2: g=1; b=f; break; case 3: g=1-f; b=1; break;
    case 4: r=f; b=1; break; default: r=1; b=1-f; break;
  }
  return rgb(int((0.12f+0.88f*r)*255*gain),int((0.12f+0.88f*g)*255*gain),int((0.12f+0.88f*b)*255*gain));
}

// Each cell owns its nearest point; backgrounds never replace nearer geometry.
struct Cell { float depth; uint16_t color; char glyph; uint8_t glow; };
struct Point {
  float x,y,z; uint16_t color,born; char glyph; uint8_t kind;
  int8_t nx,ny,nz;uint8_t ao;
};
enum Kind : uint8_t { NEEDLE, WOOD, GOLD, BALL, LIGHT, GIFT, STAR };
class Scene {
 public:
  Cell cells[COLS*MAX_ROWS];
  uint16_t bloom[COLS*MAX_ROWS]{};
  bool hasGlow=false;
  int rows = MAX_ROWS;
  bool tree;
  Point *points;
  int count = 0;
  bool overflow = false;
  float angle = 0, spinC = 1, spinS = 0;
  explicit Scene(bool isTree, Point *storage) : tree(isTree), points(storage) {}
  void put(int x, int y, float z, char ch, uint16_t color,uint8_t emission=0) {
    if (x<0 || x>=COLS || y<0 || y>=rows || !isfinite(z)) return;
    Cell &c = cells[y*COLS+x];
    if (z < c.depth) c = {z,color,ch,emission};
  }
  void project(float x, float y, float z, char ch, uint16_t color, bool rotate = true,uint8_t emission=0) {
    if (rotate) { float xx=x*spinC-z*spinS; z=x*spinS+z*spinC; x=xx; }
    // Gentle camera tilt exposes the ground and the tops of the presents.
    float yy=y*0.985f + z*0.174f;
    float zz=z*0.985f - y*0.174f;
    float distance=4.4f+zz;
    if (distance <= 0.2f) return;
    float f=4.4f/distance;
    float scale = (rows*CELL_H)*0.37f;
    float sx=120+x*f*scale, sy=rows*CELL_H*0.48f-yy*f*scale;
    if (!isfinite(sx) || !isfinite(sy)) return;
    // floor rather than truncation keeps offscreen coordinates outside the grid.
    put(int(floorf(sx/CELL_W)),int(floorf(sy/CELL_H)),zz,ch,color,emission);
  }
  void buildGlow() {
    hasGlow=false;
    for(int i=0;i<COLS*MAX_ROWS;++i) bloom[i]=0;
    for(int y=0;y<rows;++y) for(int x=0;x<COLS;++x) {
      const Cell &c=cells[y*COLS+x];if(!c.glow) continue;
      hasGlow=true;
      for(int dy=-2;dy<=2;++dy) for(int dx=-2;dx<=2;++dx) {
        int xx=x+dx,yy=y+dy;if(xx<0 || xx>=COLS || yy<0 || yy>=rows) continue;
        int wx=3-(dx<0?-dx:dx),wy=3-(dy<0?-dy:dy);
        uint16_t col=shade(c.color,0.11f*(c.glow/255.0f)*wx*wy/9);
        bloom[yy*COLS+xx]=additive(bloom[yy*COLS+xx],col);
      }
    }
  }
  uint16_t glowAt(int x,int y) const {
    if(!hasGlow) return 0;
    int gx=x/CELL_W,gy=y/CELL_H,fx=x%CELL_W,fy=y%CELL_H;
    int gx1=gx+1<COLS?gx+1:gx,gy1=gy+1<rows?gy+1:gy;
    uint16_t colors[]={bloom[gy*COLS+gx],bloom[gy*COLS+gx1],bloom[gy1*COLS+gx],bloom[gy1*COLS+gx1]};
    int weights[]={(CELL_W-fx)*(CELL_H-fy),fx*(CELL_H-fy),(CELL_W-fx)*fy,fx*fy};
    int r=0,g=0,b=0;
    for(int k=0;k<4;++k) {r+=(colors[k]>>11)*weights[k];g+=((colors[k]>>5)&63)*weights[k];b+=(colors[k]&31)*weights[k];}
    int area=CELL_W*CELL_H;
    return (uint16_t(r/area)<<11)|(uint16_t(g/area)<<5)|uint16_t(b/area);
  }
  void add(float x,float y,float z,uint16_t color,float born,char glyph,uint8_t kind,
           float nx=NAN,float ny=0.45f,float nz=0,float ao=1) {
    if (count >= MAX_POINTS || points == nullptr) { overflow=true; return; }
    if(!isfinite(nx)) { float r=sqrtf(x*x+z*z);nx=r>0?x/r:0;nz=r>0?z/r:-1; }
    float inv=1.0f/sqrtf(nx*nx+ny*ny+nz*nz);
    points[count++]={x,y,z,color,uint16_t(born*1000),glyph,kind,
      int8_t(nx*inv*127),int8_t(ny*inv*127),int8_t(nz*inv*127),uint8_t(clamp(ao,0,1)*255)};
  }
  static float radius(float y) { return 0.78f*powf(clamp((0.88f-y)/1.65f,0,1),0.95f); }
  void buildTree() {
    count=0; overflow=false;
    const char *ramp="xv%&#W";
    // Whorls of sagging branches, with a dark core for a solid silhouette.
    for (int layer=0; layer<21; ++layer) {
      float y=-0.77f+layer*0.078f, r=radius(y);
      int branches=7+(20-layer)/2;
      for (int branch=0; branch<branches; ++branch) {
        float a=TAU*branch/branches+layer*0.83f;
        for (int j=0;j<30;++j) {
          uint32_t seed=uint32_t(layer*10000+branch*100+j);
          float u=(j+1)/30.0f, rr=r*u;
          float spread=(noise(seed)-0.5f)*(0.05f+0.17f*u);
          float x=cosf(a)*rr-sinf(a)*spread, z=sinf(a)*rr+cosf(a)*spread;
          float yy=y+0.04f*u-0.13f*u*u;
          uint16_t col=rgb(24+int(noise(seed+1)*24),158+int(noise(seed+2)*65),65);
          if (noise(seed+3)>0.965f && u>0.55f) col=rgb(170,196,206);
          float normalA=a+(noise(seed+4)-0.5f)*0.65f;
          add(x,yy,z,col,0.5f+4.5f*(y+0.85f)/1.75f,ramp[hash(seed)%6],NEEDLE,
              cosf(normalA),0.3f+0.45f*noise(seed+5),sinf(normalA),0.35f+0.65f*u);
        }
      }
      for (int band=0;band<3;++band) for (int j=0;j<56;++j) {
        float a=TAU*j/56, rr=r*0.68f;
        add(cosf(a)*rr,y-band*0.026f,sinf(a)*rr,rgb(18,130,43),
            0.5f+4.5f*(y+0.85f)/1.75f,'v',NEEDLE,cosf(a),0.35f,sinf(a),0.40f);
      }
    }
    for (int j=0;j<120;++j) {
      float a=j*2.39996f, y=-0.98f+(j/120.0f)*0.26f;
      add(cosf(a)*0.055f,y,sinf(a)*0.055f,rgb(124,75,36),0.2f,'#',WOOD);
    }
    // Three-dimensional garland wrapped around the foliage.
    for (int j=0;j<650;++j) {
      float u=j/649.0f, y=-0.72f+1.47f*u, a=u*TAU*3.5f;
      float r=radius(y)+0.045f;
      add(cosf(a)*r,y-0.035f*fabsf(sinf(a*2)),sinf(a)*r,rgb(255,199,55),5.0f+u*2,'#',GOLD);
    }
    for (int k=0;k<24;++k) {
      float y=-0.67f+(k/24.0f)*1.31f, a=k*2.39996f;
      float cx=cosf(a)*(radius(y)+0.02f), cz=sinf(a)*(radius(y)+0.02f);
      for(int j=0;j<42;++j) {
        float ny=1-2*(j+0.5f)/42, rr=sqrtf(1-ny*ny), phi=j*2.39996f;
        float rad=0.055f+(1-(y+0.67f)/1.31f)*0.02f;
        add(cx+cosf(phi)*rr*rad,y+ny*rad,cz+sinf(phi)*rr*rad,
            k%3 ? rgb(237,46,52) : rgb(255,209,74),7.0f+k*0.075f,'@',BALL,
            cosf(phi)*rr,ny,sinf(phi)*rr);
      }
    }
    for (int k=0;k<70;++k) {
      float u=(k+0.5f)/70, y=-0.76f+u*1.57f, a=k*2.39996f, r=radius(y)+0.06f;
      add(cosf(a)*r,y,sinf(a)*r,k%4 ? rgb(255,225,150) : rgb(90,156,255),8.5f+u,'*',LIGHT);
    }
    const uint16_t colors[]={rgb(230,43,50),rgb(43,91,240),rgb(39,183,86),rgb(233,194,67)};
    for (int k=0;k<5;++k) {
      float a=k*TAU/5+0.25f, cx=cosf(a)*0.79f, cz=sinf(a)*0.79f;
      float w=0.20f+(k%2)*0.06f, h=0.22f+(k%3)*0.03f;
      for (int face=0;face<5;++face) for(int i=0;i<8;++i) for(int j=0;j<8;++j) {
        float u=(i/7.0f-0.5f)*w, v=(j/7.0f-0.5f)*w;
        float x=u,y=-0.98f+h,z=v;
        if (face<4) { y=-0.98f+(j/7.0f)*h; if(face<2) z=(face?1:-1)*w/2; else {x=(face==2?1:-1)*w/2;z=u;} }
        bool ribbon=(i==3 || i==4 || (face==4 && (j==3 || j==4)));
        float nx=face==2?1:(face==3?-1:0),ny=face==4?1:0,nz=face==0?-1:(face==1?1:0);
        add(cx+x,y,cz+z,ribbon?rgb(255,220,120):colors[k%4],10+k*0.2f,ribbon?'+':'#',GIFT,nx,ny,nz);
      }
    }
    // A filled five-point star, also made of text cells after projection.
    float starX[10],starY[10];
    for(int k=0;k<10;++k) {float a=k*TAU/10,r=k%2?0.058f:0.145f;starX[k]=sinf(a)*r;starY[k]=cosf(a)*r;}
    for (int iy=-15;iy<=15;++iy) for(int ix=-15;ix<=15;++ix) {
      float x=ix*0.01f,y=iy*0.01f;bool inside=false;
      for(int i=0,j=9;i<10;j=i++) {
        if((starY[i]>y)!=(starY[j]>y) && x<(starX[j]-starX[i])*(y-starY[i])/(starY[j]-starY[i])+starX[i]) inside=!inside;
      }
      if(inside) add(x,1.025f+y,0,rgb(255,218,78),11.5f,'#',STAR);
    }
  }
  void background(float t) {
    for (int i=0;i<90;++i) {
      float x=noise(i*7+8)*COLS;
      float speed=tree?0.6f:-0.3f;
      float y=fmodf(noise(i*7+9)*rows+speed*t,float(rows));
      if(y<0) y+=rows;
      float shine=0.35f+0.4f*powf(0.5f+0.5f*sinf(t*1.3f+i),6);
      put(int(x),int(y),100,i%9==0?'+':'.',shade(rgb(150,190,220),shine));
    }
    if(tree) for(int iz=0;iz<13;++iz) for(int ix=-14;ix<=14;++ix) {
      project(ix*0.12f,-1.01f,(iz-6)*0.16f,'.',rgb(30,47,61),false);
    }
  }
  void drawTree(float t) {
    float cycle=fmodf(t,40.0f);
    for(int i=0;i<count;++i) {
      const Point &p=points[i];
      float age=cycle-p.born/1000.0f;
      if(age<0) continue;
      float fade=smooth(age*2.8f);
      float nx=(p.nx*spinC-p.nz*spinS)/127.0f;
      float nz=(p.nx*spinS+p.nz*spinC)/127.0f,ny=p.ny/127.0f;
      float lambert=diffuse(nx,ny,nz),ao=p.ao/255.0f;
      float light=(0.17f+0.95f*lambert)*ao;
      // Inner foliage stays dark; upward tips catch the key light. Silvery back rim
      // separates the far edge from the black background without washing it out.
      float rim=powf(clamp(1-fabsf(nz),0,1),3)*0.10f;
      light+=rim*ao;
      float spec=0;
      if(p.kind==BALL) spec=specular(nx,ny,nz)*0.88f;
      if(p.kind==GOLD) { light=0.26f+0.83f*lambert;spec=specular(nx,ny,nz)*0.6f; }
      if(p.kind==STAR) light=1;
      if(p.kind==LIGHT) light=0.55f+0.45f*powf(0.5f+0.5f*sinf(t*3+i),2);
      uint16_t col=highlight(shade(p.color,light),spec);
      char glyph=p.glyph;
      if(p.kind==NEEDLE || p.kind==WOOD) glyph=toneGlyph(light,true);
      else if(p.kind==BALL) glyph=toneGlyph(light+spec);
      else if(p.kind==GOLD) glyph=light>0.75f?'#':(light>0.45f?'x':'+');
      col=shade(col,fade);
      uint8_t emission=(p.kind==STAR || p.kind==LIGHT)?uint8_t(210*fade):0;
      if(p.kind==GOLD && spec>0.3f) emission=40;
      if(p.kind==STAR) project(p.x,p.y,p.z,glyph,col,false,emission);
      else project(p.x,p.y,p.z,glyph,col,true,emission);
    }
  }
  void jelly(float t, float cx,float cy,float cz,float size, bool small=false) {
    float pulse=1+0.09f*sinf(t*2.3f), yaw=0.45f*sinf(t*0.44f);
    float co=cosf(yaw),si=sinf(yaw);
    auto dot=[&](float x,float y,float z,char ch,uint16_t col,uint8_t emission=0) {
      project(cx+size*(x*co-z*si),cy+size*y,cz+size*(x*si+z*co),ch,col,false,emission);
    };
    // Hemispherical bell: continuous ASCII shading instead of a sprite.
    int rings=small?8:23, slices=small?20:74;
    for(int j=0;j<rings;++j) {
      float v=(j+0.2f)/(rings-0.8f), a=v*PI_F*0.5f;
      float r=0.56f*sinf(a)*pulse, y=0.52f+0.45f*cosf(a)/pulse;
      for(int k=0;k<slices;++k) {
        float phi=k*TAU/slices, x=r*cosf(phi), z=r*sinf(phi);
        float nx=sinf(a)*cosf(phi+yaw),ny=cosf(a),nz=sinf(a)*sinf(phi+yaw);
        float lambert=diffuse(nx,ny,nz);
        float rim=powf(clamp(1-fabsf(nz),0,1),3);
        float lit=0.16f+0.74f*lambert+0.16f*rim;
        float spec=specular(nx,ny,nz)*0.75f;
        char ch=j==rings-1?'_':toneGlyph(lit+spec);
        float hue=0.54f+0.12f*sinf(t*0.23f)+v*0.13f+0.04f*cosf(phi);
        uint16_t col=highlight(rainbow(hue,clamp(lit,0,1)),spec);
        dot(x,y,z,ch,col,uint8_t(clamp(spec+0.15f*rim,0,1)*80));
      }
    }
    // Four broad oral arms, and fourteen finer tentacles flowing independently.
    for(int k=0;k<(small?5:14);++k) {
      float a=k*TAU/(small?5:14), rr=0.48f*pulse;
      int steps=small?25:72;
      for(int j=0;j<steps;++j) {
        float u=j/float(steps-1), wave=t*1.8f-u*6.5f+k*0.73f;
        float x=rr*cosf(a)+0.20f*u*sinf(wave)+0.10f*u*sinf(t*0.7f);
        float z=rr*sinf(a)+0.14f*u*cosf(wave*0.8f);
        float y=0.51f-u*(1.65f+0.16f*sinf(k*2.7f));
        char ch=sinf(wave)>0.38f?'/':(sinf(wave)<-0.38f?'\\':'|');
        float depthShade=0.2f+0.65f*clamp((0.52f-z)/1.04f,0,1);
        dot(x,y,z,ch,rainbow(0.52f+k*0.047f+t*0.025f,depthShade*(1-0.35f*u)));
      }
    }
    if(!small) for(int k=0;k<4;++k) for(int j=0;j<68;++j) {
      float u=j/67.0f, a=k*TAU/4, wave=t*1.6f-u*7+k;
      float x=cosf(a)*0.16f+0.16f*u*sinf(wave), z=sinf(a)*0.16f+0.09f*u*cosf(wave);
      for(int edge=-1;edge<=1;++edge) {
        float width=(1-u)*0.065f;
        char ch=edge==0?"x%&#@"[(j+k)%5]:(edge<0?'/':'\\');
        float light=(edge==0?0.95f:0.38f)*(0.75f+0.25f*cosf(wave));
        dot(x+edge*width,0.53f-u*1.58f,z,ch,rainbow(0.08f+k*0.14f+u*0.16f+t*0.03f,light));
      }
    }
  }
  void render(float t, bool split) {
    rows=split?SPLIT_ROWS:MAX_ROWS;
    for(int i=0;i<COLS*MAX_ROWS;++i) cells[i]={10000,0,' ',0};
    angle=t*TAU/20; spinC=cosf(angle);spinS=sinf(angle);
    background(t);
    if(tree) drawTree(t);
    else {
      jelly(t,0,0.02f+0.055f*sinf(t*1.2f),0,1);
      jelly(t+3,-1.22f,0.62f+0.12f*sinf(t*0.5f),1.4f,0.23f,true);
      jelly(t+7,1.25f,-0.52f+0.14f*sinf(t*0.6f),1.5f,0.25f,true);
    }
    buildGlow();
  }
};

// Crisp 3x5 alternative. The default adapter uses supersampled 5x7 glyphs instead.
inline uint16_t glyphBits(char c) {
  #define G(a,b,c,d,e) ((a)<<12 | (b)<<9 | (c)<<6 | (d)<<3 | (e))
  switch(c) {
    case '.': return G(0,0,0,0,2); case ':':return G(0,2,0,2,0);
    case '-':return G(0,0,7,0,0);case '_':return G(0,0,0,0,7);
    case '=':return G(0,7,0,7,0);case '+':return G(0,2,7,2,0);
    case '*':return G(5,2,7,2,5);case 'x':return G(0,5,2,5,0);
    case 'v':return G(0,5,5,5,2);case '%':return G(5,1,2,4,5);
    case '&':return G(2,5,2,5,3);case '#':return G(5,7,5,7,5);
    case '@':return G(7,5,7,4,7);case 'W':return G(5,5,7,7,5);
    case 'o':return G(0,0,7,5,7);case 'O':return G(7,5,5,5,7);
    case '/':return G(1,1,2,4,4);case '\\':return G(4,4,2,1,1);
    case '|':return G(2,2,2,2,2);case '~':return G(0,3,6,0,0);
    default:return 0;
  }
  #undef G
}
} // namespace ascii_show
