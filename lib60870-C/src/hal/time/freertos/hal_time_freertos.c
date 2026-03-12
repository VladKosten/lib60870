/**
 * \file      hal_time_freertos.c
 * \brief     FreeRTOS implementation of the lib60870 HAL time interface.
 * \details   Maps Hal_getTimeInMs / Hal_getMonotonicTimeInMs to the FreeRTOS
 *            tick counter.
 *
 *            NOTE: The time values returned by Hal_getTimeInMs() represent
 *            milliseconds since MCU boot, NOT since the Unix epoch.
 *            For IEC 60870-5-101 clock-synchronisation the application must
 *            maintain an epoch offset independently (e.g. via the
 *            CS101_ClockSynchronizationHandler).
 */

/*
 *  Copyright 2025 - STM32/FreeRTOS port
 *
 *  Part of lib60870-C HAL layer for FreeRTOS.
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

//===============================================================================[ INCLUDE]================================================================================

#include "FreeRTOS.h"
#include "hal_time.h"
#include "task.h"


//====================================================================[ PUBLIC INTERFACE FUNCTIONS
//]========================================================================

/**
 * \brief Return the system time as milliseconds since MCU boot.
 * \note  On an embedded target without a real-time clock this is the FreeRTOS tick
 *        count converted to milliseconds.  The value does NOT represent Unix epoch
 *        time.  If epoch-accurate time is required the application must maintain an
 *        offset that is updated via the clock-synchronisation handler.
 * \return milliseconds since scheduler start (wraps after ~49.7 days at 1 ms tick).
 */
msSinceEpoch
Hal_getTimeInMs(void)
{
    return (msSinceEpoch)xTaskGetTickCount() * (msSinceEpoch)portTICK_PERIOD_MS;
}

/**
 * \brief Return the system time as nanoseconds.
 * \note  Resolution is limited to the FreeRTOS tick period (typically 1 ms).
 */
nsSinceEpoch
Hal_getTimeInNs(void)
{
    return Hal_getTimeInMs() * 1000000ULL;
}

/**
 * \brief Set the system time from a nanosecond timestamp.
 * \note  Not supported on this target – the FreeRTOS tick counter cannot be
 *        adjusted at runtime.  Returns false always.
 */
bool
Hal_setTimeInNs(nsSinceEpoch nsTime)
{
    (void)nsTime;
    return false;
}

/**
 * \brief Return the monotonic system time in milliseconds.
 * \details Identical to Hal_getTimeInMs() on FreeRTOS – both use the tick count.
 */
msSinceEpoch
Hal_getMonotonicTimeInMs(void)
{
    return (msSinceEpoch)xTaskGetTickCount() * (msSinceEpoch)portTICK_PERIOD_MS;
}
