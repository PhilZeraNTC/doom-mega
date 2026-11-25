/**
 * \file TWI_Master.cpp
 *
 * \brief AVR TWI master interrupt-driven driver (derived from Atmel AVR315),
 *        hardened with TWSTO completion check, datasheet-compliant error
 *        handling, timeout guard, and 9-clock I2C bus recovery.
 *
 * Copyright (C) 2014-2015 Atmel Corporation. All rights reserved.
 */

#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "constants.h"
#include "TWI_Master.h"

static unsigned char TWI_buf[TWI_BUFFER_SIZE];           // Transceiver buffer
static volatile uint16_t TWI_msgSize;                    // Number of bytes to be transmitted
static volatile unsigned char TWI_state = TWI_NO_STATE;  // State byte
static volatile unsigned char lastTransOK;
static uint16_t twi_recovery_count = 0;

#if DEBUG_FREEZE || DEBUG_TWI_RECOVERY
static void twiPrintHexByte(uint8_t v)
{
  if (v < 16) Serial.print('0');
  Serial.print(v, HEX);
}
#endif

/****************************************************************************
  I2C Bus Recovery (9 pulsos em SCL + STOP manual + reinicializacao TWI).
  Libera escravo preso segurando SDA=LOW ou estado travado apos glitch fisico.
****************************************************************************/
static void twiRecoverBus(uint8_t prev_twsr, uint8_t prev_twcr)
{
  twi_recovery_count++;

#if DEBUG_TWI_RECOVERY || DEBUG_FREEZE
  Serial.print(F("\n[TWI_RECOVER] count="));
  Serial.print(twi_recovery_count);
  Serial.print(F(" TWSR=0x"));
  twiPrintHexByte(prev_twsr);
  Serial.print(F(" TWCR=0x"));
  twiPrintHexByte(prev_twcr);
  Serial.print(F(" state=0x"));
  twiPrintHexByte(TWI_state);
  Serial.println();
  Serial.flush();
#else
  (void)prev_twsr;
  (void)prev_twcr;
#endif

  // 1) Desabilita o periferico TWI para controlar os pinos via GPIO
  TWCR = 0;

  // 2) SDA (D20) como INPUT_PULLUP e SCL (D21) como OUTPUT em HIGH
  pinMode(PIN_TWI_SDA, INPUT_PULLUP);
  digitalWrite(PIN_TWI_SCL, HIGH);
  pinMode(PIN_TWI_SCL, OUTPUT);
  delayMicroseconds(5);

  // Gera ate 9 pulsos de clock em SCL enquanto SDA permanecer preso em LOW
  for (uint8_t i = 0; i < 9; i++) {
    if (digitalRead(PIN_TWI_SDA) == HIGH) {
      break;
    }
    digitalWrite(PIN_TWI_SCL, LOW);
    delayMicroseconds(5);
    digitalWrite(PIN_TWI_SCL, HIGH);
    delayMicroseconds(5);
  }

  // 3) Condicao manual de STOP: SDA sobe (LOW -> HIGH) com SCL em HIGH
  digitalWrite(PIN_TWI_SCL, LOW);
  delayMicroseconds(5);
  pinMode(PIN_TWI_SDA, OUTPUT);
  digitalWrite(PIN_TWI_SDA, LOW);
  delayMicroseconds(5);
  digitalWrite(PIN_TWI_SCL, HIGH);
  delayMicroseconds(5);
  digitalWrite(PIN_TWI_SDA, HIGH);
  delayMicroseconds(5);

  // 4) Libera os pinos para o hardware TWI e reinicializa o controlador
  pinMode(PIN_TWI_SDA, INPUT_PULLUP);
  pinMode(PIN_TWI_SCL, INPUT_PULLUP);
  TWI_state   = TWI_NO_STATE;
  lastTransOK = FALSE;
  TWI_Master_Initialise();
}

