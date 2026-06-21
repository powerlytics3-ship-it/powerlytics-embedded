#include "rtc_manager.h"
#include "stm32f4xx_hal.h"
#include "debug_uart.h"
#include <stdio.h>

extern RTC_HandleTypeDef hrtc;

// Use the magic defined in your header to avoid redefinition warnings
#ifndef RTC_BKP_MAGIC
#define RTC_BKP_MAGIC 0x1239U
#endif

static void enable_backup_domain_access(void) {
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
}

static int rtc_clock_source_selected(void) {
    return (RCC->BDCR & RCC_BDCR_RTCSEL) != 0;
}

int rtc_clock_configure_with_fallback(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};
    enable_backup_domain_access();

    /* 1. Reset Backup Domain ONLY if stuck on LSI (0x02) */
    if ((RCC->BDCR & RCC_BDCR_RTCSEL) == RCC_BDCR_RTCSEL_1) {
        print("[RTC] LSI detected. Resetting Backup Domain for LSE switch...\r\n");
        __HAL_RCC_BACKUPRESET_FORCE();
        __HAL_RCC_BACKUPRESET_RELEASE();
        enable_backup_domain_access(); 
    }

    if (rtc_clock_source_selected()) {
        __HAL_RCC_RTC_ENABLE();
        return HAL_OK;
    }

    /* 2. Attempt LSE initialization */
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    osc.LSEState       = RCC_LSE_ON; 
    if (HAL_RCC_OscConfig(&osc) == HAL_OK) {
        pclk.PeriphClockSelection = RCC_PERIPHCLK_RTC;
        pclk.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
        if (HAL_RCCEx_PeriphCLKConfig(&pclk) == HAL_OK) {
            __HAL_RCC_RTC_ENABLE();
            return HAL_OK;
        }
    }

    /* 3. Emergency Fallback to LSI (Will not keep time on battery) */
    print("[RTC][ERR] LSE failed. Using LSI.\r\n");
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    osc.LSIState       = RCC_LSI_ON;
    HAL_RCC_OscConfig(&osc);
    pclk.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    HAL_RCCEx_PeriphCLKConfig(&pclk);
    __HAL_RCC_RTC_ENABLE();
    return HAL_ERROR;
}

void rtc_init_with_bkp_once(void) {
    enable_backup_domain_access();
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    if (HAL_RTC_Init(&hrtc) != HAL_OK) return;

    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) == RTC_BKP_MAGIC) {
        print("[RTC] Time preserved from battery.\r\n");
        return;
    }

    print("[RTC] First-time init: Setting default time.\r\n");
    RTC_TimeTypeDef t = {20, 14, 0, RTC_DAYLIGHTSAVING_NONE, RTC_STOREOPERATION_RESET};
    RTC_DateTypeDef d = {RTC_WEEKDAY_FRIDAY, RTC_MONTH_JUNE, 19, 26};
    HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_BKP_MAGIC);
}

void rtc_get_datetime_strings(char *ts_buf, int ts_buf_sz) {
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN); 
    snprintf(ts_buf, (size_t)ts_buf_sz, "20%02d-%02d-%02dT%02d:%02d:%02dZ",
             d.Year, d.Month, d.Date, t.Hours, t.Minutes, t.Seconds);

}
