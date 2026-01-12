#include "win.h"
#include "../ui/wm/wm.h"
#include "../video/compositor.h"
#include "../video/gpu.h"
#include "../input/input.h"
#include "../drivers/serial.h"
#include "../terminal.h"
#include "../ui/flyui/flyui.h"
#include "../ui/flyui/widgets/widgets.h"
#include "../ui/flyui/theme.h"
#include "../shell/shell.h"
#include "../ui/flyui/bmp.h"
#include "../apps/apps.h"
#include "../string.h"
#include "../time/clock.h"
#include "../ui/flyui/draw.h"
#include "../apps/app_manager.h"
#include "../apps/demo3d_app.h"
#include "../apps/doom_app.h"
#include "../apps/minesweeper_app.h"
#include "../ethernet/net.h"
#include "../ethernet/net_device.h"
#include "../include/stdio.h"
#include "../mem/kmalloc.h"

extern "C" void serial(const char *fmt, ...);

/* Program Manager State */
static window_t* taskbar_win = NULL;
static flyui_context_t* taskbar_ctx = NULL;
static window_t* popup_win = NULL;
static flyui_context_t* popup_ctx = NULL;
static window_t* net_win = NULL;
static flyui_context_t* net_ctx = NULL;
static window_t* start_menu_win = NULL;
static flyui_context_t* start_menu_ctx = NULL;
static bool is_gui_running = false;
static int taskbar_last_min = -1;

/* Icon Button Logic */
typedef struct {
    int icon_type;
    bool pressed;
    bool (*event_cb)(fly_widget_t* w, fly_event_t* e);
} icon_btn_data_t;

enum {
    ICON_START = 0,
    ICON_TERM,
    ICON_FILES,
    ICON_IMG,
    ICON_NOTE,
    ICON_PAINT,
    ICON_CALC,
    ICON_CLOCK,
    ICON_CAL,
    ICON_TASK,
    ICON_INFO,
    ICON_3D,
    ICON_MINE,
    ICON_DOOM,
    ICON_NET,
    ICON_RUN
};

static void draw_icon_graphic(surface_t* s, int x, int y, int type) {
    /* Simple pixel art icons */
    switch (type) {
        case ICON_START: /* Simple Diamond */
            fly_draw_rect_fill(s, x+6, y, 4, 16, 0xFF000000);
            fly_draw_rect_fill(s, x, y+6, 16, 4, 0xFF000000);
            break;
        case ICON_TERM: /* Terminal >_ */
            fly_draw_rect_fill(s, x, y, 16, 14, 0xFF000000);
            fly_draw_text(s, x+2, y+2, ">", 0xFF00FF00);
            break;
        case ICON_FILES: /* Folder */
            fly_draw_rect_fill(s, x, y+2, 16, 12, 0xFFFFCC00);
            fly_draw_rect_fill(s, x, y, 6, 2, 0xFFFFCC00);
            break;
        case ICON_IMG: /* Image */
            fly_draw_rect_fill(s, x, y, 14, 14, 0xFFFFFFFF);
            fly_draw_rect_outline(s, x, y, 14, 14, 0xFF000000);
            fly_draw_rect_fill(s, x+4, y+4, 6, 6, 0xFFFF0000);
            break;
        case ICON_NOTE: /* Notepad */
            fly_draw_rect_fill(s, x+2, y, 12, 16, 0xFFFFFFFF);
            fly_draw_rect_fill(s, x+4, y+4, 8, 2, 0xFF000000);
            fly_draw_rect_fill(s, x+4, y+8, 8, 2, 0xFF000000);
            break;
        case ICON_PAINT: /* Brush */
            fly_draw_rect_fill(s, x+4, y, 4, 10, 0xFFA0522D);
            fly_draw_rect_fill(s, x+2, y+10, 8, 4, 0xFFFF0000);
            break;
        case ICON_CALC: /* Calc */
            fly_draw_rect_fill(s, x, y, 12, 16, 0xFFC0C0C0);
            fly_draw_rect_fill(s, x+2, y+2, 8, 4, 0xFFFFFFFF);
            break;
        case ICON_CLOCK: /* Clock */
            fly_draw_rect_outline(s, x, y, 14, 14, 0xFF000000);
            fly_draw_rect_fill(s, x+7, y+2, 1, 6, 0xFF000000);
            fly_draw_rect_fill(s, x+7, y+7, 4, 1, 0xFF000000);
            break;
        case ICON_CAL: /* Calendar */
            fly_draw_rect_fill(s, x, y, 16, 16, 0xFFFFFFFF);
            fly_draw_rect_fill(s, x, y, 16, 4, 0xFFFF0000);
            fly_draw_text(s, x+2, y+4, "7", 0xFF000000);
            break;
        case ICON_TASK: /* List */
            fly_draw_rect_fill(s, x, y, 14, 16, 0xFFFFFFFF);
            fly_draw_rect_fill(s, x+2, y+2, 10, 2, 0xFF000000);
            fly_draw_rect_fill(s, x+2, y+6, 10, 2, 0xFF000000);
            break;
        case ICON_INFO: /* Info */
            fly_draw_rect_fill(s, x, y, 14, 14, 0xFF0000FF);
            fly_draw_text(s, x+4, y, "i", 0xFFFFFFFF);
            break;
        case ICON_3D: /* Cube */
            fly_draw_rect_outline(s, x, y, 12, 12, 0xFF000000);
            fly_draw_rect_outline(s, x+4, y+4, 12, 12, 0xFF000000);
            break;
        case ICON_MINE: /* Mine */
            fly_draw_rect_fill(s, x+4, y+4, 8, 8, 0xFF000000);
            fly_draw_rect_fill(s, x+7, y, 2, 4, 0xFF000000);
            break;
        case ICON_DOOM: /* Face */
            fly_draw_rect_fill(s, x, y, 14, 16, 0xFF8B4513);
            fly_draw_rect_fill(s, x+2, y+4, 4, 4, 0xFFFFFFFF);
            fly_draw_rect_fill(s, x+8, y+4, 4, 4, 0xFFFFFFFF);
            break;
        case ICON_NET: /* Net */
            fly_draw_rect_fill(s, x, y+4, 16, 8, 0xFF0000FF);
            fly_draw_rect_fill(s, x+6, y, 4, 16, 0xFF0000FF);
            break;
        case ICON_RUN: /* Run */
            fly_draw_rect_fill(s, x, y, 16, 16, 0xFFFFFFFF);
            fly_draw_text(s, x+4, y+2, "R", 0xFF000000);
            break;
    }
}

