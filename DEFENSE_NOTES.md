# Diário Técnico — `doom-mega` (Notas para Defesa Oral)

Este documento registra as decisões de arquitetura, portabilidade de hardware e correções técnicas aplicadas na migração e evolução do `doom-nano` (ATmega328P) para o **Arduino Mega 2560 (ATmega2560)**.

---

## 1. Diferenças Arquiteturais: ATmega328P vs. ATmega2560

| Recurso | Arduino Nano / Uno (ATmega328P) | Arduino Mega 2560 (ATmega2560) | Impacto no `doom-mega` |
| :--- | :--- | :--- | :--- |
| Clock | 16 MHz | 16 MHz | Fórmulas de `TWBR`, `OCR1A` e `OCR2A` permanecem com `F_CPU = 16000000UL`. |
| Flash | 32 KB | **256 KB** | Folga para mapas, sprites e novas cenas. Atenção: ponteiros `PROGMEM` de 16 bits (`pgm_read_byte` e `uint16_t snd_ptr`) operam na janela inicial de 64 KB; se os assets ultrapassarem 64 KB, será necessário `pgm_read_byte_far()`. |
| SRAM | 2 KB | **8 KB** | O framebuffer do SSD1306 (1024 B) + buffer TWI (344 B) consumiam >65% da RAM no Nano; no Mega 2560 há ampla folga para Z-buffer mais preciso, minimapa e entidades extras. |
| Barramento TWI (I2C) | `A4` (`PC4`, SDA) / `A5` (`PC5`, SCL) | **`D20` (`PD1`, SDA) / `D21` (`PD0`, SCL)** | Os registradores TWI (`TWBR`, `TWSR`, `TWDR`, `TWCR`) e `ISR(TWI_vect)` são compatíveis, mas a fiação muda obrigatoriamente para D20/D21 (ou pinos SDA/SCL R3). |
| Saída Timer1 `OC1A` | **`D9` (`PB1`)** | **`D11` (`PB5`)** | **Ponto crítico do port:** o driver de som usa toggle em hardware (`COM1A0`) no pino `OC1A`. No Mega 2560, `D9` corresponde a `PH6` (`OC2B`), desconectado do Timer1. Manter o buzzer em `D9` deixava o áudio completamente mudo. Por isso `PIN_BUZZER` foi migrado para **`D11`**. |

---

## 2. Drivers Bare-Metal: Áudio (2 Timers) e TWI/I2C (400 kHz)

### 2.1. Driver de Áudio Não-Bloqueante (`doom-mega/sound.h` e `sound.cpp`)
Em vez de usar `tone()` ou `delay()` (que travariam o loop do raycaster), o áudio combina dois timers AVR em paralelo:
1. **Timer1 (16 bits — gerador de frequência em hardware):** configurado em modo **CTC** (`WGM12 = 1`) com toggle automático na saída **`OC1A` (`D11` / `PB5`)** (`TCCR1A = _BV(COM1A0)`). `setFrequency()` seleciona o menor prescaler adequado (1, 8, 64, 256 ou 1024) e grava o valor de comparação em `OCR1A`. A onda quadrada é gerada sem intervenção da CPU.
2. **Timer2 (8 bits — sequenciador de amostras por interrupção):** configurado em modo **CTC** (`WGM21 = 1`) com prescaler `1024` e `OCR2A = 111`, gerando `ISR(TIMER2_COMPA_vect)` a:
   $$f_{\text{ISR}} = \frac{16\,000\,000}{1024 \times 112} \approx 139{,}5\text{ Hz}\;(\sim 7{,}17\text{ ms por amostra})$$
   Essa taxa reproduz o formato *Inverse Frequency Sound* do PC Speaker (Wolfenstein 3D), onde cada byte em `PROGMEM` define uma frequência $f \approx \frac{1\,192\,030}{60 \times \text{amostra}}$.

**Correções aplicadas na Fase 2 em `sound.cpp`:**
- Variáveis compartilhadas com a ISR (`idx`, `sound`, `snd_ptr`, `snd_len`) marcadas como `volatile`.
- Escritas de 16 bits (`snd_ptr`) e reinicialização de `idx = 0` em `playSound()` protegidas com `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)` para evitar condições de corrida na arquitetura AVR de 8 bits.
- Correção do *off-by-one* na ISR: agora lê `snd[idx]` antes de incrementar `idx++` (o código original pulava o primeiro byte e lia 1 byte além do fim).
- Proteção contra amostra `0x00` (silencia via `sound_off()` em vez de dividir por zero).
- `sound_off()` desliga `TCCR1A`/`TCCR1B` e força `digitalWrite(PIN_BUZZER, LOW)`, evitando corrente DC contínua na bobina do buzzer passivo.

### 2.2. Driver TWI/I2C a 400 kHz (`doom-mega/TWI_Master.h` e `TWI_Master.cpp`)
O jogo não utiliza `Wire.h` nem `Adafruit_GFX`; usa um driver TWI bare-metal dirigido por interrupção (`ISR(TWI_vect)`) acoplado à versão enxuta de `SSD1306`:
- **Clock I2C (Fast Mode — 400 kHz):** com `TWSR = 0` (prescaler = 1) e `TWBR = ((16000000 / 400000) - 16) / 2 = 12`:
  $$f_{\text{SCL}} = \frac{16\,\text{MHz}}{16 + 2 \times 12 \times 1} = 400\,\text{kHz}$$
