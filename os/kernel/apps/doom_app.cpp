#include "doom_app.h"
#include "../games/doom/doomgeneric/doomgeneric.h"
#include "../ui/wm/wm.h"
#include "../include/setjmp.h"

extern "C" void serial(const char *fmt, ...);

/* Defined in doomgeneric_chrysalis.c */
extern "C" void DG_KeyDown(int key);
extern "C" void DG_KeyUp(int key);
extern "C" void DG_MouseMove(int dx, int dy);
extern "C" void DG_MouseButton(int button, int pressed);
extern "C" window_t* doom_get_window(void);
extern "C" void doom_destroy_window(void);

static bool doom_running = false;

/* Defined in posix_stubs.c */
extern "C" jmp_buf exit_jmp_buf;
extern "C" int exit_jmp_set;

void doom_app_create(void) {
    if (doom_running) return;
    
    /* Arguments for Doom */
    /* Force doom1.wad to ensure it hits the embedded file check */
    char* argv[] = { (char*)"doom", (char*)"-iwad", (char*)"doom1.wad", NULL };
    
    /* Set recovery point for exit() */
    exit_jmp_set = 1;
    if (setjmp(exit_jmp_buf) == 0) {
        /* Initialize Doom */
        doomgeneric_Create(1, argv);
        doom_running = true;
    } else {
        /* Returned from exit() via longjmp */
        doom_running = false;
        doom_destroy_window();
        exit_jmp_set = 0;
    }
    /* Clear flag if we return normally (though doomgeneric_Create might not return if it loops, 
       but here we assume it returns or we rely on Tick) */
}

void doom_app_update(void) {
    if (!doom_running) return;
    if (!doom_get_window()) {
        doom_running = false;
        return;
    }
    
    /* Set recovery point for errors during Tick */
    exit_jmp_set = 1;
    if (setjmp(exit_jmp_buf) == 0) {
        doomgeneric_Tick();
    } else {
        doom_running = false;
        doom_destroy_window();
        exit_jmp_set = 0;
    }
}

bool doom_app_handle_event(input_event_t* ev) {
    window_t* win = doom_get_window();
    if (!win) return false;
    
    /* Handle Window Close (Title Bar [X]) */
    if (ev->type == INPUT_MOUSE_CLICK && ev->pressed) {
        int lx = ev->mouse_x - win->x;
        int ly = ev->mouse_y - win->y;
        
        /* Close Button Area: Top Right, inside Title Bar (24px height) */
        if (lx >= win->w - 20 && ly < 24) {
            serial("[DOOM] Window close requested\n");
            doom_destroy_window();
            doom_running = false;
            return true;
        }
    }
    
    /* Handle Mouse Input (Movement & Buttons) */
    static int last_mx = -1, last_my = -1;
    
    if (ev->type == INPUT_MOUSE_MOVE) {
        if (last_mx != -1) {
            int dx = ev->mouse_x - last_mx;
            int dy = ev->mouse_y - last_my;
            if (dx != 0 || dy != 0) {
                DG_MouseMove(dx, dy);
            }
        }
        last_mx = ev->mouse_x;
        last_my = ev->mouse_y;
    }
    else if (ev->type == INPUT_MOUSE_CLICK) {
        /* Map Left(1) -> 0, Right(2) -> 1 for Doom? Or just pass raw */
        DG_MouseButton(ev->keycode, ev->pressed ? 1 : 0);
        /* Update position on click too */
        last_mx = ev->mouse_x;
        last_my = ev->mouse_y;
    }

    if (ev->type == INPUT_KEYBOARD) {
        /* Map Chrysalis Keys to Doom Keys */
        unsigned char key = 0;
        uint32_t k = ev->keycode;
        
        /* Basic mapping */
        if (k >= 'a' && k <= 'z') key = k - 32; // Doom uses UPPERCASE for ASCII keys usually, or scancodes
        else if (k >= 'A' && k <= 'Z') key = k;
        else if (k >= '0' && k <= '9') key = k;
        
        /* WASD -> Arrows Mapping */
        else if (k == 'w' || k == 'W') key = 0xAD; // KEY_UPARROW
        else if (k == 's' || k == 'S') key = 0xAF; // KEY_DOWNARROW
        else if (k == 'a' || k == 'A') key = 0xAC; // KEY_LEFTARROW
        else if (k == 'd' || k == 'D') key = 0xAE; // KEY_RIGHTARROW
        
        else if (k == 0x48) key = 0xAD; // Up Arrow (KEY_UPARROW)
        else if (k == 0x50) key = 0xAF; // Down Arrow (KEY_DOWNARROW)
        else if (k == 0x4B) key = 0xAC; // Left Arrow (KEY_LEFTARROW)
        else if (k == 0x4D) key = 0xAE; // Right Arrow (KEY_RIGHTARROW)
        else if (k == '\n') key = 13;   // Enter (KEY_ENTER)
        else if (k == 0x39) key = ' ';  // Space
        else if (k == 0x1D) key = 0x80; // Ctrl (Fire)
        else if (k == 0x01) key = 27;   // Esc
        
        if (key) {
            if (ev->pressed) DG_KeyDown(key);
            else DG_KeyUp(key);
            return true;
        }
    }
    
    return false;
}

window_t* doom_app_get_window(void) {
    return doom_get_window();
}