static void fly_draw_line(surface_t* surf, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 > y0) ? -(y1 - y0) : -(y0 - y1);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int e2;

    for (;;) {
        if (x0 >= 0 && x0 < (int)surf->width && y0 >= 0 && y0 < (int)surf->height) {
            surf->pixels[y0 * surf->width + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void taskbar_btn_draw(fly_widget_t* w, surface_t* s, int x, int y) {
    icon_btn_data_t* d = (icon_btn_data_t*)w->internal_data;
    uint32_t bg = w->bg_color;
    fly_theme_t* th = theme_get();
    
    /* Dotted pattern for active taskbar button? For now just darker */
    if (d && d->pressed) bg = th->color_hi_1; /* Inverted look */
    
    fly_draw_rect_fill(s, x, y, w->w, w->h, bg);
    
    /* 3D Bevel for Taskbar Buttons */
    uint32_t c_tl = d->pressed ? th->color_lo_1 : th->color_hi_1;
    uint32_t c_br = d->pressed ? th->color_hi_1 : th->color_lo_1;
    
    fly_draw_line(s, x, y, x+w->w-1, y, c_tl);
    fly_draw_line(s, x, y, x, y+w->h-1, c_tl);
    fly_draw_line(s, x, y+w->h-1, x+w->w-1, y+w->h-1, c_br);
    fly_draw_line(s, x+w->w-1, y, x+w->w-1, y+w->h-1, c_br);

    
    /* Draw Icon Centered */
    draw_icon_graphic(s, x + (w->w - 16)/2, y + (w->h - 16)/2, d->icon_type);
}

/* Button Handler */
/* Button Handler: Lansează Terminalul */
static bool terminal_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    
    /* Close start menu if open */
    if (start_menu_win) {
        wm_destroy_window(start_menu_win);
        start_menu_win = NULL;
        start_menu_ctx = NULL;
        wm_mark_dirty();
    }

    /* Lansăm terminalul pe Mouse Up pentru a evita lansări multiple accidentale */
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (!shell_is_window_active()) {
            serial("[WIN] Launching Terminal Window...\n");
            shell_create_window();
            
            /* Activăm randarea textului în fereastra nou creată */
            terminal_set_rendering(true);
            
            /* Forțăm o redesenare pentru a afișa fereastra imediat */
            wm_mark_dirty();
        }
        return true;
    } 
    else if (e->type == FLY_EVENT_MOUSE_DOWN) {
        /* Consumăm evenimentul de click și redesenăm pentru efect vizual */
        wm_mark_dirty();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Ceasul */
static bool clock_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        clock_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Calculatorul */
static bool calc_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        calculator_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Notepad */
static bool note_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        notepad_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Calendarul */
static bool cal_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        calendar_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează File Manager */
static bool files_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        file_manager_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Image Viewer */
static bool img_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        image_viewer_app_create(NULL);
        return true;
    }
    return false;
}

/* Button Handler: Lansează SysInfo */
static bool info_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        sysinfo_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Task Manager */
static bool task_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        task_manager_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Paint */
static bool paint_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        paint_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Demo 3D */
static bool demo_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        demo3d_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Minesweeper */
static bool mine_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        minesweeper_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Doom */
static bool doom_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        doom_app_create();
        return true;
    }
    return false;
}

