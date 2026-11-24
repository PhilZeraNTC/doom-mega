/*
 * doom-mega — Sketch de Diagnostico de Hardware (Arduino Mega 2560)
 *
 * Valida em bancada sem bibliotecas externas:
 *   1. Barramento I2C (SDA = D20, SCL = D21) via I2C Scanner
 *   2. Display OLED SSD1306 128x64 Dual-Color (16 px amarelo + 48 px azul)
 *   3. Buzzer passivo no pino D11 (Timer1 OC1A / PB5 no Mega 2560)
 *   4. 2x Joysticks KY-023 (A0/A1/D22 e A2/A3/D24) + Botao de tiro (D26)
 */

#include <Arduino.h>
#include <Wire.h>

// Pinagem oficial do contrato (espelhando doom-mega/pinout.h)
constexpr uint8_t PIN_JOY_L_X  = A0;
constexpr uint8_t PIN_JOY_L_Y  = A1;
constexpr uint8_t PIN_JOY_L_SW = 22;

constexpr uint8_t PIN_JOY_R_X  = A2;
constexpr uint8_t PIN_JOY_R_Y  = A3;
constexpr uint8_t PIN_JOY_R_SW = 24;

constexpr uint8_t PIN_BTN_FIRE = 26;
constexpr uint8_t PIN_BUZZER   = 11;

constexpr uint32_t PRINT_INTERVAL_MS = 200;
constexpr uint32_t DEBOUNCE_MS       = 30;

static uint32_t lastPrintTime = 0;
static bool     lastFirePressed = false;
static uint32_t lastFireChangeTime = 0;

static void oledSendCommandList(uint8_t addr, const uint8_t *cmds, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(0x00);
  for (uint8_t i = 0; i < len; i++) {
    Wire.write(cmds[i]);
  }
  Wire.endTransmission();
}

static void oledInit(uint8_t addr) {
  static const uint8_t initSeq1[] = {
    0xAE,       // Display OFF
    0xD5, 0x80, // Clock divide ratio
    0xA8, 0x3F, // Multiplex ratio (64 - 1)
    0xD3, 0x00, // Display offset = 0
    0x40,       // Start line = 0
    0x8D, 0x14, // Enable internal charge pump
    0x20, 0x00, // Horizontal addressing mode
    0xA1,       // Segment remap (col 127 mapped to SEG0)
    0xC8        // COM output scan direction (remapped)
  };
  static const uint8_t initSeq2[] = {
    0xDA, 0x12, // COM pins hardware config (128x64)
    0x81, 0xCF, // Contrast
    0xD9, 0xF1, // Pre-charge period
    0xDB, 0x40, // VCOMH deselect level
    0xA4,       // Resume to RAM content
    0xA6,       // Normal display (not inverted)
    0x2E,       // Deactivate scroll
    0xAF        // Display ON
  };

  oledSendCommandList(addr, initSeq1, sizeof(initSeq1));
  oledSendCommandList(addr, initSeq2, sizeof(initSeq2));
}

// Gera byte vertical (8 pixels) para (page 0..7, col 0..127):
//   - Pages 0..1 (y = 0..15, faixa AMARELA): borda externa + ticks + divisoria central
//   - Pages 2..7 (y = 16..63, faixa AZUL): retangulo externo + grade interna 16x16 px
static uint8_t makeTestByte(uint8_t page, uint8_t col) {
  if (page == 0) {
    // y = 0..7 (metade superior da faixa amarela)
    if (col == 0 || col == 127) return 0xFF;
    uint8_t b = 0x01; // Borda superior (y = 0)
    if (col == 63 || col == 64) b |= 0xF0; // Marcador central
    else if ((col % 16) == 0)   b |= 0x07; // Ticks superiores a cada 16 px
    return b;
  }

  if (page == 1) {
    // y = 8..15 (metade inferior da faixa amarela)
    if (col == 0 || col == 127) return 0xFF;
    uint8_t b = 0x80; // Borda inferior da faixa amarela (y = 15)
    if (col == 63 || col == 64) b |= 0x0F; // Marcador central
    else if ((col % 16) == 0)   b |= 0xE0; // Ticks inferiores a cada 16 px
    return b;
  }

  // Pages 2..7 -> y = 16..63 (faixa azul: 48 px = 6 paginas)
  if (col == 0 || col == 127) return 0xFF; // Bordas laterais do retangulo azul
  if ((col % 16) == 0)        return 0xFF; // Linhas verticais da grade (a cada 16 px)

  uint8_t b = 0x00;
  if (page == 2) b |= 0x01; // Borda superior da faixa azul (y = 16)
  if (page == 4) b |= 0x01; // Linha horizontal da grade (y = 32)
  if (page == 6) b |= 0x01; // Linha horizontal da grade (y = 48)
  if (page == 7) b |= 0x80; // Borda inferior da tela (y = 63)
  return b;
}

