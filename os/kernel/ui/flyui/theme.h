#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t win_bg;
    uint32_t win_title_active_bg;
    uint32_t win_title_active_fg;
    uint32_t win_title_inactive_bg;
    uint32_t win_title_inactive_fg;
    
    uint32_t color_hi_1; /* White */
    uint32_t color_lo_1; /* Dark Gray */
    uint32_t color_lo_2; /* Black */
    uint32_t color_text;
    
    int border_width;
    int title_height;
    int padding;
} fly_theme_t;

void theme_init(void);
fly_theme_t* theme_get(void);

#ifdef __cplusplus
}
#endif