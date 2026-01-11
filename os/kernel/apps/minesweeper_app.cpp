#include "minesweeper_app.h"
#include "../ui/wm/wm.h"
#include "../ui/flyui/draw.h"
#include "../mem/kmalloc.h"
#include "../string.h"
#include "../time/clock.h"

/* Game Constants */
#define GRID_W 15
#define GRID_H 15
#define MINES_COUNT 30
#define CELL_SIZE 20
#define TITLE_BAR_H 24
#define HEADER_H 30
#define WIN_W (GRID_W * CELL_SIZE)
#define WIN_H (TITLE_BAR_H + HEADER_H + GRID_H * CELL_SIZE)

/* Colors */
#define COL_BG       0xFFC0C0C0
#define COL_LIGHT    0xFFFFFFFF
#define COL_SHADOW   0xFF808080
#define COL_TEXT     0xFF000000
#define COL_HIDDEN   0xFFC0C0C0
#define COL_REVEALED 0xFFC0C0C0 /* Flat */
#define COL_MINE_BG  0xFFFF0000
#define COL_TITLE_BG 0xFF000080
#define COL_TITLE_FG 0xFFFFFFFF

/* RNG */
static unsigned long int next = 1;
static int rand(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next/65536) % 32768;
}
static void srand(unsigned int seed) {
    next = seed;
}

typedef struct {
    bool is_mine;
    bool revealed;
    bool flagged;
    int neighbors;
} cell_t;

static window_t* mine_win = NULL;
static cell_t grid[GRID_W * GRID_H];
static bool game_over = false;
static bool game_won = false;
static int score = 0;

/* --- Drawing Helpers --- */

static void draw_bevel(surface_t* s, int x, int y, int w, int h, bool pressed) {
    uint32_t top = pressed ? COL_SHADOW : COL_LIGHT;
    uint32_t bot = pressed ? COL_LIGHT : COL_SHADOW;
    
    fly_draw_rect_fill(s, x, y, w, h, COL_BG);
    fly_draw_rect_fill(s, x, y, w, 1, top);
    fly_draw_rect_fill(s, x, y, 1, h, top);
    fly_draw_rect_fill(s, x + w - 1, y, 1, h, bot);
    fly_draw_rect_fill(s, x, y + h - 1, w, 1, bot);
}

static void draw_cell(surface_t* s, int cx, int cy) {
    int idx = cy * GRID_W + cx;
    cell_t* c = &grid[idx];
    int x = cx * CELL_SIZE;
    int y = TITLE_BAR_H + HEADER_H + cy * CELL_SIZE;

    if (!c->revealed) {
        draw_bevel(s, x, y, CELL_SIZE, CELL_SIZE, false);
        if (c->flagged) {
            fly_draw_text(s, x + 6, y + 2, "F", 0xFFFF0000);
        }
        if (game_over && c->is_mine && !c->flagged) {
             /* Show mines on loss */
             fly_draw_text(s, x + 6, y + 2, "*", 0xFF000000);
        }
    } else {
        /* Revealed */
        fly_draw_rect_fill(s, x, y, CELL_SIZE, CELL_SIZE, COL_BG);
        fly_draw_rect_outline(s, x, y, CELL_SIZE, CELL_SIZE, COL_SHADOW);
        
        if (c->is_mine) {
            fly_draw_rect_fill(s, x + 1, y + 1, CELL_SIZE - 2, CELL_SIZE - 2, COL_MINE_BG);
            fly_draw_text(s, x + 6, y + 2, "*", 0xFF000000);
        } else if (c->neighbors > 0) {
            uint32_t col = 0xFF000000;
            if (c->neighbors == 1) col = 0xFF0000FF;
            else if (c->neighbors == 2) col = 0xFF008000;
            else if (c->neighbors == 3) col = 0xFFFF0000;
            else if (c->neighbors == 4) col = 0xFF000080;
            
            char buf[2] = { (char)('0' + c->neighbors), 0 };
            fly_draw_text(s, x + 6, y + 2, buf, col);
        }
    }
}

static void draw_face(surface_t* s) {
    int bx = WIN_W / 2 - 10;
    int by = TITLE_BAR_H + 5;
    draw_bevel(s, bx, by, 20, 20, false);
    const char* face = game_over ? (game_won ? ":)" : "X(") : ":)";
    fly_draw_text(s, bx + 4, by + 2, face, COL_TEXT);
}

static void draw_score(surface_t* s) {
    char buf[16];
    itoa_dec(buf, score);
    
    /* Clear score area */
    fly_draw_rect_fill(s, 5, TITLE_BAR_H + 5, 80, 20, COL_BG);
    
    fly_draw_text(s, 5, TITLE_BAR_H + 8, "Score:", COL_TEXT);
    fly_draw_text(s, 55, TITLE_BAR_H + 8, buf, 0xFF000080);
}

