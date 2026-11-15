#!/usr/bin/env python3
"""Build four standalone Arduino folders from the canonical engine. No extra packages."""
from pathlib import Path
import json
import shutil

ROOT = Path(__file__).resolve().parents[1]
SCENE = (ROOT / "engine/Scene.h").read_text()

def excerpt(start, end):
    section = SCENE.split(start, 1)[1].split(end, 1)[0]
    lines = [line[4:] if line.startswith("    ") else line for line in section.splitlines()]
    return [line.rstrip() for line in lines if line.strip()]

tree = excerpt("void buildTree() {", "void background(")
jelly = excerpt("void jelly(float t, float cx,float cy,float cz,float size, bool small=false) {", "void render(")
# The panel displays actual excerpts, not a Python script or an executable editor.
def array(name, lines):
    return (f"static const char *const {name}[] = {{\n" +
            "\n".join("  " + json.dumps(line, ensure_ascii=True) + "," for line in lines) +
            f"\n}};\nconstexpr int {name}_COUNT = sizeof({name})/sizeof({name}[0]);\n")

code = "#pragma once\nnamespace ascii_show {\n" + array("TREE_CODE", tree) + array("JELLY_CODE", jelly) + "}\n"
(ROOT / "engine/CodeLines.h").write_text(code)
MODES = [("Arvore_ASCII", 1, 0), ("Arvore_ASCII_Codigo", 1, 1),
         ("AguaViva_ASCII", 0, 0), ("AguaViva_ASCII_Codigo", 0, 1)]
for name, is_tree, show_code in MODES:
    sketch = ROOT / name
    (sketch / "src").mkdir(parents=True, exist_ok=True)
    for header in (ROOT / "engine").glob("*.h"):
        shutil.copyfile(header, sketch / "src" / header.name)
    (sketch / f"{name}.ino").write_text(f'''// Waveshare ESP32-S3-LCD-1.3 | ST7789 240x240
// Requer GFX Library for Arduino (Moon On Our Nation).
// Gerado por tools/generate.py; fonte original em ../engine/.
// Serial: p pausa/retoma, r reinicia; 115200 baud.
#define ASCII_TREE {is_tree}
#define ASCII_CODE {show_code}
#define ASCII_HIGH_DETAIL 1 // 80x60 caracteres suavizados; 0 = 60x40 nitidos.
#include "src/AsciiShow.h"
void setup() {{ ascii_show::begin(); }}
void loop() {{ ascii_show::tick(); }}
''')
print("Generated four standalone sketches and source excerpts.")
