#include "tic_tac_toe_app.h"
#include "../ui/wm/wm.h"
#include "../ui/flyui/draw.h"
#include "../ui/flyui/theme.h"
#include "../mem/kmalloc.h"
#include "../string.h"
#include "app_manager.h"

static window_t* ttt_win = NULL;
static int board[3][3];
static int turn = 0; // 0 = X, 1 = O
static int winner = -1; // -1 = Playing, 0 = X, 1 = O, 2 = Draw
static int moves = 0;

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

static void reset_game() {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            board[i][j] = -1;
    turn = 0;
    winner = -1;
    moves = 0;
}

static void check_winner() {
    // Rows & Cols
    for (int i = 0; i < 3; i++) {
        if (board[i][0] != -1 && board[i][0] == board[i][1] && board[i][1] == board[i][2]) winner = board[i][0];
        if (board[0][i] != -1 && board[0][i] == board[1][i] && board[1][i] == board[2][i]) winner = board[0][i];
    }
    // Diagonals
    if (board[0][0] != -1 && board[0][0] == board[1][1] && board[1][1] == board[2][2]) winner = board[0][0];
    if (board[0][2] != -1 && board[0][2] == board[1][1] && board[1][1] == board[2][0]) winner = board[0][2];

    if (winner == -1 && moves == 9) winner = 2; // Draw
}

static void draw_board(surface_t* s) {
    fly_theme_t* th = theme_get();
    
    // Clear background
    fly_draw_rect_fill(s, 0, 25, s->width, s->height - 25, th->win_bg);

    int cell_w = 60;
    int cell_h = 60;
    int off_x = 10;
    int off_y = 35;

    // Draw Grid
    for (int i = 1; i < 3; i++) {
        fly_draw_rect_fill(s, off_x + i * cell_w, off_y, 2, 3 * cell_h, 0xFF000000);
        fly_draw_rect_fill(s, off_x, off_y + i * cell_h, 3 * cell_w, 2, 0xFF000000);
    }

    // Draw Pieces
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            int cx = off_x + c * cell_w + 10;
            int cy = off_y + r * cell_h + 10;
            
            if (board[r][c] == 0) { // X
                // Draw X using lines
                for(int k=0; k<3; k++) { // Thickness
                    fly_draw_line(s, cx+k, cy, cx + 40+k, cy + 40, 0xFFFF0000);
                    fly_draw_line(s, cx + 40+k, cy, cx+k, cy + 40, 0xFFFF0000);
                }
            } else if (board[r][c] == 1) { // O
                fly_draw_rect_outline(s, cx, cy, 40, 40, 0xFF0000FF);
                fly_draw_rect_outline(s, cx+1, cy+1, 38, 38, 0xFF0000FF);
            }
        }
    }

    // Status Text
    const char* status = "Player X Turn";
    if (turn == 1) status = "Player O Turn";
    if (winner == 0) status = "Winner: X!";
    else if (winner == 1) status = "Winner: O!";
    else if (winner == 2) status = "Draw!";

    fly_draw_text(s, 10, 220, status, th->color_text);
    
    if (winner != -1) {
        fly_draw_text(s, 10, 240, "Click to Restart", 0xFF008000);
    }
}

void tic_tac_toe_app_create(void) {
    if (ttt_win) return;

    fly_theme_t* th = theme_get();
    int w = 200;
    int h = 260;
    surface_t* s = surface_create(w, h);
    if (!s) return;

    // Window Decoration
    fly_draw_rect_fill(s, 0, 0, w, 24, th->win_title_active_bg);
    fly_draw_text(s, 5, 4, "X and 0", th->win_title_active_fg);
    
    // Close Button
    int bx = w - 20;
    fly_draw_rect_fill(s, bx, 4, 16, 16, th->win_bg);
    fly_draw_rect_outline(s, bx, 4, 16, 16, th->color_lo_2);
    fly_draw_text(s, bx + 4, 4, "X", th->color_text);

    // Border
    fly_draw_rect_fill(s, 0, 24, w, 1, th->color_lo_1);

    reset_game();
    draw_board(s);

    ttt_win = wm_create_window(s, 100, 100);
    app_register("X and 0", ttt_win);
}

bool tic_tac_toe_app_handle_event(input_event_t* ev) {
    if (!ttt_win) return false;

    if (ev->type == INPUT_MOUSE_CLICK && ev->pressed) {
        int lx = ev->mouse_x - ttt_win->x;
        int ly = ev->mouse_y - ttt_win->y;

        // Close Button
        if (lx >= ttt_win->w - 20 && ly < 24) {
            wm_destroy_window(ttt_win);
            app_unregister(ttt_win);
            ttt_win = NULL;
            wm_mark_dirty();
            return true;
        }

        // Game Logic
        if (ly > 25) {
            if (winner != -1) {
                reset_game();
                draw_board(ttt_win->surface);
                wm_mark_dirty();
                return true;
            }

            int off_x = 10;
            int off_y = 35;
            int cell_w = 60;
            int cell_h = 60;

            if (lx >= off_x && lx < off_x + 3 * cell_w &&
                ly >= off_y && ly < off_y + 3 * cell_h) {
                
                int c = (lx - off_x) / cell_w;
                int r = (ly - off_y) / cell_h;

                if (r >= 0 && r < 3 && c >= 0 && c < 3 && board[r][c] == -1) {
                    board[r][c] = turn;
                    moves++;
                    check_winner();
                    if (winner == -1) {
                        turn = !turn;
                    }
                    draw_board(ttt_win->surface);
                    wm_mark_dirty();
                }
            }
        }
    }
    return false;
}

window_t* tic_tac_toe_app_get_window(void) {
    return ttt_win;
}