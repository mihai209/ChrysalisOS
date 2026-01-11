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

extern "C" void serial(const char *fmt, ...);

/* Program Manager State */
static window_t* taskbar_win = NULL;
static flyui_context_t* taskbar_ctx = NULL;
static window_t* popup_win = NULL;
static flyui_context_t* popup_ctx = NULL;
static bool is_gui_running = false;
static int taskbar_last_min = -1;

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

/* Button Handler: Lansează Ceasul */
static bool clock_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        clock_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Calculatorul */
static bool calc_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        calculator_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Notepad */
static bool note_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        notepad_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Calendarul */
static bool cal_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        calendar_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează File Manager */
static bool files_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        file_manager_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Image Viewer */
static bool img_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        image_viewer_app_create(NULL);
        return true;
    }
    return false;
}

/* Button Handler: Lansează SysInfo */
static bool info_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        sysinfo_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Task Manager */
static bool task_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        task_manager_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Paint */
static bool paint_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        paint_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Demo 3D */
static bool demo_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        demo3d_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Minesweeper */
static bool mine_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        minesweeper_app_create();
        return true;
    }
    return false;
}

/* Button Handler: Lansează Doom */
static bool doom_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
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

    int w = 320;
    int h = 140;
    surface_t* s = surface_create(w, h);
    if (!s) return;
    surface_clear(s, 0xFFE0E0E0);

    popup_ctx = flyui_init(s);
    fly_widget_t* root = fly_panel_create(w, h);
    root->bg_color = 0xFFE0E0E0;
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

static bool start_btn_event(fly_widget_t* w, fly_event_t* e) {
    (void)w;
    if (e->type == FLY_EVENT_MOUSE_UP) {
        create_exit_popup();
        return true;
    }
    return false;
}

static void create_taskbar() {
    gpu_device_t* gpu = gpu_get_primary();
    if (!gpu) return;

    int w = gpu->width;
    int h = 40; /* Taskbar height */
    
    /* 1. Create Surface */
    surface_t* s = surface_create(w, h);
    if (!s) return;
    
    /* Solid gray background for taskbar */
    surface_clear(s, 0xFFC0C0C0);

    /* 2. Init FlyUI */
    taskbar_ctx = flyui_init(s);
    
    /* 3. Create Widgets */
    fly_widget_t* root = fly_panel_create(w, h);
    root->bg_color = 0xFFC0C0C0;
    flyui_set_root(taskbar_ctx, root);

    int x = 10;
    int y = 5;
    int bw = 60; /* Reduced width to fit more apps */
    int bh = 30;
    int gap = 5;

    /* Start Button (<) */
    fly_widget_t* btn_start = fly_button_create("<");
    btn_start->x = x; btn_start->y = y; btn_start->w = 30; btn_start->h = bh;
    btn_start->on_event = start_btn_event;
    fly_widget_add(root, btn_start);
    x += 30 + gap;

    /* Run Button */
    fly_widget_t* btn_run = fly_button_create("Run");
    btn_run->x = x; btn_run->y = y; btn_run->w = 40; btn_run->h = bh;
    btn_run->on_event = run_btn_event;
    fly_widget_add(root, btn_run);
    x += 40 + gap;

    /* Terminal Button */
    fly_widget_t* btn = fly_button_create("Term");
    btn->x = x; btn->y = y; btn->w = bw; btn->h = bh;
    btn->on_event = terminal_btn_event;
    fly_widget_add(root, btn);
    x += bw + gap;

    /* File Manager */
    fly_widget_t* btn_files = fly_button_create("Files");
    btn_files->x = x; btn_files->y = y; btn_files->w = bw; btn_files->h = bh;
    btn_files->on_event = files_btn_event;
    fly_widget_add(root, btn_files);
    x += bw + gap;

    /* Notepad Button */
    fly_widget_t* btn_note = fly_button_create("Edit");
    btn_note->x = x; btn_note->y = y; btn_note->w = 50; btn_note->h = bh;
    btn_note->on_event = note_btn_event;
    fly_widget_add(root, btn_note);
    x += 50 + gap;

    /* Paint */
    fly_widget_t* btn_paint = fly_button_create("Paint");
    btn_paint->x = x; btn_paint->y = y; btn_paint->w = 50; btn_paint->h = bh;
    btn_paint->on_event = paint_btn_event;
    fly_widget_add(root, btn_paint);
    x += 50 + gap;

    /* Calculator Button */
    fly_widget_t* btn_calc = fly_button_create("Calc");
    btn_calc->x = x; btn_calc->y = y; btn_calc->w = 50; btn_calc->h = bh;
    btn_calc->on_event = calc_btn_event;
    fly_widget_add(root, btn_calc);
    x += 50 + gap;

    /* Clock Button */
    fly_widget_t* btn_clk = fly_button_create("Clock");
    btn_clk->x = x; btn_clk->y = y; btn_clk->w = 50; btn_clk->h = bh;
    btn_clk->on_event = clock_btn_event;
    fly_widget_add(root, btn_clk);
    x += 50 + gap;

    /* Calendar Button */
    fly_widget_t* btn_cal = fly_button_create("Cal");
    btn_cal->x = x; btn_cal->y = y; btn_cal->w = 40; btn_cal->h = bh;
    btn_cal->on_event = cal_btn_event;
    fly_widget_add(root, btn_cal);
    x += 40 + gap;

    /* Task Manager */
    fly_widget_t* btn_task = fly_button_create("Tasks");
    btn_task->x = x; btn_task->y = y; btn_task->w = 50; btn_task->h = bh;
    btn_task->on_event = task_btn_event;
    fly_widget_add(root, btn_task);
    x += 50 + gap;

    /* Info */
    fly_widget_t* btn_info = fly_button_create("Info");
    btn_info->x = x; btn_info->y = y; btn_info->w = 40; btn_info->h = bh;
    btn_info->on_event = info_btn_event;
    fly_widget_add(root, btn_info);
    x += 40 + gap;

    /* 3D Demo */
    fly_widget_t* btn_3d = fly_button_create("3D");
    btn_3d->x = x; btn_3d->y = y; btn_3d->w = 30; btn_3d->h = bh;
    btn_3d->on_event = demo_btn_event;
    fly_widget_add(root, btn_3d);
    x += 30 + gap;

    /* Minesweeper Button */
    fly_widget_t* btn_mine = fly_button_create("Mine");
    btn_mine->x = x; btn_mine->y = y; btn_mine->w = 40; btn_mine->h = bh;
    btn_mine->on_event = mine_btn_event;
    fly_widget_add(root, btn_mine);
    x += 40 + gap;

    /* Doom Button */
    fly_widget_t* btn_doom = fly_button_create("Doom");
    btn_doom->x = x; btn_doom->y = y; btn_doom->w = 50; btn_doom->h = bh;
    btn_doom->on_event = doom_btn_event;
    fly_widget_add(root, btn_doom);
    x += 50 + gap;

    /* Clock Widget (Right Aligned) */
    fly_widget_t* sys_clock = fly_panel_create(100, h);
    sys_clock->x = w - 105; /* 5px margin from right */
    sys_clock->y = 0;
    sys_clock->bg_color = 0xFFC0C0C0;
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