/* Taskbar Clock Widget Draw */
static void taskbar_clock_draw(fly_widget_t* w, surface_t* surf, int x, int y) {
    /* Background */
    fly_draw_rect_fill(surf, x, y, w->w, w->h, w->bg_color);
    
    datetime t;
    time_get_local(&t);
    
    char time_str[16];
    char date_str[16];
    char tmp[8];
    
    /* Time: HH:MM */
    time_str[0] = '0' + (t.hour / 10);
    time_str[1] = '0' + (t.hour % 10);
    time_str[2] = ':';
    time_str[3] = '0' + (t.minute / 10);
    time_str[4] = '0' + (t.minute % 10);
    time_str[5] = 0;
    
    /* Date: DD.MM.YYYY */
    date_str[0] = '0' + (t.day / 10);
    date_str[1] = '0' + (t.day % 10);
    date_str[2] = '.';
    date_str[3] = '0' + (t.month / 10);
    date_str[4] = '0' + (t.month % 10);
    date_str[5] = '.';
    
    itoa_dec(tmp, t.year);
    strcpy(date_str + 6, tmp);
    
    /* Draw Time (Top) */
    int time_w = strlen(time_str) * 8;
    int tx = x + (w->w - time_w) / 2;
    int ty = y + 4;
    fly_draw_text(surf, tx, ty, time_str, 0xFF000000);
    
    /* Draw Date (Bottom) */
    int date_w = strlen(date_str) * 8;
    int dx = x + (w->w - date_w) / 2;
    int dy = y + 20;
    fly_draw_text(surf, dx, dy, date_str, 0xFF000000);
}

/* Popup Handlers */
static bool popup_yes_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (popup_win) {
            wm_destroy_window(popup_win);
            popup_win = NULL;
            popup_ctx = NULL;
        }
        is_gui_running = false;
        return true;
    }
    return false;
}

static bool popup_no_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (popup_win) {
            wm_destroy_window(popup_win);
            popup_win = NULL;
            popup_ctx = NULL;
            wm_mark_dirty();
        }
        return true;
    }
    return false;
}

/* Run Button Handler */
static bool run_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        run_dialog_app_create();
        return true;
    }
    return false;
}

static void create_exit_popup() {
    if (popup_win) return;

    fly_theme_t* th = theme_get();
    int w = 320;
    int h = 140;
    surface_t* s = surface_create(w, h);
    if (!s) return;
    surface_clear(s, th->win_bg);
    
    /* Window Border */
    fly_draw_rect_outline(s, 0, 0, w, h, th->color_hi_1);
    fly_draw_rect_outline(s, 0, 0, w-1, h-1, th->color_lo_2);

    popup_ctx = flyui_init(s);
    fly_widget_t* root = fly_panel_create(w, h);
    root->bg_color = th->win_bg;
    flyui_set_root(popup_ctx, root);

    /* Title/Question */
    fly_widget_t* lbl = fly_label_create("Want to exit the GUI enviroment?");
    lbl->x = 20; lbl->y = 40;
    fly_widget_add(root, lbl);

    /* Yes Button */
    fly_widget_t* btn_yes = fly_button_create("Yes");
    btn_yes->x = 60; btn_yes->y = 90; btn_yes->w = 80; btn_yes->h = 30;
    btn_yes->on_event = popup_yes_event;
    fly_widget_add(root, btn_yes);

    /* No Button */
    fly_widget_t* btn_no = fly_button_create("No");
    btn_no->x = 180; btn_no->y = 90; btn_no->w = 80; btn_no->h = 30;
    btn_no->on_event = popup_no_event;
    fly_widget_add(root, btn_no);

    /* X Button */
    fly_widget_t* btn_x = fly_button_create("X");
    btn_x->x = w - 30; btn_x->y = 5; btn_x->w = 25; btn_x->h = 25;
    btn_x->on_event = popup_no_event;
    fly_widget_add(root, btn_x);

    flyui_render(popup_ctx);

    gpu_device_t* gpu = gpu_get_primary();
    int sx = (gpu->width - w) / 2;
    int sy = (gpu->height - h) / 2;
    popup_win = wm_create_window(s, sx, sy);
}

/* Network Popup Handlers */
static bool net_popup_close_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (net_win) {
            wm_destroy_window(net_win);
            net_win = NULL;
            net_ctx = NULL;
            wm_mark_dirty();
        }
        return true;
    }
    return false;
}

