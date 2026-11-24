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

---

## 6. Layout Dual-Color por Páginas do SSD1306 e Z-buffer de 16 bits (Fase 4)

### 6.1. Alinhamento por Páginas do SSD1306 e Zero Flicker no HUD (`hud.cpp`, `display.cpp`)
O controlador SSD1306 128×64 organiza sua GDDRAM em **8 páginas horizontais de 128 bytes** (1024 bytes no total), onde cada byte controla uma coluna vertical de 8 pixels (bit 0 no topo da página, bit 7 na base). No display OLED 0.96" Dual-Color, a fronteira física entre os LEDs amarelos (`y = 0..15`) e azuis (`y = 16..63`) coincide **exatamente** com a fronteira entre a página 1 e a página 2:

| Região física | Coordenadas `y` | Páginas SSD1306 | Offset em `display_buf` | Uso no `doom-mega` |
| :--- | :---: | :---: | :---: | :--- |
| Faixa superior **Amarela** (16 px) | `0..15` | Páginas 0 e 1 | `0..255` (256 B) | HUD dedicado (`hud.h`/`.cpp`): vida + barra visual, chaves, `RUN` (sprint), `FPS` e divisória em `y = 15`. |
| Faixa inferior **Azul** (48 px) | `16..63` | Páginas 2 a 7 | `256..1023` (768 B) | Viewport 3D: raycaster, sprites e arma (horizonte em `y = 40`). |

- **Limpeza em uma única operação sem flicker:** a cada frame, `memset(display_buf + 256, 0, 768)` zera exclusivamente as páginas 2–7 (viewport 3D), preservando integralmente os 256 bytes do HUD amarelo. Elementos estáticos e vitais só são redesenhados quando mudam (`hud_update_vitals()`), e o fade-in do gameplay atua apenas em `[16, 64)` (`fadeScreen(..., onlyViewport = true)`).
- **Ganho de preenchimento vertical:** reduzir a área de rasterização para 48 px (`RENDER_HEIGHT = 48`) diminui em **25%** o preenchimento vertical em relação à tela cheia de 64 px (e ~14% em relação aos 56 px originais), enquanto o clipping estrito em `drawPixel()`, `drawVLine()`, `drawSprite()` e `drawBitmapViewport()` garante que nenhum pixel 3D vaze para a faixa amarela.

### 6.2. Z-buffer de 16 bits por Coluna e Linha de Visão em $O(1)$
No `doom-nano` original (2 KB SRAM), o Z-buffer tinha apenas `uint8_t zbuffer[64]` e testava oclusão uma única vez na borda esquerda do sprite (além de um bug de indexação que nunca lia a metade direita do array, overflow de `int8_t` em `x`, falta de reset por frame e clamp mínimo de parede em `1.0`).

Aproveitando os **8 KB de SRAM** do ATmega2560, o Z-buffer foi reconstruído como **`uint16_t zbuffer[128]`** (**256 bytes**, escala ponto fixo 8.8 `Z_SCALE = 256`, resolução de `~0,0039` bloco):
- **Reset determinístico por frame:** `clearZBuffer()` preenche todas as 128 entradas com `0xFFFF` no início de `renderMap()`, eliminando valores residuais em raios sem colisão.
- **Escrita completa por coluna e `WALL_MIN_DIST = 0.15`:** cada raio grava sua distância em todas as colunas cobertas por `RES_DIVIDER`, e paredes muito próximas não sofrem mais clamping artificial em `1.0` nem overflow de `line_height` (`int16_t`).
- **Oclusão parcial por coluna em quinas (`drawSprite()`):** o laço externo percorre colunas de tela `screen_x = x + tx` (com coordenadas `int16_t` e interpolação de textura DDA em ponto fixo 8.8, sem divisões por pixel) e descarta qualquer coluna onde `sprite_z >= zbuffer[screen_x]` antes de ler a Flash (`PROGMEM`). Assim, inimigos e itens são recortados com precisão de 1 pixel atrás de quinas à esquerda e à direita.
- **Bloqueio de tiro através de paredes em $O(1)$ (`fire()`):** o disparo pendente é resolvido imediatamente após `renderMap()`, garantindo que `zbuffer` reflita a câmera exata do frame atual. Antes de aplicar dano a um inimigo na mira, compara-se `encodeZDistance(transform.y) >= zbuffer[screen_x]` em tempo constante $O(1)$, bloqueando instantaneamente tiros contra inimigos ocultos atrás de paredes sólidas.

---

## 7. Minimapa 2D, Persistência por Bitmask, FSM Completa e Áudio (Fase 5)