static void render_all() {
    if (!mine_win) return;
    surface_t* s = mine_win->surface;
    
    /* Title Bar */
    fly_draw_rect_fill(s, 0, 0, WIN_W, TITLE_BAR_H, COL_TITLE_BG);
    fly_draw_text(s, 5, 4, "Minesweeper", COL_TITLE_FG);
    
    /* Close Button */
    int bx = WIN_W - 20;
    fly_draw_rect_fill(s, bx, 4, 16, 16, COL_BG);
    fly_draw_rect_outline(s, bx, 4, 16, 16, COL_SHADOW);
    fly_draw_text(s, bx + 4, 4, "X", COL_TEXT);

    /* Header */
    fly_draw_rect_fill(s, 0, TITLE_BAR_H, WIN_W, HEADER_H, COL_BG);
    draw_face(s);
    draw_score(s);
    
    /* Grid */
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            draw_cell(s, x, y);
        }
    }
    wm_mark_dirty();
}

/* --- Game Logic --- */

static void init_game() {
    memset(grid, 0, sizeof(grid));
    game_over = false;
    game_won = false;
    score = 0;

    /* Seed RNG */
    datetime t;
    time_get_local(&t);
    srand(t.second + t.minute * 60 + t.hour * 3600);

    /* Place Mines */
    int mines_placed = 0;
    while (mines_placed < MINES_COUNT) {
        int idx = rand() % (GRID_W * GRID_H);
        if (!grid[idx].is_mine) {
            grid[idx].is_mine = true;
            mines_placed++;
        }
    }

    /* Calc Neighbors */
    for (int y = 0; y < GRID_H; y++) {
        for (int x = 0; x < GRID_W; x++) {
            if (grid[y * GRID_W + x].is_mine) continue;
            
            int n = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H) {
                        if (grid[ny * GRID_W + nx].is_mine) n++;
                    }
                }
            }
            grid[y * GRID_W + x].neighbors = n;
        }
    }
}

static void reveal(int x, int y) {
    if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H) return;
    cell_t* c = &grid[y * GRID_W + x];
    
    if (c->revealed || c->flagged) return;
    
    c->revealed = true;
    score += 10; /* Points for revealing safe cell */
    
    if (c->neighbors == 0 && !c->is_mine) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx != 0 || dy != 0) reveal(x + dx, y + dy);
            }
        }
    }
}

static void check_win() {
    int revealed_count = 0;
    for (int i = 0; i < GRID_W * GRID_H; i++) {
        if (grid[i].revealed) revealed_count++;
    }
    if (revealed_count == (GRID_W * GRID_H - MINES_COUNT)) {
        game_over = true;
        game_won = true;
        score += 1000; /* Bonus for winning */
    }
}

/* --- Public API --- */

void minesweeper_app_create(void) {
    if (mine_win) return;

    surface_t* s = surface_create(WIN_W, WIN_H);
    if (!s) return;

    init_game();
    
    /* Center on screen */
    mine_win = wm_create_window(s, 200, 200);
    render_all();
}

window_t* minesweeper_app_get_window(void) {
    return mine_win;
}

void minesweeper_app_handle_event(input_event_t* ev) {
    if (!mine_win) return;

    if (ev->type == INPUT_MOUSE_CLICK && ev->pressed) {
        int lx = ev->mouse_x - mine_win->x;
        int ly = ev->mouse_y - mine_win->y;

        /* Check Close Button */
        if (ly >= 4 && ly <= 20 && lx >= WIN_W - 20 && lx <= WIN_W - 4) {
            wm_destroy_window(mine_win);
            mine_win = NULL;
            return;
        }

        /* Check Reset Button */
        int bx = WIN_W / 2 - 10;
        int by = TITLE_BAR_H + 5;
        if (ly >= by && ly <= by + 20 && lx >= bx && lx <= bx + 20) {
            init_game();
            render_all();
            return;
        }

        /* Check Grid */
        int grid_y = TITLE_BAR_H + HEADER_H;
        if (ly >= grid_y && !game_over) {
            int cx = lx / CELL_SIZE;
            int cy = (ly - grid_y) / CELL_SIZE;
            
            if (cx >= 0 && cx < GRID_W && cy >= 0 && cy < GRID_H) {
                cell_t* c = &grid[cy * GRID_W + cx];
                
                /* Left Click: Reveal */
                if (ev->keycode == 1) { 
                    if (!c->flagged) {
                        if (c->is_mine) {
                            c->revealed = true;
                            game_over = true;
                            game_won = false;
                        } else {
                            reveal(cx, cy);
                            check_win();
                        }
                    }
                }
                /* Right Click: Flag */
                else if (ev->keycode == 2) {
                    if (!c->revealed) {
                        c->flagged = !c->flagged;
                    }
                }
                render_all();
            }
        }
    }
}