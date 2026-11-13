# AGENTS.md — instruções para Codex / agentes de código

## Objetivo

Evoluir o **Liquid Cube**, uma simulação visual de água dentro de um recipiente exibido na Waveshare ESP32-S3-LCD-1.3. O usuário gira a placa física e a superfície da água reage à gravidade, com ondas, inércia e bolhas.

## Contexto do hardware

- Waveshare **ESP32-S3-LCD-1.3**; ESP32-S3, display ST7789 240x240 SPI e QMI8658 integrado por I²C.
- Core: **esp32 by Espressif Systems** no Arduino IDE.
- Biblioteca gráfica: **GFX Library for Arduino**, de **Moon On Our Nation** (header `Arduino_GFX_Library.h`).
- **Sem hardware extra** no estágio atual; não depender de Wi-Fi, GPS, cartão SD ou sensores externos.
- Configuração previamente usada: 16 MB flash, 8 MB OPI PSRAM, ESP32S3 Dev Module, Huge APP.
- Pinos LCD: DC 38, CS 39, SCK 40, MOSI 41, RST 42, BL 20.
- Pinos IMU: SDA 47, SCL 48; endereços I²C 0x6B/0x6A e ID do QMI8658 0x05 (como usado no protótipo).

## Código-fonte principal

`LiquidCube_ESP32S3/LiquidCube_ESP32S3.ino`

Preservar o nome da pasta igual ao nome do arquivo `.ino` para abertura direta no Arduino IDE.

O código inicial implementa:
- desenho de vidro e líquido com `Arduino_Canvas`;
- recorte geométrico para manter a área aproximada da água;
- direção gravitacional filtrada por acelerômetro;
- ondas amortecidas e bolhas.

### Parâmetros de ajuste atuais

- `SWAP_XY`, `AXIS_X_SIGN`, `AXIS_Y_SIGN`: correção da orientação dos eixos;
- `WATER_FRACTION`: nível de preenchimento;
- `BUBBLE_COUNT`: número de bolhas;
- `previousFrame` e intervalo de 33 ms: alvo visual de ~30 FPS.

## Regras de desenvolvimento

1. Priorizar compatibilidade com Arduino/C++ e execução offline no ESP32-S3.
2. **Não** portar funcionalidades do projeto separado Pocket Explorer para cá sem pedido explícito.
3. Antes de alterar GPIOs, endereços, opções da Arduino_GFX ou inicialização do QMI8658, conferir a variante específica da Waveshare e explicar a alteração.
4. Não afirmar que compilou, funcionou no hardware ou atingiu 30 FPS sem realmente realizar o teste correspondente.
5. Alterações relevantes devem incluir notas de teste e documentação atualizada.
6. Manter a simulação responsiva e o frame buffer dentro dos recursos do ESP32; reduzir desenho pixel-a-pixel custoso quando viável.
7. Não substituir o código funcional por efeitos visuais não testados sem manter um caminho de recuperação.
8. Ao receber um relato de falha do usuário, pedir ou usar o erro de compilação, log serial ou observação visual específica antes de suposições.
9. Antes de alterações grandes, propor etapas pequenas e verificáveis; use commits/PRs descritivos.
10. Nunca adicionar credenciais, tokens ou segredos ao repositório (ele é público).

## Situação atual e validação

O firmware foi gerado no ChatGPT e importado para este repositório. **Ainda precisa ser compilado e testado na placa física.**

Ao iniciar o trabalho:
1. Ler `README.md`, este arquivo e `docs/TESTE_LOCAL.md`.
2. Revisar primeiro a biblioteca, construtores SPI/ST7789/Canvas e a configuração I²C do QMI8658.
3. Solicitar ao usuário resultados do teste Arduino IDE; corrigir primeiro problemas de compilação, depois inicialização do display/IMU, depois orientação e fluidez.
4. Se disponível, instalar/usar `arduino-cli` para verificar a compilação, explicitando versão do core e bibliotecas.
5. Separar resultados comprovados de hipóteses.

## Visão de evolução

- MVP: água inclinável e ondas no display.
- Refinamento: gravidade e inércia mais naturais, colisão com bordas, reflexos e bolhas.
- Futuro, opcional: escolha de líquidos, parâmetros de viscosidade e estilo visual.
