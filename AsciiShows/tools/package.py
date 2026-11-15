#!/usr/bin/env python3
"""Package complete standalone sketch folders with installation instructions."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

ROOT=Path(__file__).resolve().parents[1]
out=ROOT/"downloads"
out.mkdir(exist_ok=True)
for name in ("Arvore_ASCII","Arvore_ASCII_Codigo","AguaViva_ASCII","AguaViva_ASCII_Codigo"):
    with ZipFile(out/(name+".zip"),"w",ZIP_DEFLATED) as zipfile:
        for file in sorted((ROOT/name).rglob("*")):
            if file.is_file(): zipfile.write(file,file.relative_to(ROOT))
        zipfile.writestr("LEIA-ME.md",f'''# {name} — Waveshare ESP32-S3-LCD-1.3

1. Extraia o ZIP mantendo a pasta {name} e sua subpasta src.
2. Instale o pacote esp32 by Espressif Systems e a biblioteca GFX Library for Arduino, de Moon On Our Nation.
3. Abra {name}/{name}.ino no Arduino IDE.
4. Selecione ESP32S3 Dev Module, Flash 16 MB, OPI PSRAM, Huge APP e USB CDC On Boot Enabled.
5. Selecione a porta USB e envie o firmware.

Monitor Serial: 115200 baud. Envie p para pausar/retomar ou r para reiniciar.
O upload substitui o firmware atual da placa. Para restaurar o Liquid Cube, grave novamente o sketch original.

A tela física tem 240 x 240 pixels. ASCII_HIGH_DETAIL=1 usa 80 x 60 caracteres suavizados;
ASCII_HIGH_DETAIL=0 usa 60 x 40 caracteres nítidos. Nas versões com código, a cena ocupa
156 pixels de altura e o painel inferior ocupa 84 pixels. O painel é um efeito visual
com trechos reais de C++ rolando. As animações funcionam offline, sem módulos extras.

LCD: DC 38, CS 39, SCK 40, MOSI 41, RST 42, BL 20. Rotação do canvas: 1 (90 graus).

Prévias, documentação e as quatro versões:
https://github.com/vagner-cv/esp32-liquid-simulator/tree/feat/ascii-tree-jellyfish/AsciiShows

Consulte VALIDACAO.md para os testes realizados e as verificações físicas ainda pendentes.
''')
        zipfile.write(ROOT/"VALIDACAO.md","VALIDACAO.md")
    print(out/(name+".zip"))
