/**
 * \file TWI_Master.h
 *
 * \brief AVR TWI master interrupt-driven driver (derived from Atmel AVR315).
 *
 * Copyright (C) 2014-2015 Atmel Corporation. All rights reserved.
 */

#ifndef TWI_MASTER_H
#define TWI_MASTER_H

#include <stdint.h>
#include "pinout.h"

/****************************************************************************
  TWI Status/Control register definitions
****************************************************************************/
constexpr uint16_t TWI_BUFFER_SIZE = 344; // Largest message size including address and control bytes
constexpr uint16_t TWI_TIMEOUT_MS  = 20;  // Timeout de seguranca antes de acionar I2C Bus Recovery
#define TWI_FREQ 400000UL
#define TWI_ADDR ((uint8_t)((OLED_I2C_ADDR) << 1)) // 8-bit SLA+W address (0x78 for 0x3C)

/****************************************************************************
  Function definitions
****************************************************************************/
void TWI_Master_Initialise(void);
unsigned char TWI_Transceiver_Busy(void);
unsigned char TWI_Get_State_Info(void);
void TWI_Start_Transceiver_With_Data(uint8_t cmd, unsigned char *msg, uint16_t msgSize);
void TWI_Start_Transceiver(void);
unsigned char TWI_Get_Data_From_Transceiver(unsigned char *msg, unsigned char msgSize);
uint16_t TWI_Get_Recovery_Count(void);

/****************************************************************************
  Bit and byte definitions
****************************************************************************/
#define TWI_READ_BIT  0 // Bit position for R/W bit in "address byte".
#define TWI_ADR_BITS  1 // Bit position for LSB of the slave address bits in the init byte.

#ifndef TRUE
#define TRUE          1
#endif
#ifndef FALSE
#define FALSE         0
#endif

/****************************************************************************
  TWI State codes
****************************************************************************/
// General TWI Master status codes
#define TWI_START                  0x08 // START has been transmitted
#define TWI_REP_START              0x10 // Repeated START has been transmitted
#define TWI_ARB_LOST               0x38 // Arbitration lost

// TWI Master Transmitter status codes
#define TWI_MTX_ADR_ACK            0x18 // SLA+W has been transmitted and ACK received
#define TWI_MTX_ADR_NACK           0x20 // SLA+W has been transmitted and NACK received
#define TWI_MTX_DATA_ACK           0x28 // Data byte has been transmitted and ACK received
#define TWI_MTX_DATA_NACK          0x30 // Data byte has been transmitted and NACK received

// TWI Master Receiver status codes
#define TWI_MRX_ADR_ACK            0x40 // SLA+R has been transmitted and ACK received
#define TWI_MRX_ADR_NACK           0x48 // SLA+R has been transmitted and NACK received
#define TWI_MRX_DATA_ACK           0x50 // Data byte has been received and ACK transmitted
#define TWI_MRX_DATA_NACK          0x58 // Data byte has been received and NACK transmitted

// TWI Slave Transmitter status codes
#define TWI_STX_ADR_ACK            0xA8 // Own SLA+R has been received; ACK has been returned
#define TWI_STX_ADR_ACK_M_ARB_LOST 0xB0 // Arbitration lost in SLA+R/W as Master; own SLA+R received; ACK returned
#define TWI_STX_DATA_ACK           0xB8 // Data byte in TWDR has been transmitted; ACK has been received
#define TWI_STX_DATA_NACK          0xC0 // Data byte in TWDR has been transmitted; NOT ACK has been received
#define TWI_STX_DATA_ACK_LAST_BYTE 0xC8 // Last data byte in TWDR has been transmitted (TWEA = '0'); ACK received

// TWI Slave Receiver status codes
#define TWI_SRX_ADR_ACK            0x60 // Own SLA+W has been received; ACK has been returned
#define TWI_SRX_ADR_ACK_M_ARB_LOST 0x68 // Arbitration lost in SLA+R/W as Master; own SLA+W received; ACK returned
#define TWI_SRX_GEN_ACK            0x70 // General call address has been received; ACK has been returned
#define TWI_SRX_GEN_ACK_M_ARB_LOST 0x78 // Arbitration lost in SLA+R/W as Master; General call received; ACK returned
#define TWI_SRX_ADR_DATA_ACK       0x80 // Previously addressed with own SLA+W; data received; ACK returned
#define TWI_SRX_ADR_DATA_NACK      0x88 // Previously addressed with own SLA+W; data received; NOT ACK returned
#define TWI_SRX_GEN_DATA_ACK       0x90 // Previously addressed with general call; data received; ACK returned
#define TWI_SRX_GEN_DATA_NACK      0x98 // Previously addressed with general call; data received; NOT ACK returned
#define TWI_SRX_STOP_RESTART       0xA0 // STOP or repeated START received while still addressed as Slave

// TWI Miscellaneous status codes
#define TWI_NO_STATE               0xF8 // No relevant state information available; TWINT = '0'
#define TWI_BUS_ERROR              0x00 // Bus error due to an illegal START or STOP condition

#endif // TWI_MASTER_H
