# doom-mega

Port e evolução do raycaster `doom-nano` para **Arduino Mega 2560 (ATmega2560)** compilado pela **Arduino IDE**.

## Estrutura do Repositório

- `doom-mega/` — Sketch principal do jogo (`doom-mega.ino`) com contrato de pinagem (`pinout.h`) e drivers embarcados (SSD1306 slim via TWI 400 kHz e áudio não-bloqueante via Timer1/Timer2).
- `hardware-test/` — Sketch autônomo de diagnóstico de bancada (`hardware-test.ino`) para validar I2C (D20/D21), OLED Dual-Color, buzzer (D11), joysticks KY-023 e botão de tiro (D26) sem bibliotecas externas.
- `DEFENSE_NOTES.md` — Diário técnico detalhando diferenças ATmega328P vs. ATmega2560, drivers e tabela de pinagem.

## Como Compilar na Arduino IDE

1. Selecione **Placa:** `Arduino Mega or Mega 2560` e **Processador:** `ATmega2560 (Mega 2560)`.
2. Nenhuma biblioteca externa precisa ser instalada.
3. Para validar a montagem física antes do jogo, abra e envie `hardware-test/hardware-test.ino` e abra o Serial Monitor a **115200 baud**.
4. Para compilar o jogo base, abra `doom-mega/doom-mega.ino`.