/****************************************************************************
  Aguarda conclusao da transacao anterior (TWIE==0) e do STOP fisico (TWSTO==0)
  com timeout de seguranca (TWI_TIMEOUT_MS). Retorna false se precisou recuperar.
****************************************************************************/
static bool twiWaitReady(void)
{
  uint32_t start_ms = millis();
  uint32_t spins    = 0;

  while (TWI_Transceiver_Busy()) {
    spins++;
    if (((millis() - start_ms) >= TWI_TIMEOUT_MS) || (spins >= 60000UL)) {
      uint8_t twsr_val = TWSR;
      uint8_t twcr_val = TWCR;
      twiRecoverBus(twsr_val, twcr_val);
      return false;
    }
  }
  return true;
}

/****************************************************************************
  Set up the TWI master to its initial standby state (400 kHz @ 16 MHz).
****************************************************************************/
void TWI_Master_Initialise(void)
{
  TWSR = 0;                                          // Prescaler = 1 (TWPS1:0 = 00)
  TWBR = ((F_CPU / TWI_FREQ) - 16) / 2;              // Bit rate register (12 for 400 kHz @ 16 MHz)
  TWDR = 0xFF;                                       // Default content = SDA released
  TWCR = (1 << TWEN) |                               // Enable TWI-interface and release TWI pins
         (0 << TWIE) | (0 << TWINT) |                // Disable Interrupt
         (0 << TWEA) | (0 << TWSTA) | (0 << TWSTO) | // No signal requests
         (0 << TWWC);
}

/****************************************************************************
  Test if the TWI_ISR is busy transmitting OR hardware STOP is still active.
****************************************************************************/
unsigned char TWI_Transceiver_Busy(void)
{
  return (TWCR & ((1 << TWIE) | (1 << TWSTO)));
}

/****************************************************************************
  Wait for completion and return TWI state info of the previous operation.
****************************************************************************/
unsigned char TWI_Get_State_Info(void)
{
  (void)twiWaitReady();
  return TWI_state;
}

uint16_t TWI_Get_Recovery_Count(void)
{
  return twi_recovery_count;
}

/****************************************************************************
  Prepare and send a message to the configured OLED I2C address (TWI_ADDR).
****************************************************************************/
void TWI_Start_Transceiver_With_Data(uint8_t cmd, unsigned char *msg, uint16_t msgSize)
{
  if (!twiWaitReady()) {
    // Barramento recuperado apos timeout: aborta apenas esta transacao sem travar a CPU
    return;
  }

  if (msgSize > (TWI_BUFFER_SIZE - 2)) {
    msgSize = TWI_BUFFER_SIZE - 2;
  }

  TWI_msgSize = msgSize + 2; // Address + control byte + payload
  TWI_buf[0]  = TWI_ADDR;    // Slave address with W bit (0)
  TWI_buf[1]  = cmd;

  for (uint16_t i = 2; msgSize--; i++) {
    TWI_buf[i] = *msg++;
  }

  // Garante folga minima t_BUF (>= 1.3 us em I2C 400 kHz) entre STOP anterior e novo START
  delayMicroseconds(2);

  lastTransOK = 0;
  TWI_state   = TWI_NO_STATE;
  TWCR = (1 << TWEN) |                               // TWI Interface enabled
         (1 << TWIE) | (1 << TWINT) |                // Enable TWI Interrupt and clear the flag
         (0 << TWEA) | (1 << TWSTA) | (0 << TWSTO) | // Initiate a START condition
         (0 << TWWC);
}

/****************************************************************************
  Resend the last message already buffered in TWI_buf.
****************************************************************************/
void TWI_Start_Transceiver(void)
{
  if (!twiWaitReady()) {
    return;
  }

  delayMicroseconds(2);

  lastTransOK = 0;
  TWI_state   = TWI_NO_STATE;
  TWCR = (1 << TWEN) |
         (1 << TWIE) | (1 << TWINT) |
         (0 << TWEA) | (1 << TWSTA) | (0 << TWSTO) |
         (0 << TWWC);
}

