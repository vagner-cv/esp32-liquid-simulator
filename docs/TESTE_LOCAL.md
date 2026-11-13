# Checklist do teste local

Este documento registra o que verificar no **primeiro upload na Waveshare ESP32-S3-LCD-1.3**. A versão 3D ainda não foi compilada para ESP32-S3 nem validada fisicamente. Os testes de geometria no computador usam APIs de hardware simuladas.

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
- [ ] Texto aparece na posição correta após o giro de 90° horário.
- [ ] A tela inteira funciona como vidro frontal; paredes internas recuam para o fundo, sem cubo externo ou título.
- [ ] Monitor Serial em **115200** mostra endereço do QMI8658 (`0x6B` ou `0x6A`) e `ok=1`.
- [ ] Se não encontrar sensor, copiar o log; conferir SDA=47 e SCL=48 com a revisão do hardware.

## 4. Movimento

- [ ] Ao inclinar para a direita, o líquido vai para o lado que corresponde à gravidade.
- [ ] Ao inclinar para a esquerda, movimento se inverte.
- [ ] Ao girar 90°/180° no plano da tela, o nível parece manter o volume.
- [ ] Com a tela horizontal voltada para cima, a água se acumula no fundo do recipiente.
- [ ] Ao inclinar em direção à frente/fundo da tela, o nível muda na profundidade, não apenas no plano X/Y.
- [ ] Ao virar a placa com a tela para baixo, a água vem contra o vidro e pode cobrir toda a área visível, com o ar atrás dela.
- [ ] Ao girar lentamente em cada um dos três eixos, não aparecem saltos de volume nem travamentos.
- [ ] Ao sacudir com cuidado, a superfície oscila e volta ao repouso.
- [ ] Bolhas se movem para cima em relação à gravidade.
- [ ] Não há piscadas excessivas ou travamentos.

Caso a direção fique errada, conferir `SWAP_XY`, `AXIS_X_SIGN`, `AXIS_Y_SIGN` e `AXIS_Z_SIGN` no `.ino`. X/Y recebem compensação pela rotação da imagem; consulte a seção de orientação do README antes de inverter sinais.

## 5. Resultado

Quando voltar ao Codex, informar:
- Versão do Arduino IDE/core esp32 e Arduino_GFX.
- Se compilou e gravou.
- Log do Monitor Serial (se houver).
- Se a tela aparece corretamente e se os movimentos são invertidos.
- Se a animação parece fluida ou lenta.

**Aviso:** a gravação substitui o firmware atual da flash, mas este sketch não escreve no microSD.
