/*
 *  hal_time_stm32.c
 *
 *  STM32 HAL RTC-based time adapter for lib60870 HAL.
 */

#include <stdint.h>
#include "hal_time.h"
#include "stm32f4xx_hal.h"
#include "rtc.h"

static bool
isLeapYear(uint32_t year)
{
    return (((year % 4U) == 0U) && (((year % 100U) != 0U) || ((year % 400U) == 0U)));
}

static uint32_t
daysInMonth(uint32_t year, uint32_t month)
{
    static const uint8_t monthDays [12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if ((month == 2U) && isLeapYear(year))
        return 29U;

    return monthDays [month - 1U];
}

static uint64_t
dateTimeToUnixMs(uint32_t year,
                 uint32_t month,
                 uint32_t day,
                 uint32_t hour,
                 uint32_t minute,
                 uint32_t second)
{
    uint64_t days = 0;

    for (uint32_t y = 1970U; y < year; y++)
        days += isLeapYear(y) ? 366U : 365U;

    for (uint32_t m = 1U; m < month; m++)
        days += daysInMonth(year, m);

    days += (uint64_t) (day - 1U);

    uint64_t totalSeconds = days * 86400ULL;
    totalSeconds += (uint64_t) hour * 3600ULL;
    totalSeconds += (uint64_t) minute * 60ULL;
    totalSeconds += (uint64_t) second;

    return totalSeconds * 1000ULL;
}

static void
unixSecondsToDateTime(uint64_t unixSeconds,
                      uint32_t* year,
                      uint32_t* month,
                      uint32_t* day,
                      uint32_t* hour,
                      uint32_t* minute,
                      uint32_t* second,
                      uint32_t* weekday)
{
    uint64_t days = unixSeconds / 86400ULL;
    uint32_t secOfDay = (uint32_t) (unixSeconds % 86400ULL);

    *hour = secOfDay / 3600U;
    secOfDay %= 3600U;
    *minute = secOfDay / 60U;
    *second = secOfDay % 60U;
    uint32_t y = 1970U;

    while (true)
    {
        uint32_t yearDays = isLeapYear(y) ? 366U : 365U;

        if (days < yearDays)
            break;

        days -= yearDays;
        y++;
    }

    uint32_t m = 1U;

    while (true)
    {
        uint32_t dim = daysInMonth(y, m);

        if (days < dim)
            break;

        days -= dim;
        m++;
    }

    *year = y;
    *month = m;
    *day = (uint32_t) days + 1U;

    /* RTC weekday: 1=Monday ... 7=Sunday; 1970-01-01 was Thursday (=4) */
    *weekday = (uint32_t) (((unixSeconds / 86400ULL + 3ULL) % 7ULL) + 1ULL);
}

static uint64_t
getMonotonicMs(void)
{
    return (uint64_t) HAL_GetTick();
}

msSinceEpoch
Hal_getTimeInMs(void)
{
    RTC_TimeTypeDef rtcTime = {0};
    RTC_DateTypeDef rtcDate = {0};

    if (HAL_RTC_GetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN) != HAL_OK)
        return 0;

    if (HAL_RTC_GetDate(&hrtc, &rtcDate, RTC_FORMAT_BIN) != HAL_OK)
        return 0;

    uint32_t year = 2000U + (uint32_t) rtcDate.Year;

    return (msSinceEpoch) dateTimeToUnixMs(year,
                                           (uint32_t) rtcDate.Month,
                                           (uint32_t) rtcDate.Date,
                                           (uint32_t) rtcTime.Hours,
                                           (uint32_t) rtcTime.Minutes,
                                           (uint32_t) rtcTime.Seconds);
}

nsSinceEpoch
Hal_getTimeInNs(void)
{
    return Hal_getTimeInMs() * 1000000ULL;
}

bool Hal_setTimeInNs(nsSinceEpoch nsTime)
{
    uint64_t unixSeconds = nsTime / 1000000000ULL;

    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    uint32_t hour = 0;
    uint32_t minute = 0;
    uint32_t second = 0;
    uint32_t weekday = 0;

    unixSecondsToDateTime(unixSeconds,
                          &year,
                          &month,
                          &day,
                          &hour,
                          &minute,
                          &second,
                          &weekday);

    if ((year < 2000U) || (year > 2099U))
        return false;

    RTC_TimeTypeDef rtcTime = {0};
    RTC_DateTypeDef rtcDate = {0};

    rtcTime.Hours = (uint8_t) hour;
    rtcTime.Minutes = (uint8_t) minute;
    rtcTime.Seconds = (uint8_t) second;
    rtcTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    rtcTime.StoreOperation = RTC_STOREOPERATION_RESET;

    rtcDate.Year = (uint8_t) (year - 2000U);
    rtcDate.Month = (uint8_t) month;
    rtcDate.Date = (uint8_t) day;
    rtcDate.WeekDay = (uint8_t) weekday;

    if (HAL_RTC_SetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN) != HAL_OK)
        return false;

    if (HAL_RTC_SetDate(&hrtc, &rtcDate, RTC_FORMAT_BIN) != HAL_OK)
        return false;

    return true;
}

msSinceEpoch
Hal_getMonotonicTimeInMs(void)
{
    return (msSinceEpoch) getMonotonicMs();
}

nsSinceEpoch
Hal_getMonotonicTimeInNs(void)
{
    return Hal_getMonotonicTimeInMs() * 1000000ULL;
}
