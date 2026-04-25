/*
 *  serial_port_stm32.c
 *
 *  STM32 UART implementation of lib60870 serial HAL.
 */

#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "hal_serial.h"
#include "hal_serial_stm32.h"
#include "lib_memory.h"

#define SERIAL_PORT_RX_QUEUE_LENGTH    256U
#define SERIAL_PORT_MAX_INSTANCES      1U
#define SERIAL_PORT_RX_DMA_BUFFER_SIZE 512U

struct sSerialPort
{
    char interfaceName [32];
    int baudRate;
    uint8_t dataBits;
    char parity;
    uint8_t stopBits;

    uint32_t timeoutMs;
    SerialPortError lastError;
    bool isOpen;

    UART_HandleTypeDef* uartHandle;

    QueueHandle_t rxQueue;
    uint8_t rxDmaBuffer [SERIAL_PORT_RX_DMA_BUFFER_SIZE];
    uint16_t rxDmaBufferPos;

    SemaphoreHandle_t txDoneSem;
};

static SerialPort g_serialPortInstances [SERIAL_PORT_MAX_INSTANCES] = {0};

static bool serialPortRegisterInstance(SerialPort self);
static void serialPortUnregisterInstance(SerialPort self);
static SerialPort serialPortFindByHandle(UART_HandleTypeDef* uartHandle);
static bool serialPortApplyUartConfig(SerialPort self);
static bool serialPortStartRxInterrupt(SerialPort self);
static TickType_t serialPortTimeoutToTicks(uint32_t timeoutMs);

static bool serialPortRegisterInstance(SerialPort self)
{
    bool registered = false;

    taskENTER_CRITICAL();
    for (uint32_t i = 0U; i < SERIAL_PORT_MAX_INSTANCES; i++)
    {
        if (g_serialPortInstances [i] == self)
        {
            registered = true;
            break;
        }

        if (g_serialPortInstances [i] == NULL)
        {
            g_serialPortInstances [i] = self;
            registered = true;
            break;
        }
    }
    taskEXIT_CRITICAL();

    return registered;
}

static void
serialPortUnregisterInstance(SerialPort self)
{
    taskENTER_CRITICAL();
    for (uint32_t i = 0U; i < SERIAL_PORT_MAX_INSTANCES; i++)
    {
        if (g_serialPortInstances [i] == self)
        {
            g_serialPortInstances [i] = NULL;
            break;
        }
    }
    taskEXIT_CRITICAL();
}

static SerialPort
serialPortFindByHandle(UART_HandleTypeDef* uartHandle)
{
    if (uartHandle == NULL)
        return NULL;

    for (uint32_t i = 0U; i < SERIAL_PORT_MAX_INSTANCES; i++)
    {
        SerialPort instance = g_serialPortInstances [i];

        if ((instance != NULL) && (instance->uartHandle == uartHandle))
            return instance;
    }

    return NULL;
}

static bool
serialPortApplyUartConfig(SerialPort self)
{
    if ((self == NULL) || (self->uartHandle == NULL))
        return false;

    /* De-initialize UART first to ensure clean state and proper DMA setup */
    if (self->uartHandle->gState != HAL_UART_STATE_RESET)
    {
        if (HAL_UART_DeInit(self->uartHandle) != HAL_OK)
        {
            self->lastError = SERIAL_PORT_ERROR_OPEN_FAILED;
            return false;
        }
    }

    uint32_t wordLength = UART_WORDLENGTH_8B;
    uint32_t parity = UART_PARITY_NONE;
    uint32_t stopBits = UART_STOPBITS_1;

    switch (self->parity)
    {
        case 'N' :
        case 'n' :
            parity = UART_PARITY_NONE;
            wordLength = (self->dataBits == 9U) ? UART_WORDLENGTH_9B : UART_WORDLENGTH_8B;
            break;

        case 'E' :
        case 'e' :
            parity = UART_PARITY_EVEN;
            wordLength = (self->dataBits >= 8U) ? UART_WORDLENGTH_9B : UART_WORDLENGTH_8B;
            break;

        case 'O' :
        case 'o' :
            parity = UART_PARITY_ODD;
            wordLength = (self->dataBits >= 8U) ? UART_WORDLENGTH_9B : UART_WORDLENGTH_8B;
            break;

        default :
            self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;
            return false;
    }

    switch (self->stopBits)
    {
        case 1U :
            stopBits = UART_STOPBITS_1;
            break;

        case 2U :
            stopBits = UART_STOPBITS_2;
            break;

        default :
            self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;
            return false;
    }

    self->uartHandle->Init.BaudRate = (uint32_t) self->baudRate;
    self->uartHandle->Init.WordLength = wordLength;
    self->uartHandle->Init.StopBits = stopBits;
    self->uartHandle->Init.Parity = parity;
    self->uartHandle->Init.Mode = UART_MODE_TX_RX;
    self->uartHandle->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    self->uartHandle->Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(self->uartHandle) != HAL_OK)
    {
        self->lastError = SERIAL_PORT_ERROR_OPEN_FAILED;
        return false;
    }

    return true;
}