static void create_net_popup() {
    if (net_win) return;

    fly_theme_t* th = theme_get();
    int w = 250;
    int h = 180;
    surface_t* s = surface_create(w, h);
    if (!s) return;
    surface_clear(s, th->win_bg);
    
    /* Border */
    fly_draw_rect_outline(s, 0, 0, w, h, th->color_hi_1);
    fly_draw_rect_outline(s, 0, 0, w-1, h-1, th->color_lo_2);

    net_ctx = flyui_init(s);
    fly_widget_t* root = fly_panel_create(w, h);
    root->bg_color = th->win_bg;
    flyui_set_root(net_ctx, root);

    /* Title */
    fly_widget_t* lbl_title = fly_label_create("Network Status");
    lbl_title->x = 10; lbl_title->y = 10;
    fly_widget_add(root, lbl_title);

    /* Close Button */
    fly_widget_t* btn_close = fly_button_create("X");
    btn_close->x = w - 30; btn_close->y = 5; btn_close->w = 25; btn_close->h = 25;
    btn_close->on_event = net_popup_close_event;
    fly_widget_add(root, btn_close);

    /* Data */
    net_device_t* dev = net_get_primary_device();
    char buf[64];
    int y = 40;

    if (dev) {
        fly_widget_t* lbl_stat = fly_label_create("Status: Connected");
        lbl_stat->x = 10; lbl_stat->y = y;
        fly_widget_add(root, lbl_stat);
        y += 20;

        uint32_t ip = dev->ip;
        sprintf(buf, "IP: %d.%d.%d.%d", ip&0xFF, (ip>>8)&0xFF, (ip>>16)&0xFF, (ip>>24)&0xFF);
        fly_widget_t* lbl_ip = fly_label_create(buf);
        lbl_ip->x = 10; lbl_ip->y = y;
        fly_widget_add(root, lbl_ip);
        y += 20;

        uint32_t gw = dev->gateway;
        sprintf(buf, "GW: %d.%d.%d.%d", gw&0xFF, (gw>>8)&0xFF, (gw>>16)&0xFF, (gw>>24)&0xFF);
        fly_widget_t* lbl_gw = fly_label_create(buf);
        lbl_gw->x = 10; lbl_gw->y = y;
        fly_widget_add(root, lbl_gw);
        y += 20;

        uint32_t dns = dev->dns_server;
        sprintf(buf, "DNS: %d.%d.%d.%d", dns&0xFF, (dns>>8)&0xFF, (dns>>16)&0xFF, (dns>>24)&0xFF);
        fly_widget_t* lbl_dns = fly_label_create(buf);
        lbl_dns->x = 10; lbl_dns->y = y;
        fly_widget_add(root, lbl_dns);
        y += 20;

        sprintf(buf, "MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
            dev->mac[0], dev->mac[1], dev->mac[2], dev->mac[3], dev->mac[4], dev->mac[5]);
        fly_widget_t* lbl_mac = fly_label_create(buf);
        lbl_mac->x = 10; lbl_mac->y = y;
        fly_widget_add(root, lbl_mac);
    } else {
        fly_widget_t* lbl_stat = fly_label_create("Status: No Device");
        lbl_stat->x = 10; lbl_stat->y = y;
        fly_widget_add(root, lbl_stat);
    }

    flyui_render(net_ctx);

    gpu_device_t* gpu = gpu_get_primary();
    /* Position near bottom right, above taskbar */
    int sx = gpu->width - w - 10;
    int sy = gpu->height - 40 - h - 5; 
    net_win = wm_create_window(s, sx, sy);
}

static bool net_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (net_win) {
            wm_destroy_window(net_win);
            net_win = NULL;
            net_ctx = NULL;
            wm_mark_dirty();
        } else {
            create_net_popup();
        }
        return true;
    }
    return false;
}

/* Start Menu Implementation */
static bool start_menu_shutdown_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        if (start_menu_win) {
            wm_destroy_window(start_menu_win);
            start_menu_win = NULL;
            start_menu_ctx = NULL;
        }
        create_exit_popup();
        return true;
    }
    return false;
}

