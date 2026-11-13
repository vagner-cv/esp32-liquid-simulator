# Liquid Cube — simulador de líquidos para ESP32-S3

Firmware Arduino/C++ para a **Waveshare ESP32-S3-LCD-1.3**: uma caixa de vidro na tela contendo água animada, com nível que se desloca conforme a inclinação da placa. O sensor de movimento integrado **QMI8658** fornece a direção da gravidade. A água apresenta ondulação amortecida, inércia visual e bolhas.

> **Status:** protótipo v1.0. Código inicial gerado e importado para este repositório. **Ainda não compilado nem validado na placa física.** O teste local determinará possíveis ajustes de biblioteca, eixo e desempenho.

## Recursos

- Visualização em **ST7789 240 × 240** com moldura e reflexos de vidro.
- Água preenchendo ~57% do recipiente, com volume aparente aproximadamente conservado ao girar.
- Leitura do acelerômetro **QMI8658** por I²C (endereços 0x6A / 0x6B).
- Ondas, amortecimento, bolhas e sombras.
- Sem Wi-Fi, cartão SD ou módulos adicionais; renderização via **Arduino_GFX** com canvas.

## Hardware

| Componente | Especificação |
| --- | --- |
| Placa | Waveshare ESP32-S3-LCD-1.3 |
| Microcontrolador | ESP32-S3 |
| Tela | ST7789, 240 × 240, SPI |
| IMU | QMI8658 (acelerômetro) |
| Flash / PSRAM | 16 MB / 8 MB na configuração conhecida da placa |

### Pinagem atualmente usada no firmware

| Sinal | GPIO |
| --- | ---: |
| LCD DC | 38 |
| LCD CS | 39 |
| LCD SCK | 40 |
| LCD MOSI | 41 |
| LCD RST | 42 |
| LCD BL | 20 |
| I²C SDA (QMI8658) | 47 |
| I²C SCL (QMI8658) | 48 |

A pinagem é a usada no código inicial; se a sua revisão da placa diferir, consulte a documentação oficial.

## Instalação — Arduino IDE

1. Instale o pacote de placas **esp32 by Espressif Systems** no Gerenciador de Placas.
2. No Gerenciador de Bibliotecas instale **GFX Library for Arduino**, de **Moon On Our Nation** (biblioteca Arduino_GFX). Não confundir com Adafruit GFX.
3. Abra [`LiquidCube_ESP32S3/LiquidCube_ESP32S3.ino`](LiquidCube_ESP32S3/LiquidCube_ESP32S3.ino) no Arduino IDE.
4. Selecione **ESP32S3 Dev Module**; configure **Flash Size: 16 MB**, **PSRAM: OPI PSRAM** e **Partition Scheme: Huge APP** conforme opções disponíveis na sua versão do core.
5. Conecte a placa por USB-C, selecione a porta serial e execute **Upload**.
6. Para depurar, abra o **Monitor Serial a 115200 baud**.

**Atenção:** fazer upload substitui o firmware anterior (Pocket Explorer) na flash. Os dados armazenados no cartão microSD não são alterados por este sketch.

## Orientação da água

No início do `.ino`, ajuste os valores se os eixos ficarem invertidos:

```cpp
static constexpr bool SWAP_XY = false;
static constexpr float AXIS_X_SIGN = -1.0f;
static constexpr float AXIS_Y_SIGN = -1.0f;
```

- Água se movendo horizontalmente no sentido errado: inverta `AXIS_X_SIGN`.
- Água se movendo verticalmente no sentido errado: inverta `AXIS_Y_SIGN`.
- Movimentos em eixos trocados: alterne `SWAP_XY`.

Quando a tela está perfeitamente horizontal, a projeção de gravidade no plano da tela se aproxima de zero; o firmware assume uma orientação visual padrão. Este é um **efeito 2D** e não uma simulação física tridimensional de fluidos.

## Estrutura

```text
.
├── LiquidCube_ESP32S3/
│   └── LiquidCube_ESP32S3.ino   # sketch principal; nome coincide com a pasta
├── docs/
│   └── TESTE_LOCAL.md           # roteiro para validar a placa
├── AGENTS.md                    # contexto e regras para trabalho com Codex
├── .gitignore
└── README.md
```

## Próximos passos sugeridos

1. Compilar e testar na placa real.
2. Confirmar reconhecimento do QMI8658 e corrigir mapeamento dos eixos, se necessário.
3. Medir FPS e otimizar a renderização caso haja lentidão.
4. Evoluir a simulação: superfície com ondas mais naturais, efeito de choque nas paredes e interface opcional de ajustes.

## Referências

- [Waveshare — ESP32-S3-LCD-1.3](https://www.waveshare.com/wiki/ESP32-S3-LCD-1.3)
- [Arduino_GFX — Moon On Our Nation](https://github.com/moononournation/Arduino_GFX)

Projeto experimental de eletrônica + software embarcado.