- **Throughput do framebuffer:** o buffer interno de `TWI_Master` (`TWI_BUFFER_SIZE = 344` bytes) permite transmitir o framebuffer completo de `1024` bytes em apenas **3 transações** (`WIRE_MAX = 342` bytes por pacote), reduzindo drasticamente o overhead de `START`/endereço em comparação ao buffer de 32 bytes do `Wire.h`.
- **Correções aplicadas na Fase 2:** inicialização explícita de `TWSR = 0`, máscara `TWSR & 0xF8` no `switch` da ISR, derivação automática de `TWI_ADDR = (OLED_I2C_ADDR << 1)` a partir de `pinout.h` (eliminando endereço `0x78` desalinhado) e proteção contra estouro de `TWI_BUFFER_SIZE`.

---

## 3. Tabela de Pinagem Oficial (`doom-mega/pinout.h`)

| Periférico | Sinal | Constante / Macro | Pino (Mega 2560) | Observação |
| :--- | :--- | :--- | :---: | :--- |
| OLED 0.96" SSD1306 (I2C) | SDA | *(TWI hardware)* | **D20** (`PD1`) | Endereço 7-bit `OLED_I2C_ADDR = 0x3C` (`TWI_ADDR = 0x78`) |
| OLED 0.96" SSD1306 (I2C) | SCL | *(TWI hardware)* | **D21** (`PD0`) | Clock 400 kHz (Fast Mode) |
| Joystick Esquerdo (Movimento) | VRx | `PIN_JOY_L_X` | **A0** | Strafe / eixo X (Fase 3) |
| Joystick Esquerdo (Movimento) | VRy | `PIN_JOY_L_Y` | **A1** | Frente / trás (Fase 3) |
| Joystick Esquerdo (Movimento) | SW | `PIN_JOY_L_SW` | **D22** | `INPUT_PULLUP` (ativo em `LOW`) |
| Joystick Direito (Câmera) | VRx | `PIN_JOY_R_X` | **A2** | Rotação da câmera (Fase 3) |
| Joystick Direito (Câmera) | VRy | `PIN_JOY_R_Y` | **A3** | Eixo Y auxiliar (Fase 3) |
| Joystick Direito (Câmera) | SW | `PIN_JOY_R_SW` | **D24** | `INPUT_PULLUP` (ativo em `LOW`) |
| Push-Button Comum | Tiro | `PIN_BTN_FIRE` | **D26** | `INPUT_PULLUP` (ativo em `LOW`); já mapeado como `K_FIRE` |
| Buzzer Passivo | Sinal PWM/CTC | `PIN_BUZZER` | **D11** (`PB5`/`OC1A`) | Saída direta do Timer1 `OC1A` |

> **Status atual (Fase 3):** `input.cpp` opera integralmente com os 2 módulos analógicos KY-023 (`A0..A3`, `D22`, `D24`) e o botão de tiro (`D26`).

---

## 4. Resumo das Estabilizações Estruturais (Fase 2)

- **Estrutura Arduino IDE:** sketch movido para `doom-mega/doom-mega.ino` (nome da pasta igual ao `.ino`), mais sketch independente `hardware-test/hardware-test.ino` para validação em bancada (I2C Scanner, teste visual dual-color `y=0..15` amarelo / `y=16..63` azul via `Wire`, bips no D11 e telemetria Serial a 115200 baud).
- **C++ / Compilação limpa no `avr-g++`:**
  - Instanciação explícita `template class Adafruit_SSD1306<128, 64>` movida para o final de `SSD1306.cpp` (`extern template` em `SSD1306.h`), eliminando risco de `undefined reference` em GCC 7.3+.
  - Eliminação de VLA (`uint8_t tmp[n]`) em `SSD1306.cpp` por envio em blocos fixos de 16 bytes.
  - Separação modular de `display.h`/`.cpp` e `sound.h`/`.cpp` com include guards e protótipo `drawText(..., const __FlashStringHelper *txt_p, ...)` corrigido.
  - Remoção de comportamentos indefinidos: `sortEntities()` alterada para `void`, correção de loops/comparações `uint8_t` (`initializeLevel`, `getBlockAt`), acesso out-of-bounds em `removeEntity`/`removeStaticEntity`, shift com sinal em `create_uid()`, typo `crate_static_entity`, inicialização de `bp`, `last_uid`, `view_height`, `jogging`, reset de `num_entities` e `lastFrameTime` ao iniciar partida.

---

## 5. Controles Twin-Stick FPS e Cinemática Vetorial (Fase 3)