static void create_start_menu() {
    if (start_menu_win) {
        wm_destroy_window(start_menu_win);
        start_menu_win = NULL;
        start_menu_ctx = NULL;
        wm_mark_dirty();
        return;
    }

    fly_theme_t* th = theme_get();
    int w = 150;
    int h = 300;
    surface_t* s = surface_create(w, h);
    if (!s) return;
    surface_clear(s, th->win_bg);
    
    /* Draw 3D border */
    fly_draw_rect_outline(s, 0, 0, w, h, th->color_hi_1);
    fly_draw_rect_outline(s, 0, 0, w-1, h-1, th->color_lo_2);

    start_menu_ctx = flyui_init(s);
    fly_widget_t* root = fly_panel_create(w, h);
    root->bg_color = th->win_bg;
    flyui_set_root(start_menu_ctx, root);
    
    /* Side bar */
    fly_widget_t* side = fly_panel_create(24, h-4);
    side->x = 2; side->y = 2;
    side->bg_color = 0xFF000080; /* Dark Blue */
    fly_widget_add(root, side);

    int y = 5;
    int bh = 25;
    int bx = 30;
    int bw = w - 35;
    
    /* Menu Items */
    fly_widget_t* btn;
    
    btn = fly_button_create("Terminal"); btn->x = bx; btn->y = y; btn->w = bw; btn->h = bh; btn->on_event = terminal_btn_event; fly_widget_add(root, btn); y += bh + 2;
    btn = fly_button_create("Files");    btn->x = bx; btn->y = y; btn->w = bw; btn->h = bh; btn->on_event = files_btn_event;    fly_widget_add(root, btn); y += bh + 2;
    btn = fly_button_create("Notepad");  btn->x = bx; btn->y = y; btn->w = bw; btn->h = bh; btn->on_event = note_btn_event;     fly_widget_add(root, btn); y += bh + 2;
    btn = fly_button_create("Paint");    btn->x = bx; btn->y = y; btn->w = bw; btn->h = bh; btn->on_event = paint_btn_event;    fly_widget_add(root, btn); y += bh + 2;
    btn = fly_button_create("Calc");     btn->x = bx; btn->y = y; btn->w = bw; btn->h = bh; btn->on_event = calc_btn_event;     fly_widget_add(root, btn); y += bh + 2;
    btn = fly_button_create("Run...");   btn->x = bx; btn->y = y; btn->w = bw; btn->h = bh; btn->on_event = run_btn_event;      fly_widget_add(root, btn); y += bh + 2;
    
    y += 5;
    /* Separator */
    fly_widget_t* sep = fly_panel_create(bw, 2);
    sep->x = bx; sep->y = y; sep->bg_color = 0xFF808080;
    fly_widget_add(root, sep);
    y += 10;
    
    btn = fly_button_create("Shutdown"); btn->x = bx; btn->y = y; btn->w = bw; btn->h = bh; btn->on_event = start_menu_shutdown_event; fly_widget_add(root, btn);

    flyui_render(start_menu_ctx);

    gpu_device_t* gpu = gpu_get_primary();
    /* Position above start button */
    start_menu_win = wm_create_window(s, 0, gpu->height - 32 - h);
}

static bool start_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        create_start_menu();
        return true;
    }
    return false;
}

static fly_widget_t* create_taskbar_btn(int x, int y, int w, int h, int icon, bool (*cb)(fly_widget_t*, fly_event_t*)) {
    fly_widget_t* btn = fly_widget_create();
    btn->x = x; btn->y = y; btn->w = w; btn->h = h;
    
    icon_btn_data_t* d = (icon_btn_data_t*)kmalloc(sizeof(icon_btn_data_t));
    d->icon_type = icon;
    d->pressed = false;
    d->event_cb = cb;
    
    btn->internal_data = d;
    btn->bg_color = theme_get()->win_bg;
    btn->on_draw = taskbar_btn_draw;
    
    /* Generic Event Wrapper */
    btn->on_event = [](fly_widget_t* w, fly_event_t* e) -> bool {
        icon_btn_data_t* d = (icon_btn_data_t*)w->internal_data;
        if (e->type == FLY_EVENT_MOUSE_DOWN) {
            d->pressed = true;
            return true;
        } else if (e->type == FLY_EVENT_MOUSE_UP) {
            d->pressed = false;
            if (d->event_cb) return d->event_cb(w, e);
            return true;
        }
        return false;
    };
    
    return btn;
}