static bool
serialPortStartRxInterrupt(SerialPort self)
{
    if ((self == NULL) || (self->uartHandle == NULL))
        return false;

    /* Clear any pending UART errors and flush RX buffer before starting DMA reception */
    __HAL_UART_CLEAR_PEFLAG(self->uartHandle);
    __HAL_UART_CLEAR_FEFLAG(self->uartHandle);
    __HAL_UART_CLEAR_NEFLAG(self->uartHandle);
    __HAL_UART_CLEAR_OREFLAG(self->uartHandle);
    __HAL_UART_FLUSH_DRREGISTER(self->uartHandle);

    /* Enable IDLE interrupt */
    __HAL_UART_ENABLE_IT(self->uartHandle, UART_IT_IDLE);

    /* Start DMA reception in circular mode */
    HAL_StatusTypeDef status = HAL_UART_Receive_DMA(self->uartHandle,
                                                    self->rxDmaBuffer,
                                                    SERIAL_PORT_RX_DMA_BUFFER_SIZE);
    if (status == HAL_OK)
        return true;

    if (status == HAL_BUSY)
        return true;

    self->lastError = SERIAL_PORT_ERROR_OPEN_FAILED;
    return false;
}

static TickType_t
serialPortTimeoutToTicks(uint32_t timeoutMs)
{
    if (timeoutMs == 0U)
        return 0U;

    TickType_t ticks = pdMS_TO_TICKS(timeoutMs);
    return (ticks == 0U) ? 1U : ticks;
}

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
    self->rxDmaBufferPos = 0U;
    self->rxQueue = xQueueCreate(SERIAL_PORT_RX_QUEUE_LENGTH, sizeof(uint8_t));

    if (self->rxQueue == NULL)
    {
        GLOBAL_FREEMEM(self);
        return NULL;
    }

    self->txDoneSem = xSemaphoreCreateBinary();

    if (self->txDoneSem == NULL)
    {
        vQueueDelete(self->rxQueue);
        GLOBAL_FREEMEM(self);
        return NULL;
    }

    return self;
}

void SerialPort_destroy(SerialPort self)
{
    if (self == NULL)
        return;

    SerialPort_close(self);

    if (self->rxQueue != NULL)
        vQueueDelete(self->rxQueue);

    if (self->txDoneSem != NULL)
        vSemaphoreDelete(self->txDoneSem);

    GLOBAL_FREEMEM(self);
}

void SerialPort_setUartHandle(SerialPort self, UART_HandleTypeDef* uartHandle)
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

bool SerialPort_open(SerialPort self)
{
    if (self == NULL)
        return false;

    if (self->uartHandle == NULL)
    {
        self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;
        return false;
    }

    if (!serialPortApplyUartConfig(self))
        return false;

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    /* Register UART callbacks when callback registration is enabled */
    if (HAL_UART_RegisterCallback(self->uartHandle, HAL_UART_TX_COMPLETE_CB_ID, (pUART_CallbackTypeDef) SerialPort_TxCpltCallback) != HAL_OK)
    {
        self->lastError = SERIAL_PORT_ERROR_OPEN_FAILED;
        return false;
    }

    if (HAL_UART_RegisterCallback(self->uartHandle, HAL_UART_ERROR_CB_ID, (pUART_CallbackTypeDef) SerialPort_ErrorCallback) != HAL_OK)
    {
        self->lastError = SERIAL_PORT_ERROR_OPEN_FAILED;
        return false;
    }
#endif

    if (!serialPortRegisterInstance(self))
    {
        self->lastError = SERIAL_PORT_ERROR_OPEN_FAILED;
        return false;
    }

    if (self->rxQueue != NULL)
        xQueueReset(self->rxQueue);

    self->isOpen = true;

    if (!serialPortStartRxInterrupt(self))
    {
        self->isOpen = false;
        serialPortUnregisterInstance(self);
        return false;
    }

    self->lastError = SERIAL_PORT_ERROR_NONE;
    return true;
}

void SerialPort_close(SerialPort self)
{
    if (self == NULL)
        return;

    self->isOpen = false;
    serialPortUnregisterInstance(self);

    if (self->uartHandle != NULL)
    {
        __HAL_UART_DISABLE_IT(self->uartHandle, UART_IT_IDLE);
        (void) HAL_UART_AbortReceive(self->uartHandle);
    }
}

int SerialPort_getBaudRate(SerialPort self)
{
    if (self == NULL)
        return 0;

    return self->baudRate;
}

void SerialPort_setTimeout(SerialPort self, int timeout)
{
    if (self == NULL)
        return;

    if (timeout < 0)
        timeout = 0;

    self->timeoutMs = (uint32_t) timeout;
}

void SerialPort_discardInBuffer(SerialPort self)
{
    if ((self == NULL) || (self->uartHandle == NULL))
        return;

    if (self->rxQueue != NULL)
        xQueueReset(self->rxQueue);

    __HAL_UART_FLUSH_DRREGISTER(self->uartHandle);

    taskENTER_CRITICAL();
    self->rxDmaBufferPos = 0U;
    taskEXIT_CRITICAL();

    if (self->isOpen)
        (void) serialPortStartRxInterrupt(self);
}

