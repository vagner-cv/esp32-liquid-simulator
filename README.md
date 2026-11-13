# Liquid Cube — simulador de líquidos para ESP32-S3

Firmware Arduino/C++ para a **Waveshare ESP32-S3-LCD-1.3**: um recipiente 3D visto através da própria tela, que funciona como parede frontal, contendo água animada, que responde à inclinação nos três eixos, inclusive em direção à frente e ao fundo da tela. O sensor de movimento integrado **QMI8658** fornece a direção da gravidade. A água apresenta ondulação amortecida, inércia visual e bolhas.

> **Status:** protótipo 3D. A geometria foi verificada em testes no computador com APIs de hardware simuladas; a versão 3D **ainda não foi compilada para ESP32-S3 nem validada na placa física**. O teste local determinará ajustes de eixo e desempenho.

## Recursos

- Visualização em **ST7789 240 × 240** com moldura e reflexos de vidro.
- Água preenchendo ~57% do volume 3D do recipiente, com nível calculado por recorte de um cubo por um plano.
- Leitura do acelerômetro **QMI8658** por I²C (endereços 0x6A / 0x6B).
- Balanço amortecido da superfície e bolhas com posição e movimento em 3D.
- Imagem girada 90° no sentido horário, com compensação dos eixos X/Y.
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
static constexpr float AXIS_Z_SIGN = -1.0f;
static constexpr uint8_t DISPLAY_ROTATION = 1; // 90° horário
```

Os sinais são aplicados aos eixos do sensor antes da compensação da rotação da imagem. Com a rotação atual, o X do sensor alimenta o Y lógico (com sinal invertido), e o Y do sensor alimenta o X lógico. `SWAP_XY` troca os eixos antes dessa transformação; confirme as direções na placa antes de ajustar sinais.

O eixo Z participa do cálculo mesmo com a tela horizontal. Inclinar a placa para a frente ou para trás muda a posição do líquido na profundidade do cubo; virar a placa transfere o líquido para a face oposta. Se frente e fundo estiverem invertidos, confira `AXIS_Z_SIGN` com o teste local.

A tela inteira coincide com a face frontal do recipiente. A câmera olha de frente para dentro: as paredes laterais recuam até o fundo, sem um cubo flutuando fora da tela. Não há título nem margens durante o funcionamento normal. A rotação de 90° horário foi mantida. A simulação usa um volume 3D com superfície plana oscilante e conservação de preenchimento; ainda não resolve escoamento, respingos ou ondas locais como um solver completo de fluidos. O desenho usa blocos 2 × 2 para limitar o custo. O intervalo de 33 ms é um alvo, não uma medição de FPS.

## Validação desta alteração

- Testes C++ no computador com AddressSanitizer e UndefinedBehaviorSanitizer: nove orientações de preenchimento, inversão frente/fundo, projeção da face frontal nos limites da tela, raios até as paredes internas, bolhas e execução de 100 quadros; aprovados. As chamadas Arduino/GFX/I²C foram simuladas, portanto esses testes não comprovam compilação ou execução no ESP32.
- Assinaturas dos construtores SPI, ST7789 e Canvas conferidas no código Arduino_GFX consultado (versão declarada 1.6.9). Incluído `esp_random.h` explicitamente para a função usada no seed.
- Arduino CLI 1.3.1 instalado no ambiente com SHA-256 verificado. Instalação do core/biblioteca bloqueada pela política de rede nos índices Arduino/Espressif; compilação real pendente.
- GPIOs, registradores do QMI8658 e frequência SPI foram preservados. O teste físico ainda deve confirmar tela, orientação, sensor e fluidez.

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