static void create_taskbar() {
    gpu_device_t* gpu = gpu_get_primary();
    if (!gpu) return;

    fly_theme_t* th = theme_get();
    int w = gpu->width;
    int h = 32; /* Taskbar height */
    
    /* 1. Create Surface */
    surface_t* s = surface_create(w, h);
    if (!s) return;
    
    /* Gray background */
    surface_clear(s, th->win_bg);
    
    /* Top highlight line */
    fly_draw_line(s, 0, 0, w, 0, th->color_hi_1);

    /* 2. Init FlyUI */
    taskbar_ctx = flyui_init(s);
    
    /* 3. Create Widgets */
    fly_widget_t* root = fly_panel_create(w, h);
    root->bg_color = th->win_bg;
    flyui_set_root(taskbar_ctx, root);

    int x = 0;
    int y = 0;
    int bw = 32; /* Icon button width */
    int bh = 32; /* Icon button height */
    
    /* Start Button */
    fly_widget_t* btn_start = create_taskbar_btn(x, y, 40, bh, ICON_START, start_btn_event);
    fly_widget_add(root, btn_start);
    x += 40 + 4;

    /* Run */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_RUN, run_btn_event)); x += bw;
    
    /* Terminal */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_TERM, terminal_btn_event)); x += bw;
    
    /* Files */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_FILES, files_btn_event)); x += bw;
    
    /* Image */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_IMG, img_btn_event)); x += bw;
    
    /* Notepad */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_NOTE, note_btn_event)); x += bw;
    
    /* Paint */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_PAINT, paint_btn_event)); x += bw;
    
    /* Calc */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_CALC, calc_btn_event)); x += bw;
    
    /* Clock App */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_CLOCK, clock_btn_event)); x += bw;
    
    /* Calendar */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_CAL, cal_btn_event)); x += bw;
    
    /* Task Manager */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_TASK, task_btn_event)); x += bw;
    
    /* Info */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_INFO, info_btn_event)); x += bw;
    
    /* 3D */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_3D, demo_btn_event)); x += bw;
    
    /* Mine */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_MINE, mine_btn_event)); x += bw;
    
    /* Doom */
    fly_widget_add(root, create_taskbar_btn(x, y, bw, bh, ICON_DOOM, doom_btn_event)); x += bw;
    
    /* Net (Right Aligned) */
    fly_widget_add(root, create_taskbar_btn(w - 150, y, bw, bh, ICON_NET, net_btn_event));

    /* Clock Widget (Right Aligned) */
    fly_widget_t* sys_clock = fly_panel_create(100, h);
    sys_clock->x = w - 105; /* 5px margin from right */
    sys_clock->y = 0;
    sys_clock->bg_color = th->win_bg;
    sys_clock->on_draw = taskbar_clock_draw;
    fly_widget_add(root, sys_clock);

    /* 4. Initial Render */
    flyui_render(taskbar_ctx);

    /* 5. Create WM Window */
    /* Position at bottom of screen */
    taskbar_win = wm_create_window(s, 0, gpu->height - h);
}

extern "C" int cmd_launch_exit(int argc, char** argv) {
    (void)argc; (void)argv;
    if (is_gui_running) {
        is_gui_running = false;
        return 0;
    }
    terminal_writestring("GUI is not running.\n");
    return 1;
}