### 7.1. Minimapa 2D / Radar em Tempo Real (`minimap.h` e `minimap.cpp`)
O radar 2D é alternado sem travar o gameplay pelo clique único do joystick direito (`PIN_JOY_R_SW`, D24) e sinalizado pelo indicador `MAP` no HUD amarelo (`x = 89..102, y = 1..6`):
- **Fonte única de verdade (zero duplicação de mapa):** `minimap_render()` lê diretamente a mesma matriz compactada de 4 bits/bloco em `PROGMEM` (`sto_level_1`) usada pelo raycaster, através da função compartilhada `level_get_block()` em `level.h`.
- **Projeção centrada no jogador (top-down, Norte para cima):** ocupa a janela `x = 82..125, y = 18..61` (`44 × 44 px` com moldura 1 px; grade interna de `20 × 20` células em escala `2 × 2 px/bloco` → `x = 84..123, y = 20..59`), deixando a metade esquerda do viewport 3D azul livre.
- **Coordenadas relativas:** cada célula $(m_x, m_y)$ ao redor de $(\lfloor p_x \rfloor, \lfloor p_y \rfloor)$ com $\Delta x, \Delta y \in [-10, +9]$ é projetada em:
  $$s_x = 84 + (\Delta x + 10) \times 2, \qquad s_y = 20 + (9 - \Delta y) \times 2$$
  Paredes (`E_WALL`) são desenhadas como blocos 2×2; `E_EXIT` pisca a cada 180 ms; inimigos ativos piscam em padrão diagonal; itens coletáveis aparecem como pontos 1×1; o jogador ocupa o centro `(104, 38)` com um ponteiro de 3 passos orientado por $(+d_x, -d_y)$, coerente com a rotação da câmera.

### 7.2. Bitmask de Persistência na SRAM: Trade-off Nano vs. Mega
- **Limitação no ATmega328P (2 KB SRAM):** no `doom-nano` original, o array `static_entity[28]` não chegou a ser finalizado; entidades removidas ao sair do raio `MAX_ENTITY_DISTANCE` reapareciam intactas ao retornar (inimigos ressuscitavam e itens podiam ser coletados infinitamente).
- **Solução no ATmega2560 (8 KB SRAM):** um **bitmask de 1 bit por célula** cobre integralmente a grade `64 × 57 = 3648` células em exatos **456 bytes** (`cleared_cells[456]`):
  - `bit_index = y * 64 + x` → byte `bit_index >> 3`, bit `1 << (bit_index & 7)`.
  - Tanto a consulta (`isCellCleared()`) quanto a marcação (`markCellCleared()`) executam em tempo constante **$O(1)$** (em vez de busca linear $O(N)$ em vetor limitado).
  - Como o `UID` de 16 bits preserva as coordenadas originais de spawn (`uid_get_x()`, `uid_get_y()`), mesmo um inimigo que caminhou antes de morrer marca sua célula de origem como limpa, impedindo qualquer respawn na partida.

### 7.3. Máquina de Estados Finitos (FSM) e Áudio Completo em `PROGMEM`
- **FSM em `doom-mega.ino`:** ciclo completo entre 4 cenas com transição limpa por fade-out de tela inteira:
  `INTRO` → `GAME_PLAY` → `GAME_OVER` (queda da câmera ao zerar `player.health`) ou `VICTORY` (ao alcançar o bloco `E_EXIT`, renderizado no 3D com padrão zebrado animado via `drawExitVLine()`).
- **Telas Dual-Color:** `GAME_OVER` e `VICTORY` exibem o título em destaque na faixa amarela (`y = 0..15`) e as estatísticas da partida (`KILLS`, `KEYS`, `TIME` em segundos + prompt `PRESS FIRE OR R-SW`) na faixa azul (`y = 16..63`), reiniciando diretamente para `GAME_PLAY` com `resetLevelState()`.
- **Expansão de áudio em `PROGMEM` com prioridade (`sound.h`/`.cpp`):** adicionados 6 efeitos no formato *Inverse Frequency Sound* (`enemy_hit_snd`, `player_hurt_snd`, `fireball_snd`, `ui_click_snd`, `game_over_snd`, `victory_snd`), todos mantidos em Flash (`PROGMEM`) muito abaixo do limite de 64 KB. `playSound()` recebe nível de prioridade (`SND_PRIO_LOW` a `SND_PRIO_CRITICAL`), impedindo que passos ou batidas na parede interrompam efeitos críticos, e `hit_wall_snd` passou a tocar apenas em colisões do jogador.



