/**
 * \file      hal_serial_stm32.h
 * \brief     STM32-specific extension to the lib60870 SerialPort HAL.
 * \details   Declares the extra initialisation function that binds a
 *            UART_HandleTypeDef* to an existing SerialPort instance.
 *            Include this header together with "hal_serial.h" when using
 *            the STM32 serial port HAL implementation.
 */

#ifndef HAL_SERIAL_STM32_H_
#define HAL_SERIAL_STM32_H_

#include "hal_serial.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief Bind an STM32 UART handle to the SerialPort instance.
 *
 * Must be called after SerialPort_create() and before SerialPort_open().
 * The UART peripheral must already have been initialised by HAL_UART_Init()
 * (done automatically by CubeMX-generated code).
 *
 * \param[in] self  SerialPort instance created with SerialPort_create().
 * \param[in] huart Pointer to the STM32 HAL UART handle (e.g. &huart6).
 */
void
SerialPort_setUartHandle(SerialPort self, UART_HandleTypeDef* huart);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SERIAL_STM32_H_ */
