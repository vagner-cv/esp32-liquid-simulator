# Validação das quatro animações

## Ambiente e compilação real

- Arduino CLI **1.6.0**.
- Core **esp32 by Espressif Systems 3.3.12**.
- **GFX Library for Arduino 1.6.9**, instalada pelo gerenciador Arduino.
- Alvo: `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=huge_app,CDCOnBoot=cdc`.
- Construtores SPI/ST7789/Canvas, fonte e APIs compilados com a biblioteca real e a toolchain Xtensa do core.

As quatro configurações finais compilaram e linkaram com sucesso:

| Configuração | Compilação | Flash (bytes) | Globais RAM (bytes) |
| --- | --- | ---: | ---: |
| `Arvore_ASCII` | Aprovado | 383,616 | 73,952 |
| `Arvore_ASCII_Codigo` | Aprovado | 388,424 | 74,000 |
| `AguaViva_ASCII` | Aprovado | 379,728 | 73,944 |
| `AguaViva_ASCII_Codigo` | Aprovado | 383,124 | 73,992 |

A RAM global informada pelo compilador não inclui o framebuffer e a nuvem de pontos alocados durante a inicialização. A verificação final recompilou os conteúdos dos quatro `.ino`, com os mesmos headers, em uma pasta de compilação fixa para reutilizar os objetos da biblioteca; cada pasta distribuída mantém seu `.ino` de mesmo nome.

Para reproduzir, com o core e a biblioteca instalados:

```sh
bash AsciiShows/tools/compile.sh
```

Se o CLI usar um arquivo de configuração específico, defina `ARDUINO_CONFIG=/caminho/arduino-cli.yaml` antes do comando.

## Testes da cena no computador

`tests/scene_test.cpp` verifica:

- 14.628 pontos finitos da árvore, sete tipos de material e capacidade máxima respeitada.
- Normais compactas com comprimento unitário, direção da luz lateral, mais de 12 tons verdes e diferença superior a 4× entre verdes claros e escuros no quadro de teste.
- Interpolação do halo nos limites da tela e presença de emissão nas luzes.
- Formação progressiva, mudança ao girar e reinício da sequência aos 40 segundos.
- Movimento da água-viva entre instantes diferentes.
- Partículas continuam dando a volta na tela após semanas de funcionamento simulado.
- 1.280 quadros nos dois layouts, limites da grade, profundidade finita e regiões externas limpas.
- Oclusão do ponto distante pelo mais próximo e descarte de coordenadas fora da tela/atrás da câmera.
- Tratamento de armazenamento de pontos indisponível.

O conjunto foi executado nos modos **80 × 60 suavizado** e **60 × 40 nítido**, com **AddressSanitizer e UndefinedBehaviorSanitizer**, sem erros de memória ou comportamento indefinido nos testes. A detecção de vazamentos do LeakSanitizer foi desativada porque este ambiente usa ptrace; a checagem de limites do AddressSanitizer permaneceu ativa.

```sh
g++ -std=c++17 -Wall -Wextra -Werror -O1 -g \
  -fsanitize=address,undefined AsciiShows/tests/scene_test.cpp -o /tmp/ascii-scene-test
ASAN_OPTIONS=detect_leaks=0 /tmp/ascii-scene-test
```

Para repetir a outra densidade, acrescente `-DASCII_HIGH_DETAIL=0` à compilação.

## Renderizador, painel e controles

`tests/preview.cpp` executa o mesmo adaptador `engine/AsciiShow.h`, substituindo apenas tela, alocador e serial por implementações de computador. A fonte 5 × 7 vem da instalação real de Arduino_GFX.

Os quatro executáveis de prévia foram compilados com `-Wall -Wextra -Werror` e passaram em pausa/retomada, reinício pelo serial e transbordamento de `millis()`. As duas árvores também passaram no teste de falha de alocação, exibindo o aviso de PSRAM sem iniciar o renderizador. Cada prévia tem 48 quadros de movimento e quadros adicionais dos estágios de formação.

```sh
# Pillow só é necessário para gerar as prévias, não para usar a placa.
python3 AsciiShows/tools/preview.py \
  --gfx-font-dir /caminho/GFX_Library_for_Arduino/src/font
```

As imagens mostram a orientação lógica da cena; a rotação e a transferência SPI no hardware não são simuladas.

## O que falta conferir na placa física

Não houve acesso à placa para upload ou teste físico. Compilar para ESP32-S3 e renderizar no computador não comprovam imagem, orientação ou desempenho no display real.

Depois de gravar cada sketch:

1. Verifique se a tela acende, ocupa os 240 × 240 pixels e aparece na orientação esperada.
2. Aguarde pelo menos 15 segundos para ver a árvore formada e decorada; confira rotação, presentes e estrela. Aguarde 40 segundos para conferir o reinício.
3. Na água-viva, confira pulsação, tentáculos, cores e as duas figuras menores ao fundo.
4. Nas versões com código, confirme que a cena continua animando e o texto rola apenas no painel inferior.
5. No serial a 115200, teste `p` e `r` e anote o **FPS medido** a cada cinco segundos. Não há FPS de hardware medido neste relatório.
6. Compare `ASCII_HIGH_DETAIL=1` e `0` caso queira escolher entre maior densidade e símbolos mais nítidos.

O Liquid Cube original permanece na pasta de origem para ser gravado novamente caso queira restaurá-lo.