int SerialPort_readByte(SerialPort self)
{
    if ((self == NULL) || !self->isOpen || (self->uartHandle == NULL))
    {
        if (self != NULL)
            self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;

        return -1;
    }

    if (self->rxQueue == NULL)
    {
        self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;
        return -1;
    }

    uint8_t value = 0U;
    BaseType_t received = xQueueReceive(self->rxQueue,
                                        &value,
                                        serialPortTimeoutToTicks(self->timeoutMs));

    if (received == pdPASS)
    {
        self->lastError = SERIAL_PORT_ERROR_NONE;
        return (int) value;
    }

    self->lastError = SERIAL_PORT_ERROR_NONE;
    return -1;
}

int SerialPort_write(SerialPort self, uint8_t* buffer, int startPos, int numberOfBytes)
{
    if ((self == NULL) || !self->isOpen || (self->uartHandle == NULL) || (buffer == NULL) ||
        (startPos < 0) || (numberOfBytes < 0))
    {
        if (self != NULL)
            self->lastError = SERIAL_PORT_ERROR_INVALID_ARGUMENT;

        return -1;
    }

    /* Clear any stale «done» token before starting TX */
    (void) xSemaphoreTake(self->txDoneSem, 0);

    HAL_StatusTypeDef status = HAL_UART_Transmit_IT(self->uartHandle,
                                                    &buffer [startPos],
                                                    (uint16_t) numberOfBytes);

    if (status != HAL_OK)
    {
        self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
        return -1;
    }

    /* Wait for TxCpltCallback to signal completion */
    TickType_t ticks = serialPortTimeoutToTicks(self->timeoutMs);
    if (xSemaphoreTake(self->txDoneSem, ticks) != pdPASS)
    {
        (void) HAL_UART_AbortTransmit(self->uartHandle);
        self->lastError = SERIAL_PORT_ERROR_OPEN_FAILED;
        return -1;
    }

    self->lastError = SERIAL_PORT_ERROR_NONE;
    return numberOfBytes;
}

SerialPortError
SerialPort_getLastError(SerialPort self)
{
    if (self == NULL)
        return SERIAL_PORT_ERROR_INVALID_ARGUMENT;

    return self->lastError;
}

void SerialPort_TxCpltCallback(UART_HandleTypeDef* huart)
{
    SerialPort self = serialPortFindByHandle(huart);

    if (self == NULL)
        return;

    BaseType_t higherPriorityTaskWoken = pdFALSE;

    if (self->txDoneSem != NULL)
        xSemaphoreGiveFromISR(self->txDoneSem, &higherPriorityTaskWoken);

    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

/**
 * \brief UART IDLE interrupt callback - called when line goes idle (end of frame).
 * \details This function extracts received data from DMA buffer and pushes it to the queue.
 *          Only complete frames are made available for reading.
 */
void SerialPort_IdleCallback(UART_HandleTypeDef* huart)
{
    SerialPort self = serialPortFindByHandle(huart);

    if ((self == NULL) || !self->isOpen || (self->rxQueue == NULL))
        return;

    /* Clear IDLE flag */
    __HAL_UART_CLEAR_IDLEFLAG(huart);

    /* Calculate how many bytes were received */
    uint16_t dmaCounter = (uint16_t) __HAL_DMA_GET_COUNTER(huart->hdmarx);
    uint16_t currentPos = SERIAL_PORT_RX_DMA_BUFFER_SIZE - dmaCounter;
    uint16_t receivedBytes = 0U;

    if (currentPos >= self->rxDmaBufferPos)
    {
        receivedBytes = currentPos - self->rxDmaBufferPos;
    }
    else
    {
        /* Buffer wrapped around */
        receivedBytes = (SERIAL_PORT_RX_DMA_BUFFER_SIZE - self->rxDmaBufferPos) + currentPos;
    }

    if (receivedBytes == 0U)
        return;

    /* Push received frame bytes to the queue */
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    for (uint16_t i = 0U; i < receivedBytes; i++)
    {
        uint16_t idx = (self->rxDmaBufferPos + i) % SERIAL_PORT_RX_DMA_BUFFER_SIZE;

        if (xQueueSendFromISR(self->rxQueue, &self->rxDmaBuffer [idx], &higherPriorityTaskWoken) != pdPASS)
        {
            self->lastError = SERIAL_PORT_ERROR_UNKNOWN;
            break;
        }
    }

    /* Update buffer position */
    self->rxDmaBufferPos = currentPos;

    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void SerialPort_ErrorCallback(UART_HandleTypeDef* huart)
{
    SerialPort self = serialPortFindByHandle(huart);

    if (self == NULL)
        return;

    self->lastError = SERIAL_PORT_ERROR_UNKNOWN;

    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);

    if (self->isOpen)
    {
        self->rxDmaBufferPos = 0U;
        (void) HAL_UART_Receive_DMA(self->uartHandle, self->rxDmaBuffer, SERIAL_PORT_RX_DMA_BUFFER_SIZE);
    }
}