/****************************************************************************
  Read out requested data from the TWI transceiver buffer after completion.
****************************************************************************/
unsigned char TWI_Get_Data_From_Transceiver(unsigned char *msg, unsigned char msgSize)
{
  if (!twiWaitReady()) {
    return FALSE;
  }

  if (lastTransOK) {
    for (unsigned char i = 0; i < msgSize; i++) {
      msg[i] = TWI_buf[i];
    }
  }
  return lastTransOK;
}

// ********** Interrupt Handlers ********** //
ISR(TWI_vect)
{
  static uint16_t TWI_bufPtr;

  switch (TWSR & 0xF8)
  {
    case TWI_START:             // START has been transmitted
    case TWI_REP_START:         // Repeated START has been transmitted
      TWI_bufPtr = 0;           // Set buffer pointer to the TWI Address location
      /* fall through */
    case TWI_MTX_ADR_ACK:       // SLA+W has been transmitted and ACK received
    case TWI_MTX_DATA_ACK:      // Data byte has been transmitted and ACK received
      if (TWI_bufPtr < TWI_msgSize) {
        TWDR = TWI_buf[TWI_bufPtr++];
        TWCR = (1 << TWEN) |
               (1 << TWIE) | (1 << TWINT) |
               (0 << TWEA) | (0 << TWSTA) | (0 << TWSTO) |
               (0 << TWWC);
      } else {                  // Send STOP after last byte
        lastTransOK = TRUE;
        TWCR = (1 << TWEN) |
               (0 << TWIE) | (1 << TWINT) |
               (0 << TWEA) | (0 << TWSTA) | (1 << TWSTO) |
               (0 << TWWC);
      }
      break;

    case TWI_MRX_DATA_ACK:      // Data byte has been received and ACK transmitted
      TWI_buf[TWI_bufPtr++] = TWDR;
      /* fall through */
    case TWI_MRX_ADR_ACK:       // SLA+R has been transmitted and ACK received
      if (TWI_bufPtr < (TWI_msgSize - 1)) { // Detect the last byte to NACK it
        TWCR = (1 << TWEN) |
               (1 << TWIE) | (1 << TWINT) |
               (1 << TWEA) | (0 << TWSTA) | (0 << TWSTO) |
               (0 << TWWC);
      } else {                  // Send NACK after next reception
        TWCR = (1 << TWEN) |
               (1 << TWIE) | (1 << TWINT) |
               (0 << TWEA) | (0 << TWSTA) | (0 << TWSTO) |
               (0 << TWWC);
      }
      break;

    case TWI_MRX_DATA_NACK:     // Data byte has been received and NACK transmitted
      TWI_buf[TWI_bufPtr] = TWDR;
      lastTransOK = TRUE;
      TWCR = (1 << TWEN) |
             (0 << TWIE) | (1 << TWINT) |
             (0 << TWEA) | (0 << TWSTA) | (1 << TWSTO) |
             (0 << TWWC);
      break;

    case TWI_ARB_LOST:          // Arbitration lost (0x38)
    case TWI_MTX_ADR_NACK:      // SLA+W has been transmitted and NACK received (0x20)
    case TWI_MRX_ADR_NACK:      // SLA+R has been transmitted and NACK received (0x48)
    case TWI_MTX_DATA_NACK:     // Data byte has been transmitted and NACK received (0x30)
    case TWI_BUS_ERROR:         // Bus error due to an illegal START or STOP condition (0x00)
    default:
      // Conforme datasheet do ATmega2560: limpa TWINT escrevendo 1, emite STOP (TWSTO=1)
      // para liberar o periferico/escravo e desabilita TWIE para nao travar o loop principal.
      TWI_state   = TWSR & 0xF8;
      lastTransOK = FALSE;
      TWCR = (1 << TWEN) |
             (0 << TWIE) | (1 << TWINT) |
             (0 << TWEA) | (0 << TWSTA) | (1 << TWSTO) |
             (0 << TWWC);
      break;
  }
}