static void oledDrawDualColorPattern(uint8_t addr) {
  static const uint8_t addrWindow[] = {
    0x21, 0x00, 0x7F, // Column address: 0..127
    0x22, 0x00, 0x07  // Page address:   0..7
  };
  oledSendCommandList(addr, addrWindow, sizeof(addrWindow));

  // Envia 16 bytes por transacao Wire (seguro dentro do buffer de 32 bytes do Wire.h)
  for (uint8_t page = 0; page < 8; page++) {
    for (uint8_t colBase = 0; colBase < 128; colBase += 16) {
      Wire.beginTransmission(addr);
      Wire.write(0x40); // Control byte: Co = 0, D/C = 1 (GDDRAM data)
      for (uint8_t i = 0; i < 16; i++) {
        Wire.write(makeTestByte(page, colBase + i));
      }
      Wire.endTransmission();
    }
  }
}

static uint8_t scanI2C() {
  Serial.println(F("\n[1/3] Executando I2C Scanner nos pinos D20 (SDA) e D21 (SCL)..."));

  uint8_t foundCount = 0;
  uint8_t oledAddr = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      foundCount++;
      Serial.print(F("  -> Dispositivo I2C encontrado em 0x"));
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);

      if (addr == 0x3C || addr == 0x3D) {
        oledAddr = addr;
      }
    } else if (error == 4) {
      Serial.print(F("  -> Erro desconhecido no endereco 0x"));
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
    }
  }

  if (foundCount == 0) {
    Serial.println(F("  [AVISO] Nenhum dispositivo I2C respondeu nos pinos D20 (SDA) / D21 (SCL)!"));
    Serial.println(F("          Verifique VCC (5V/3.3V), GND e se SDA/SCL estao em D20/D21 do Mega 2560."));
  } else if (oledAddr == 0) {
    Serial.println(F("  [AVISO] Nenhum OLED SSD1306 detectado em 0x3C ou 0x3D."));
  } else {
    Serial.print(F("  -> OLED SSD1306 detectado em 0x"));
    Serial.println(oledAddr, HEX);
  }

  return oledAddr;
}

static void buzzerStartupBeep() {
  Serial.println(F("[3/3] Testando buzzer passivo no pino D11 (3 bips)..."));
  const uint16_t freqs[3] = { 880, 1175, 1760 };
  for (uint8_t i = 0; i < 3; i++) {
    tone(PIN_BUZZER, freqs[i], 100);
    delay(160);
  }
  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println(F("============================================================"));
  Serial.println(F("  doom-mega — Diagnostico de Hardware (Arduino Mega 2560)"));
  Serial.println(F("============================================================"));

  // Configuracao de entradas digitais e buzzer
  pinMode(PIN_JOY_L_SW, INPUT_PULLUP);
  pinMode(PIN_JOY_R_SW, INPUT_PULLUP);
  pinMode(PIN_BTN_FIRE, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // 1) I2C Scanner
  Wire.begin();
  Wire.setClock(100000UL);
  uint8_t oledAddr = scanI2C();

  // 2) Teste visual do SSD1306 Dual-Color
  if (oledAddr != 0) {
    Serial.println(F("[2/3] Inicializando SSD1306 via Wire e desenhando padrao dual-color..."));
    Wire.setClock(400000UL);
    oledInit(oledAddr);
    oledDrawDualColorPattern(oledAddr);
    Serial.println(F("  -> Padrao desenhado: borda amarela (y=0..15) + grade azul (y=16..63)."));
  } else {
    Serial.println(F("[2/3] Pulando inicializacao do OLED (endereco 0x3C/0x3D nao encontrado)."));
  }

  // 3) Teste de audio no D11
  buzzerStartupBeep();

  Serial.println(F("\nIniciando leitura continua dos controles (a cada 200 ms):"));
  Serial.println(F("  (Botoes em INPUT_PULLUP: 1 = solto, 0 = pressionado; BTN_FIRE gera bip)\n"));
}

void loop() {
  uint32_t now = millis();

  // Monitora BTN_FIRE (D26) com debounce simples para gerar bip curto ao pressionar
  bool firePressed = (digitalRead(PIN_BTN_FIRE) == LOW);
  if (firePressed != lastFirePressed && (now - lastFireChangeTime) >= DEBOUNCE_MS) {
    lastFireChangeTime = now;
    lastFirePressed = firePressed;
    if (firePressed) {
      tone(PIN_BUZZER, 1200, 60);
    }
  }

  // Impressao periodica dos 2 joysticks + botao de tiro
  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = now;

    int l_x = analogRead(PIN_JOY_L_X);
    int l_y = analogRead(PIN_JOY_L_Y);
    int l_sw = digitalRead(PIN_JOY_L_SW);

    int r_x = analogRead(PIN_JOY_R_X);
    int r_y = analogRead(PIN_JOY_R_Y);
    int r_sw = digitalRead(PIN_JOY_R_SW);

    int btn_fire = digitalRead(PIN_BTN_FIRE);

    Serial.print(F("L_X: "));       Serial.print(l_x);
    Serial.print(F(" | L_Y: "));    Serial.print(l_y);
    Serial.print(F(" | L_SW: "));   Serial.print(l_sw);
    Serial.print(F(" || R_X: "));   Serial.print(r_x);
    Serial.print(F(" | R_Y: "));    Serial.print(r_y);
    Serial.print(F(" | R_SW: "));   Serial.print(r_sw);
    Serial.print(F(" || BTN_FIRE: "));
    Serial.println(btn_fire);
  }
}
