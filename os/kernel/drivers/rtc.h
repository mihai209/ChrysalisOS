#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rtc_time {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

void rtc_read(struct rtc_time* t);
void rtc_print();

#ifdef __cplusplus
}
#endif
