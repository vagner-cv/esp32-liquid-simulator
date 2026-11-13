# Checklist do teste local

Este documento registra o que verificar no **primeiro upload na Waveshare ESP32-S3-LCD-1.3**. O firmware ainda não foi validado fisicamente.

## 1. Preparação

- [ ] Arquivo `LiquidCube_ESP32S3.ino` aberto pelo Arduino IDE na pasta homônima.
- [ ] Placa `ESP32S3 Dev Module` selecionada.
- [ ] Pacote `esp32 by Espressif Systems` instalado; anotar versão.
- [ ] Biblioteca `GFX Library for Arduino` / `Arduino_GFX` de Moon On Our Nation instalada; anotar versão.
- [ ] Flash 16 MB, OPI PSRAM, partições Huge APP (quando disponíveis).
- [ ] Porta USB/serial selecionada.

## 2. Compilação

- [ ] Código compila sem erros.
- [ ] Upload termina corretamente.
- [ ] Caso falhe, copiar **o texto completo da primeira mensagem de erro relevante** e versões do core/biblioteca.

## 3. Tela e sensor

- [ ] Backlight acende.
- [ ] Tela mostra fundo, caixa de vidro e água azul.
- [ ] Monitor Serial em **115200** mostra endereço do QMI8658 (`0x6B` ou `0x6A`) e `ok=1`.
- [ ] Se não encontrar sensor, copiar o log; conferir SDA=47 e SCL=48 com a revisão do hardware.

## 4. Movimento

- [ ] Ao inclinar para a direita, o líquido vai para o lado que corresponde à gravidade.
- [ ] Ao inclinar para a esquerda, movimento se inverte.
- [ ] Ao girar 90°/180°, o nível parece manter o volume.
- [ ] Ao sacudir com cuidado, surgem ondas que se dissipam.
- [ ] Bolhas se movem para cima em relação à gravidade.
- [ ] Não há piscadas excessivas ou travamentos.

Caso a direção fique errada, conferir `SWAP_XY`, `AXIS_X_SIGN` e `AXIS_Y_SIGN` no `.ino`.

## 5. Resultado

Quando voltar ao Codex, informar:
- Versão do Arduino IDE/core esp32 e Arduino_GFX.
- Se compilou e gravou.
- Log do Monitor Serial (se houver).
- Se a tela aparece corretamente e se os movimentos são invertidos.
- Se a animação parece fluida ou lenta.

**Aviso:** a gravação substitui o firmware atual da flash, mas este sketch não escreve no microSD.
