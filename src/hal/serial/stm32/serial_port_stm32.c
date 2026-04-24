/*
 *  serial_port_stm32.c
 *
 *  STM32 UART implementation of lib60870 serial HAL.
 */

#include <string.h>

#include "hal_serial.h"
#include "hal_serial_stm32.h"
#include "lib_memory.h"

struct sSerialPort
{
    char interfaceName[32];
    int baudRate;
    uint8_t dataBits;
    char parity;
    uint8_t stopBits;

    uint32_t timeoutMs;
    SerialPortError lastError;
    bool isOpen;

    UART_HandleTypeDef* uartHandle;
};

SerialPort
SerialPort_create(const char* interfaceName, int baudRate, uint8_t dataBits, char parity, uint8_t stopBits)
{
    SerialPort self = (SerialPort) GLOBAL_CALLOC(1, sizeof(struct sSerialPort));

    if (self == NULL)
        return NULL;

    if (interfaceName != NULL)
        (void) strncpy(self->interfaceName, interfaceName, sizeof(self->interfaceName) - 1U);

    self->baudRate = baudRate;
    self->dataBits = dataBits;
    self->parity = parity;
    self->stopBits = stopBits;
    self->timeoutMs = 100U;
    self->lastError = SERIAL_PORT_ERROR_NONE;
    self->isOpen = false;
    self->uartHandle = NULL;

    return self;
}

void
SerialPort_destroy(SerialPort self)
{
    if (self == NULL)
        return;

    SerialPort_close(self);
    GLOBAL_FREEMEM(self);
}

void
SerialPort_setUartHandle(SerialPort self, UART_HandleTypeDef* uartHandle)
{
    if (self == NULL)
        return;

    self->uartHandle = uartHandle;
}

UART_HandleTypeDef*
SerialPort_getUartHandle(SerialPort self)
{
    if (self == NULL)
        return NULL;

    return self->uartHandle;
}

bool
SerialPort_open(SerialPort self)
{
    if (self == NULL)
        return false;

    if (self->uartHandle == NULL)
    {
        self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;
        return false;
    }

    self->lastError = SERIAL_PORT_ERROR_NONE;
    self->isOpen = true;

    return true;
}

void
SerialPort_close(SerialPort self)
{
    if (self == NULL)
        return;

    self->isOpen = false;
}

int
SerialPort_getBaudRate(SerialPort self)
{
    if (self == NULL)
        return 0;

    return self->baudRate;
}

void
SerialPort_setTimeout(SerialPort self, int timeout)
{
    if (self == NULL)
        return;

    if (timeout < 0)
        timeout = 0;

    self->timeoutMs = (uint32_t) timeout;
}

void
SerialPort_discardInBuffer(SerialPort self)
{
    if ((self == NULL) || (self->uartHandle == NULL))
        return;

    __HAL_UART_FLUSH_DRREGISTER(self->uartHandle);
}

int
SerialPort_readByte(SerialPort self)
{
    if ((self == NULL) || !self->isOpen || (self->uartHandle == NULL))
    {
        if (self != NULL)
            self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;

        return -1;
    }

    uint8_t value = 0;

    HAL_StatusTypeDef status = HAL_UART_Receive(self->uartHandle,
                                                &value,
                                                1U,
                                                self->timeoutMs);

    if (status == HAL_OK)
    {
        self->lastError = SERIAL_PORT_ERROR_NONE;
        return (int) value;
    }

    if (status == HAL_TIMEOUT)
    {
        self->lastError = SERIAL_PORT_ERROR_NONE;
        return -1;
    }

    self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
    return -1;
}

int
SerialPort_write(SerialPort self, uint8_t* buffer, int startPos, int numberOfBytes)
{
    if ((self == NULL) || !self->isOpen || (self->uartHandle == NULL) || (buffer == NULL) ||
        (startPos < 0) || (numberOfBytes < 0))
    {
        if (self != NULL)
            self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;

        return -1;
    }

    HAL_StatusTypeDef status = HAL_UART_Transmit(self->uartHandle,
                                                 &buffer[startPos],
                                                 (uint16_t) numberOfBytes,
                                                 self->timeoutMs);

    if (status == HAL_OK)
    {
        self->lastError = SERIAL_PORT_ERROR_NONE;
        return numberOfBytes;
    }

    self->lastError = (status == HAL_TIMEOUT) ? SERIAL_PORT_ERROR_OPEN_FAILED : SERIAL_PORT_ERROR_UNKNOWN;
    return -1;
}

SerialPortError
SerialPort_getLastError(SerialPort self)
{
    if (self == NULL)
        return SERIAL_PORT_ERROR_INVALID_ARGUMENT;

    return self->lastError;
}
