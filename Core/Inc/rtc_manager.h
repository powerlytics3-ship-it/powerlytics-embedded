#pragma once

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTC_BKP_MAGIC     0x1238U   // your existing magic
#define RTC_CLK_MAGIC     0xBEEFU   // new: prevents backup reset every boot

int  rtc_clock_configure_with_fallback(void);
void rtc_init_with_bkp_once(void);
void rtc_get_datetime_strings(char *ts_buf, int ts_buf_sz);


#ifdef __cplusplus
}
#endif
