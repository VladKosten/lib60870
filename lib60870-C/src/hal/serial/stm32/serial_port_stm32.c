/**
 * \file      serial_port_stm32.c
 * \brief     STM32 HAL implementation of the lib60870 SerialPort interface.
 * \details   Wraps STM32 HAL UART (HAL_UART_Receive / HAL_UART_Transmit) for use
 *            with the CS 101 serial link layer.
 *            The UART peripheral is assumed to be already initialised by CubeMX-generated
 *            code (MX_USARTx_UART_Init).  Call SerialPort_setUartHandle() after
 *            SerialPort_create() to bind the desired UART_HandleTypeDef* before calling
 *            SerialPort_open().
 */

/*
 *  Copyright 2025 - STM32/FreeRTOS port
 *
 *  Part of lib60870-C HAL layer for STM32 microcontrollers.
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

//===============================================================================[ INCLUDE ]================================================================================

#include "lib_memory.h"
#include "hal_serial.h"
#include "stm32f4xx_hal.h"

//====================================================================[ STRUCT DEFINITION ]================================================================================

struct sSerialPort
{
    UART_HandleTypeDef* huart;      /**< STM32 HAL UART handle (set via SerialPort_setUartHandle) */
    uint32_t timeoutMs;             /**< Per-byte receive timeout in ms (default 100 ms) */
    SerialPortError lastError;      /**< Last recorded error code */
};

//====================================================================[ PUBLIC INTERFACE FUNCTIONS ]========================================================================

/**
 * \brief Create a new SerialPort instance.
 * \note  interfaceName, baudRate, dataBits, parity and stopBits are ignored –
 *        the UART is configured by CubeMX-generated HAL init code.
 *        After creation call SerialPort_setUartHandle() to bind the UART handle.
 */
SerialPort SerialPort_create(const char* interfaceName,
                  int baudRate,
                  uint8_t dataBits,
                  char parity,
                  uint8_t stopBits)
{
    (void) interfaceName;
    (void) baudRate;
    (void) dataBits;
    (void) parity;
    (void) stopBits;

    SerialPort self = (SerialPort) GLOBAL_MALLOC(sizeof(struct sSerialPort));

    if (self != NULL)
    {
        self->huart     = NULL;
        self->timeoutMs = 100U;    /* 100 ms default – lib60870 overrides via SerialPort_setTimeout */
        self->lastError = SERIAL_PORT_ERROR_NONE;
    }

    return self;
}

/**
 * \brief Bind an STM32 UART handle to the SerialPort instance.
 * \note  Must be called before SerialPort_open().
 * \param[in] self  SerialPort instance.
 * \param[in] huart Pointer to the HAL UART handle (e.g. &huart6).
 */
void
SerialPort_setUartHandle(SerialPort self, UART_HandleTypeDef* huart)
{
    if (self != NULL)
    {
        self->huart = huart;
    }
}

/**
 * \brief Destroy the SerialPort instance and free memory.
 */
void
SerialPort_destroy(SerialPort self)
{
    if (self != NULL)
    {
        GLOBAL_FREEMEM(self);
    }
}

/**
 * \brief Open the serial interface.
 * \details  No hardware initialisation is performed here – the UART is already
 *           initialised by CubeMX code.  This function just validates that a UART
 *           handle has been bound via SerialPort_setUartHandle().
 * \return true when handle is valid, false otherwise.
 */
bool
SerialPort_open(SerialPort self)
{
    if (self == NULL)
    {
        return false;
    }

    if (self->huart == NULL)
    {
        self->lastError = SERIAL_PORT_ERROR_OPEN_FAILED;
        return false;
    }

    self->lastError = SERIAL_PORT_ERROR_NONE;
    return true;
}

/**
 * \brief Close the serial interface.
 * \note  No hardware operation is performed – the UART lifecycle is managed externally.
 */
void
SerialPort_close(SerialPort self)
{
    /* UART lifecycle managed by application – nothing to do */
    (void) self;
}

/**
 * \brief Return the baud rate of the underlying UART.
 */
int
SerialPort_getBaudRate(SerialPort self)
{
    if ((self == NULL) || (self->huart == NULL))
    {
        return 0;
    }

    return (int) self->huart->Init.BaudRate;
}

/**
 * \brief Discard all pending bytes in the UART receive buffer.
 */
void
SerialPort_discardInBuffer(SerialPort self)
{
    if ((self == NULL) || (self->huart == NULL))
    {
        return;
    }

    /* Drain the UART DR/FIFO with zero-timeout reads */
    uint8_t dummy = 0U;
    while (HAL_UART_Receive(self->huart, &dummy, 1U, 0U) == HAL_OK)
    {
        /* keep reading until empty */
    }

    /* Clear any pending data-register-not-empty flag */
    __HAL_UART_FLUSH_DRREGISTER(self->huart);
}

/**
 * \brief Set the per-byte receive timeout.
 * \param[in] timeout timeout in milliseconds.
 */
void
SerialPort_setTimeout(SerialPort self, int timeout)
{
    if (self != NULL)
    {
        self->timeoutMs = (uint32_t) timeout;
    }
}

/**
 * \brief Return the last error code.
 */
SerialPortError
SerialPort_getLastError(SerialPort self)
{
    if (self == NULL)
    {
        return SERIAL_PORT_ERROR_UNKNOWN;
    }

    return self->lastError;
}

/**
 * \brief Read one byte from the UART.
 * \return the received byte value (0–255) or -1 on timeout / error.
 */
int
SerialPort_readByte(SerialPort self)
{
    if ((self == NULL) || (self->huart == NULL))
    {
        return -1;
    }

    uint8_t byte = 0U;

    self->lastError = SERIAL_PORT_ERROR_NONE;

    HAL_StatusTypeDef status = HAL_UART_Receive(self->huart, &byte, 1U, self->timeoutMs);

    if (status == HAL_OK)
    {
        return (int) byte;
    }

    /* HAL_TIMEOUT is normal – link layer polls for incoming data */
    return -1;
}

/**
 * \brief Write bytes to the UART.
 * \param[in] buffer        source buffer.
 * \param[in] startPos      start offset within buffer.
 * \param[in] numberOfBytes number of bytes to transmit.
 * \return number of bytes written, or -1 on error.
 */
int
SerialPort_write(SerialPort self, uint8_t* buffer, int startPos, int numberOfBytes)
{
    if ((self == NULL) || (self->huart == NULL) || (buffer == NULL))
    {
        return -1;
    }

    self->lastError = SERIAL_PORT_ERROR_NONE;

    HAL_StatusTypeDef status = HAL_UART_Transmit(self->huart,
                                                 buffer + startPos,
                                                 (uint16_t) numberOfBytes,
                                                 1000U);

    if (status == HAL_OK)
    {
        return numberOfBytes;
    }

    self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
    return -1;
}