extern "C" int cmd_launch(int argc, char** argv) {
    (void)argc; (void)argv;
    
    if (is_gui_running) {
        terminal_writestring("GUI is already running.\n");
        return 1;
    }
    is_gui_running = true;
    
    serial("[WIN] Starting GUI environment...\n");

    /* 1. Disable Terminal Rendering (Text Mode OFF) - until shell window is opened */
    terminal_set_rendering(false);

    /* 2. Initialize GUI Subsystems */
    theme_init();
    compositor_init();
    wm_init();
    app_manager_init();
    
    /* 3. Create Taskbar */
    create_taskbar();
    
    /* 4. Main GUI Loop */
    input_event_t ev;
    
    /* Force initial render */
    wm_mark_dirty();
    wm_render();

    /* State for Window Dragging */
    window_t* drag_win = NULL;
    int drag_off_x = 0;
    int drag_off_y = 0;

    while (is_gui_running) {
        /* Update Apps */
        clock_app_update();
        demo3d_app_update();
        doom_app_update();

        /* Update Taskbar Clock */
        datetime t;
        time_get_local(&t);
        if (t.minute != taskbar_last_min) {
            taskbar_last_min = t.minute;
            /* Redraw taskbar to update clock */
            flyui_render(taskbar_ctx);
            wm_mark_dirty();
        }

        /* Poll Input */
        while (input_pop(&ev)) {
            /* Handle Global Keys */
            if (ev.type == INPUT_KEYBOARD && ev.pressed) {
                if (ev.keycode == 0x58) { /* F12 to Exit */
                     is_gui_running = false;
                }
                
                /* Route keyboard to Shell if it's active and focused (or if progman isn't focused) */
                window_t* focused = wm_get_focused();
                if (focused == shell_get_window()) {
                     shell_handle_char((char)ev.keycode);
                } else if (focused == notepad_app_get_window()) {
                     notepad_app_handle_key((char)ev.keycode);
                } else if (focused == run_dialog_app_get_window()) {
                     run_dialog_app_handle_key((char)ev.keycode);
                } 
            }
            
            /* Mouse Event Handling */
            if (ev.type == INPUT_MOUSE_MOVE || ev.type == INPUT_MOUSE_CLICK) {
                
                /* Handle Dragging Logic */
                if (drag_win && ev.type == INPUT_MOUSE_MOVE) {
                    drag_win->x = ev.mouse_x - drag_off_x;
                    drag_win->y = ev.mouse_y - drag_off_y;
                    wm_mark_dirty();
                }

                /* 1. Find Window Under Mouse (Top-most) if not dragging */
                window_t* target = drag_win ? drag_win : wm_find_window_at(ev.mouse_x, ev.mouse_y);

                /* 2. Handle Focus & Drag Start on Click */
                if (ev.type == INPUT_MOUSE_CLICK) {
                    if (ev.pressed) {
                        /* Handle Scroll (Buttons 4 & 5) */
                        if (ev.keycode == 4 || ev.keycode == 5) {
                            if (target == shell_get_window()) {
                                /* Map Scroll to History Navigation (Ctrl-P / Ctrl-N) */
                                char key = (ev.keycode == 4) ? 16 /* Ctrl-P */ : 14 /* Ctrl-N */;
                                shell_handle_char(key);
                            }
                        }
                        /* Handle Left Click (Button 1) for Focus/Drag */
                        else if (ev.keycode == 1) {
                            if (target) {
                                wm_focus_window(target);
                                wm_mark_dirty();
                                
                                /* Check for Title Bar Hit (0-24px relative to window) */
                                int rel_y = ev.mouse_y - target->y;
                                if (rel_y >= 0 && rel_y < 24 && target != taskbar_win) { /* Fix: Don't drag taskbar */
                                    drag_win = target;
                                    drag_off_x = ev.mouse_x - target->x;
                                    drag_off_y = ev.mouse_y - target->y;
                                }
                            }
                        }
                    } else {
                        /* Mouse Up: Stop Dragging */
                        drag_win = NULL;
                    }
                }

                /* 3.1 Dispatch to Clock App */
                if (target == clock_app_get_window()) {
                    clock_app_handle_event(&ev);
                }

                /* 3.2 Dispatch to Shell Window */
                if (target == shell_get_window()) {
                    if (shell_handle_event(&ev)) {
                        target = NULL; /* Window destroyed */
                    }
                }

                /* 3.3 Dispatch to Calculator */
                if (target == calculator_app_get_window()) {
                    calculator_app_handle_event(&ev);
                }

                /* 3.4 Dispatch to Notepad */
                if (target == notepad_app_get_window()) {
                    notepad_app_handle_event(&ev);
                }

                /* 3.5 Dispatch to Calendar */
                if (target == calendar_app_get_window()) {
                    calendar_app_handle_event(&ev);
                }

                /* 3.6 Dispatch to File Manager */
                if (target == file_manager_app_get_window()) {
                    file_manager_app_handle_event(&ev);
                }

                /* 3.7 Dispatch to Image Viewer */
                if (target == image_viewer_app_get_window()) {
                    image_viewer_app_handle_event(&ev);
                }

                /* 3.8 Dispatch to SysInfo */
                if (target == sysinfo_app_get_window()) {
                    sysinfo_app_handle_event(&ev);
                }

                /* 3.9 Dispatch to Run Dialog */
                if (target == run_dialog_app_get_window()) {
                    run_dialog_app_handle_event(&ev);
                }

                /* 3.10 Dispatch to Task Manager */
                if (target == task_manager_app_get_window()) {
                    task_manager_app_handle_event(&ev);
                }

                /* 3.11 Dispatch to Paint */
                if (target == paint_app_get_window()) {
                    paint_app_handle_event(&ev);
                }

                /* 3.12 Dispatch to Demo 3D */
                if (target == demo3d_app_get_window()) {
                    demo3d_app_handle_event(&ev);
                }

                /* 3.13 Dispatch to Doom */
                if (target == doom_app_get_window()) {
                    doom_app_handle_event(&ev);
                }

                /* 3.14 Dispatch to Minesweeper */
                if (target == minesweeper_app_get_window()) {
                    minesweeper_app_handle_event(&ev);
                }

                /* 3.6 Dispatch to Popup */
                if (target == popup_win && popup_ctx) {
                    fly_event_t fev;
                    fev.mx = ev.mouse_x - popup_win->x;
                    fev.my = ev.mouse_y - popup_win->y;
                    fev.keycode = 0;
                    fev.type = FLY_EVENT_NONE;

                    if (ev.type == INPUT_MOUSE_MOVE) {
                        fev.type = FLY_EVENT_MOUSE_MOVE;
                    } else if (ev.type == INPUT_MOUSE_CLICK) {
                        fev.type = ev.pressed ? FLY_EVENT_MOUSE_DOWN : FLY_EVENT_MOUSE_UP;
                    }

                    if (fev.type != FLY_EVENT_NONE) {
                        flyui_dispatch_event(popup_ctx, &fev);
                        if (fev.type != FLY_EVENT_MOUSE_MOVE) {
                            flyui_render(popup_ctx);
                            wm_mark_dirty();
                        }
                    }
                }

                /* 3.15 Dispatch to Net Popup */
                if (target == net_win && net_ctx) {
                    fly_event_t fev;
                    fev.mx = ev.mouse_x - net_win->x;
                    fev.my = ev.mouse_y - net_win->y;
                    fev.keycode = 0;
                    fev.type = FLY_EVENT_NONE;

                    if (ev.type == INPUT_MOUSE_MOVE) {
                        fev.type = FLY_EVENT_MOUSE_MOVE;
                    } else if (ev.type == INPUT_MOUSE_CLICK) {
                        fev.type = ev.pressed ? FLY_EVENT_MOUSE_DOWN : FLY_EVENT_MOUSE_UP;
                    }

                    if (fev.type != FLY_EVENT_NONE) {
                        flyui_dispatch_event(net_ctx, &fev);
                        if (fev.type != FLY_EVENT_MOUSE_MOVE) {
                            flyui_render(net_ctx);
                            wm_mark_dirty();
                        }
                    }
                }

                /* 3.16 Dispatch to Start Menu */
                if (target == start_menu_win && start_menu_ctx) {
                    fly_event_t fev;
                    fev.mx = ev.mouse_x - start_menu_win->x;
                    fev.my = ev.mouse_y - start_menu_win->y;
                    fev.keycode = 0;
                    fev.type = FLY_EVENT_NONE;

                    if (ev.type == INPUT_MOUSE_MOVE) {
                        fev.type = FLY_EVENT_MOUSE_MOVE;
                    } else if (ev.type == INPUT_MOUSE_CLICK) {
                        fev.type = ev.pressed ? FLY_EVENT_MOUSE_DOWN : FLY_EVENT_MOUSE_UP;
                    }

                    if (fev.type != FLY_EVENT_NONE) {
                        flyui_dispatch_event(start_menu_ctx, &fev);
                        if (fev.type != FLY_EVENT_MOUSE_MOVE) {
                            flyui_render(start_menu_ctx);
                            wm_mark_dirty();
                        }
                    }
                }

                /* 3. Dispatch to Taskbar (only if it's the target and we are NOT dragging) */
                if (target == taskbar_win && taskbar_ctx && !drag_win) {
                    fly_event_t fev;
                    fev.mx = ev.mouse_x - taskbar_win->x;
                    fev.my = ev.mouse_y - taskbar_win->y;
                    fev.keycode = 0;
                    fev.type = FLY_EVENT_NONE;

                    if (ev.type == INPUT_MOUSE_MOVE) {
                        fev.type = FLY_EVENT_MOUSE_MOVE;
                    } else if (ev.type == INPUT_MOUSE_CLICK) {
                        fev.type = ev.pressed ? FLY_EVENT_MOUSE_DOWN : FLY_EVENT_MOUSE_UP;
                    }

                    if (fev.type != FLY_EVENT_NONE) {
                        flyui_dispatch_event(taskbar_ctx, &fev);
                        
                        /* Only redraw on interaction (clicks), NOT on mouse move.
                           This prevents the "Rendering" spam. */
                        if (fev.type != FLY_EVENT_MOUSE_MOVE) {
                            flyui_render(taskbar_ctx);
                            wm_mark_dirty();
                        }
                    }
                } else if (target == NULL) {
                    /* Clicked on background/desktop */
                    if (ev.type == INPUT_MOUSE_CLICK && ev.pressed) {
                        wm_mark_dirty();
                    }
                }
            }
        }
        
        /* Render GUI */
        wm_render();
        
        asm volatile("hlt");
    }
    
    /* 5. Cleanup & Return to Text Mode */
    if (taskbar_win) wm_destroy_window(taskbar_win);
    taskbar_win = NULL;
    taskbar_ctx = NULL;
    if (popup_win) wm_destroy_window(popup_win);
    popup_win = NULL;
    popup_ctx = NULL;
    if (net_win) wm_destroy_window(net_win);
    net_win = NULL;
    net_ctx = NULL;
    if (start_menu_win) wm_destroy_window(start_menu_win);
    start_menu_win = NULL;
    start_menu_ctx = NULL;
    
    /* Dacă terminalul a fost deschis, îl închidem curat */
    if (shell_is_window_active()) {
        shell_destroy_window();
    }
    
    terminal_set_rendering(true);
    terminal_clear(); /* Clear artifacts */
    serial("[WIN] GUI shutdown. Returning to text mode.\n");
    is_gui_running = false;
    return 0;
}