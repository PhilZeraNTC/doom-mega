/**
 * \file TWI_Master.cpp
 *
 * \brief AVR TWI master interrupt-driven driver (derived from Atmel AVR315).
 *
 * Copyright (C) 2014-2015 Atmel Corporation. All rights reserved.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "constants.h"
#include "TWI_Master.h"

#if DEBUG_FREEZE
#include <Arduino.h>
#endif

static unsigned char TWI_buf[TWI_BUFFER_SIZE];       // Transceiver buffer
static volatile uint16_t TWI_msgSize;                // Number of bytes to be transmitted
static volatile unsigned char TWI_state = TWI_NO_STATE; // State byte
static volatile unsigned char lastTransOK;

#if DEBUG_FREEZE
static volatile uint16_t TWI_dbgBufPtr    = 0;
static volatile uint8_t  TWI_dbgLastTwsr  = TWI_NO_STATE;
static volatile uint32_t TWI_dbgIsrCount  = 0;

static void twiPrintHexByte(uint8_t v)
{
  if (v < 16) Serial.print('0');
  Serial.print(v, HEX);
}
#endif

static void twiWaitReady(void)
{
#if DEBUG_FREEZE
  uint32_t start_ms = millis();
  uint32_t spins    = 0;
  bool     reported = false;

  while (TWI_Transceiver_Busy()) {
    spins++;
    if (!reported && (((millis() - start_ms) >= 50UL) || (spins >= 100000UL))) {
      reported = true;
      uint8_t twsr_val  = TWSR;
      uint8_t twcr_val  = TWCR;
      uint8_t state_val = TWI_state;
      uint8_t sreg_val  = SREG;

      Serial.print(F("\n[TWI_HANG] TWSR=0x"));
      twiPrintHexByte(twsr_val);
      Serial.print(F(" TWCR=0x"));
      twiPrintHexByte(twcr_val);
      Serial.print(F(" state=0x"));
      twiPrintHexByte(state_val);
      Serial.print(F(" lastISR=0x"));
      twiPrintHexByte(TWI_dbgLastTwsr);
      Serial.print(F(" ptr="));
      Serial.print(TWI_dbgBufPtr);
      Serial.print('/');
      Serial.print(TWI_msgSize);
      Serial.print(F(" isrCnt="));
      Serial.print(TWI_dbgIsrCount);
      Serial.print(F(" SREG=0x"));
      twiPrintHexByte(sreg_val);
      Serial.println();
      Serial.flush();
    }
  }
#else
  while (TWI_Transceiver_Busy());
#endif
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
  Test if the TWI_ISR is busy transmitting.
****************************************************************************/
unsigned char TWI_Transceiver_Busy(void)
{
  return (TWCR & (1 << TWIE)); // If TWI Interrupt is enabled then the transceiver is busy
}

/****************************************************************************
  Wait for completion and return TWI state info of the previous operation.
****************************************************************************/
unsigned char TWI_Get_State_Info(void)
{
  twiWaitReady(); // Wait until TWI has completed the transmission
  return TWI_state;
}

/****************************************************************************
  Prepare and send a message to the configured OLED I2C address (TWI_ADDR).
****************************************************************************/
void TWI_Start_Transceiver_With_Data(uint8_t cmd, unsigned char *msg, uint16_t msgSize)
{
  twiWaitReady(); // Wait until TWI is ready for next transmission

  if (msgSize > (TWI_BUFFER_SIZE - 2)) {
    msgSize = TWI_BUFFER_SIZE - 2;
  }

  TWI_msgSize = msgSize + 2; // Address + control byte + payload
  TWI_buf[0]  = TWI_ADDR;    // Slave address with W bit (0)
  TWI_buf[1]  = cmd;

  for (uint16_t i = 2; msgSize--; i++) {
    TWI_buf[i] = *msg++;
  }

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
  twiWaitReady();
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
  twiWaitReady();

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

#if DEBUG_FREEZE
  TWI_dbgIsrCount++;
  TWI_dbgLastTwsr = TWSR & 0xF8;
  TWI_dbgBufPtr   = TWI_bufPtr;
#endif

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

    case TWI_ARB_LOST:          // Arbitration lost
      TWCR = (1 << TWEN) |
             (1 << TWIE) | (1 << TWINT) |
             (0 << TWEA) | (1 << TWSTA) | (0 << TWSTO) |
             (0 << TWWC);
      break;

    case TWI_MTX_ADR_NACK:      // SLA+W has been transmitted and NACK received
    case TWI_MRX_ADR_NACK:      // SLA+R has been transmitted and NACK received
    case TWI_MTX_DATA_NACK:     // Data byte has been transmitted and NACK received
    case TWI_BUS_ERROR:         // Bus error due to an illegal START or STOP condition
    default:
      TWI_state = TWSR & 0xF8;  // Store masked TWSR status code
      TWCR = (1 << TWEN) |      // Reset TWI interface and release pins
             (0 << TWIE) | (0 << TWINT) |
             (0 << TWEA) | (0 << TWSTA) | (0 << TWSTO) |
             (0 << TWWC);
      break;
  }
}
