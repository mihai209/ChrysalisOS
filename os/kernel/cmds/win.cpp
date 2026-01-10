#include "win.h"
#include "../ui/wm/wm.h"
#include "../video/compositor.h"
#include "../input/input.h"
#include "../drivers/serial.h"
#include "../terminal.h"
#include "../ui/flyui/flyui.h"
#include "../ui/flyui/widgets/widgets.h"
#include "../ui/flyui/theme.h"
#include "../shell/shell.h"
#include "../ui/flyui/bmp.h"

extern "C" void serial(const char *fmt, ...);

/* Program Manager State */
static window_t* progman_win = NULL;
static flyui_context_t* progman_ctx = NULL;
static bool is_gui_running = false;

/* Button Handler */
/* Button Handler: Lansează Terminalul */
static bool terminal_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    
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

static void create_program_manager() {
    int w = 400;
    int h = 300;
    
    /* 1. Create Surface */
    surface_t* s = surface_create(w, h);
    if (!s) return;
    
    /* Try to load wallpaper, fallback to solid color */
    if (fly_load_bmp_to_surface(s, "/bg.bmp") != 0) {
        serial("[WIN] Wallpaper not found or invalid, using default color.\n");
        surface_clear(s, FLY_COLOR_BG);
    }

    /* 2. Init FlyUI */
    progman_ctx = flyui_init(s);
    
    /* 3. Create Widgets */
    fly_widget_t* root = fly_panel_create(w, h);
    flyui_set_root(progman_ctx, root);

    /* Title Label */
    fly_widget_t* lbl = fly_label_create("Chrysalis OS Program Manager");
    lbl->x = 10; lbl->y = 10;
    fly_widget_add(root, lbl);

    /* Info Label */
    fly_widget_t* info = fly_label_create("Welcome to the GUI Environment.");
    info->x = 10; info->y = 40;
    fly_widget_add(root, info);

    /* Terminal Button */
    fly_widget_t* btn = fly_button_create("Terminal");
    btn->x = 10; btn->y = 80; btn->w = 100; btn->h = 30;
    btn->on_event = terminal_btn_event;
    fly_widget_add(root, btn);

    /* 4. Initial Render */
    flyui_render(progman_ctx);

    /* 5. Create WM Window */
    progman_win = wm_create_window(s, 100, 100);
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
    compositor_init();
    wm_init();
    
    /* 3. Create Program Manager (The Desktop Shell) */
    create_program_manager();
    
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
        /* Poll Input */
        while (input_pop(&ev)) {
            /* Handle Global Keys */
            if (ev.type == INPUT_KEYBOARD && ev.pressed) {
                if (ev.keycode == 0x58) { /* F12 to Exit */
                     is_gui_running = false;
                }
                
                /* Route keyboard to Shell if it's active and focused (or if progman isn't focused) */
                /* Dacă fereastra Program Manager NU are focusul, presupunem că e la Terminal */
                if (wm_get_focused() != progman_win) {
                     shell_handle_char((char)ev.keycode);
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
                        if (target) {
                            wm_focus_window(target);
                            wm_mark_dirty();
                            
                            /* Check for Title Bar Hit (0-24px relative to window) */
                            int rel_y = ev.mouse_y - target->y;
                            if (rel_y >= 0 && rel_y < 24) {
                                drag_win = target;
                                drag_off_x = ev.mouse_x - target->x;
                                drag_off_y = ev.mouse_y - target->y;
                            }
                        }
                    } else {
                        /* Mouse Up: Stop Dragging */
                        drag_win = NULL;
                    }
                }

                /* 3. Dispatch to Program Manager (only if it's the target and we are NOT dragging) */
                if (target == progman_win && progman_ctx && !drag_win) {
                    fly_event_t fev;
                    fev.mx = ev.mouse_x - progman_win->x;
                    fev.my = ev.mouse_y - progman_win->y;
                    fev.keycode = 0;
                    fev.type = FLY_EVENT_NONE;

                    if (ev.type == INPUT_MOUSE_MOVE) {
                        fev.type = FLY_EVENT_MOUSE_MOVE;
                    } else if (ev.type == INPUT_MOUSE_CLICK) {
                        fev.type = ev.pressed ? FLY_EVENT_MOUSE_DOWN : FLY_EVENT_MOUSE_UP;
                    }

                    if (fev.type != FLY_EVENT_NONE) {
                        flyui_dispatch_event(progman_ctx, &fev);
                        
                        /* Only redraw on interaction (clicks), NOT on mouse move.
                           This prevents the "Rendering" spam. */
                        if (fev.type != FLY_EVENT_MOUSE_MOVE) {
                            flyui_render(progman_ctx);
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
    if (progman_win) wm_destroy_window(progman_win);
    progman_win = NULL;
    progman_ctx = NULL;
    
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