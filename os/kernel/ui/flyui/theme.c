#include "theme.h"

static fly_theme_t current_theme;

void theme_init(void) {
    /* Classic / Serenity-inspired Silver Theme */
    current_theme.win_bg = 0xFFC0C0C0;
    
    current_theme.win_title_active_bg = 0xFF000080; /* Navy Blue */
    current_theme.win_title_active_fg = 0xFFFFFFFF;
    
    current_theme.win_title_inactive_bg = 0xFF808080; /* Dark Gray */
    current_theme.win_title_inactive_fg = 0xFFC0C0C0;
    
    current_theme.color_hi_1 = 0xFFFFFFFF;
    current_theme.color_lo_1 = 0xFF808080;
    current_theme.color_lo_2 = 0xFF000000;
    current_theme.color_text = 0xFF000000;
    
    current_theme.border_width = 1;
    current_theme.title_height = 24;
    current_theme.padding = 4;
}

fly_theme_t* theme_get(void) {
    return &current_theme;
}