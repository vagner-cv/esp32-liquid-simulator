#!/usr/bin/env python3
"""Render firmware on desktop with a framebuffer adapter. Requires g++, Pillow and Arduino_GFX."""
from pathlib import Path
import argparse
import subprocess
import tempfile
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument("--gfx-font-dir",type=Path,required=True,help="Arduino_GFX/src/font containing glcdfont.h")
args=parser.parse_args()
MODES=[("Arvore_ASCII",1,0),("Arvore_ASCII_Codigo",1,1),
       ("AguaViva_ASCII",0,0),("AguaViva_ASCII_Codigo",0,1)]
out=ROOT/"previews"
out.mkdir(exist_ok=True)
tiles=[]
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    for name,tree,code in MODES:
        exe=tmp/name
        subprocess.run(["g++","-std=c++17","-O2","-Wall","-Wextra","-Werror",
                        f"-DASCII_TREE={tree}",f"-DASCII_CODE={code}",
                        "-I"+str(ROOT/"tests/host"),"-I"+str(args.gfx_font_dir.parent),
                        str(ROOT/"tests/preview.cpp"),"-o",str(exe)],check=True)
        frames_dir=tmp/(name+"-frames")
        subprocess.run([str(exe),str(frames_dir)],check=True)
        if tree:
            subprocess.run([str(exe),str(frames_dir),"fail-allocation"],check=True)
        frames=[Image.open(p).convert("RGB") for p in sorted(frames_dir.glob("frame-*.ppm"))]
        frames[0].save(out/(name+".png"))
        frames[0].save(out/(name+".gif"),save_all=True,append_images=frames[1:],duration=125,loop=0)
        tiles.append((name,frames[0]))
    board=Image.new("RGB",(512,586),(10,16,25))
    draw=ImageDraw.Draw(board)
    for i,(name,im) in enumerate(tiles):
        x=8+(i%2)*256;y=30+(i//2)*276
        draw.text((x,y-18),name,fill=(215,224,235))
        board.paste(im,(x,y))
    draw.text((8,570),"240 x 240 | Simulacao do firmware no computador",fill=(150,168,186))
    board.save(out/"quatro-versoes.png")
print("Previews generated. These are desktop simulations, not captures from a physical ESP32.")
