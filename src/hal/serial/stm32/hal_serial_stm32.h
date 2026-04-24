/*
 *  hal_serial_stm32.h
 *
 *  STM32 extension API for lib60870 serial HAL adapter.
 */

#ifndef HAL_SERIAL_STM32_H_
#define HAL_SERIAL_STM32_H_

#include "hal_serial.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

PAL_API void
SerialPort_setUartHandle(SerialPort self, UART_HandleTypeDef* uartHandle);

PAL_API UART_HandleTypeDef*
SerialPort_getUartHandle(SerialPort self);

/* HAL UART callback dispatchers - call these from the global HAL_UART_*Callback functions */
PAL_API void SerialPort_TxCpltCallback(UART_HandleTypeDef* huart);
PAL_API void SerialPort_RxCpltCallback(UART_HandleTypeDef* huart);
PAL_API void SerialPort_ErrorCallback(UART_HandleTypeDef* huart);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SERIAL_STM32_H_ */