### 5.1. Conversão A/D de 10 bits, Calibração Dinâmica e Deadzone Contínua (`input.cpp`)
- **ADC de 10 bits no ATmega2560:** cada potenciômetro dos módulos KY-023 (`A0..A3`) é convertido pelo ADC interno em um inteiro de `0` a `1023` ($2^{10} - 1$).
- **Calibração dinâmica do centro no boot (`input_calibrate()`):** potenciômetros mecânicos baratos raramente descansam exatamente em `512` (podem variar entre ~480 e ~540). No `setup()`, o driver descarta a primeira leitura após trocar o canal do multiplexador (estabilização do capacitor *Sample-and-Hold*) e calcula a média de **16 amostras** (`JOY_CAL_SAMPLES`) para cada um dos 4 eixos em repouso, armazenando o centro real `center_*`.
- **Deadzone com remapeamento contínuo (`normalizeAxis()`):** em vez de um corte abrupto (que criaria um "degrau" de saída ao sair da zona morta), o valor fora de `±JOY_DEADZONE` (`±80`) é subtraído da borda da deadzone e dividido pelo curso útil restante até o batente (`1023 - center - deadzone` no lado positivo; `center - deadzone` no lado negativo):
  $$u = \begin{cases} 0 & \text{se } |\text{raw} - c| \le D \\ +\dfrac{(\text{raw} - c) - D}{(1023 - c) - D} & \text{se } \text{raw} - c > D \\ -\dfrac{(c - \text{raw}) - D}{c - D} & \text{se } \text{raw} - c < -D \end{cases}$$
  Isso garante saída `0.0` estável em repouso, transição suave a partir de `0.0` fora da zona morta e saturação simétrica em `±1.0` nos batentes, mesmo com centro físico assimétrico. Flags `JOY_*_INVERT_*` em `pinout.h` permitem inverter qualquer eixo em uma linha caso o módulo seja montado girado na protoboard.

### 5.2. Debounce Não-Bloqueante e Detecção de Borda (`INPUT_PULLUP`)
- Os 3 botões digitais (`PIN_BTN_FIRE = D26`, `PIN_JOY_L_SW = D22`, `PIN_JOY_R_SW = D24`) usam resistores internos `INPUT_PULLUP` (lógica ativa-baixa: pressionado = `LOW`).
- Cada botão mantém estado bruto (`raw_state`), estado estável (`stable_state`), timestamp da última transição (`last_change_ms`) e flag de evento único (`just_pressed`).
- Uma mudança só é promovida para `stable_state` após permanecer consistente por `BTN_DEBOUNCE_MS` (`30 ms`) medidos via `millis()` — sem `delay()` bloqueante.
- A flag `just_pressed` é levantada por **exatamente um ciclo de `input_update()`** na transição `solto → pressionado` (borda de descida elétrica). Com isso, confirmar início em `loopIntro()` ou reiniciar após morte não gera disparo acidental no primeiro frame da partida.

### 5.3. Matemática Vetorial do Strafe, Normalização Diagonal e Otimização de Rotação
- **Convenção de base da câmera no raycaster:** em `entities.h`, o jogador inicia olhando em $\vec{d} = (1, 0)$ com plano de câmera $\vec{p} = (0, -0{,}66)$. Como o raycaster calcula $\vec{r} = \vec{d} + \text{camera\_x}\cdot\vec{p}$ com $\text{camera\_x} \in [-1, +1]$ da esquerda para a direita da tela, $\vec{p}$ aponta sempre para a **direita da câmera**.
- **Vetor ortogonal de strafe:** o vetor unitário na mesma direção de $\vec{p}$ (direita) é obtido girando $\vec{d}$ por $-90^\circ$:
  $$\vec{r}_{\text{dir}} = (+d_y,\; -d_x)$$
  Assim, o deslocamento no mundo combinando velocidade longitudinal $v$ e lateral $s$ é:
  $$\Delta x = (d_x \cdot v + d_y \cdot s)\cdot\Delta t, \qquad \Delta y = (d_y \cdot v - d_x \cdot s)\cdot\Delta t$$
- **Rotação coerente:** para `input_turn() > 0` (direita), aplica-se matriz de rotação 2D com ângulo negativo $\theta = -\text{turn}\cdot\text{ROT\_SPEED}\cdot\Delta t$ em $\vec{d}$ e $\vec{p}$ simultaneamente, mantendo $\vec{r}_{\text{dir}}$ sempre alinhado a $\vec{p}$.
- **Normalização diagonal:** como os dois eixos do joystick formam um domínio quadrado $[-1, 1]^2$, empurrar na diagonal produziria magnitude $\sqrt{1^2 + 1^2} \approx 1{,}414$ (+41% de velocidade). Quando `fwd² + str² > 1.0`, o vetor é reescalado por $1 / \sqrt{\text{fwd}^2 + \text{str}^2}$, mantendo velocidade uniforme em $360^\circ$ (e evitando `sqrt()` quando a norma já está dentro do círculo unitário).
- **Economia de CPU de 8 bits:** na rotação, `cos()` e `sin()` são completamente **ignorados** quando `input_turn() == 0.0f` (analógico dentro da deadzone) e, quando ativos, calculados apenas **1 vez por frame** (antes eram calculados 4 vezes por frame).

