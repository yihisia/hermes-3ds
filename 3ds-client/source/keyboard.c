/**
 * Virtual keyboard for Hermes 3DS Client
 * Touch-screen based QWERTY keyboard on the lower screen
 * Lower screen is 320x240
 */

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "keyboard.h"

// Keyboard layout
// Lower screen: 320x240, keyboard occupies bottom portion
#define KB_X_OFFSET 5
#define KB_Y_OFFSET 100
#define KEY_W 28
#define KEY_H 26

// QWERTY layout
static const char* kb_rows[] = {
    "1234567890",
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm",
    " .,!?"
};

static const int kb_row_lengths[] = {10, 10, 9, 7, 5};
static const int kb_row_offsets[] = {0, 14, 28, 56, 84}; // X offsets per row

// Current shift/caps state
static bool kb_shift = false;
static int kb_page = 0; // 0 = letters, 1 = symbols

// Symbol page
static const char* kb_sym_rows[] = {
    "!@#$%^&*()",
    "-_=+[]{}|",
    ";:'\",.<>/",
    "`~\\? SP"
};
static const int kb_sym_row_lengths[] = {10, 9, 9, 7};

void keyboard_draw(void) {
    // Draw keyboard using console characters
    // Position: row 6 onwards on bottom screen
    int row_y = 6;
    
    if (kb_page == 0) {
        // Letter page
        for (int r = 0; r < 5; r++) {
            printf("\x1b[%d;1H", row_y + r);
            int x_off = kb_row_offsets[r] / 8; // rough char offset
            
            for (int c = 0; c < kb_row_lengths[r]; c++) {
                char ch = kb_rows[r][c];
                if (kb_shift && ch >= 'a' && ch <= 'z') {
                    ch -= 32; // uppercase
                }
                printf("[%c]", ch);
            }
            printf("\n");
        }
    } else {
        // Symbol page
        for (int r = 0; r < 4; r++) {
            printf("\x1b[%d;1H", row_y + r);
            for (int c = 0; c < kb_sym_row_lengths[r]; c++) {
                printf("[%c]", kb_sym_rows[r][c]);
            }
            printf("\n");
        }
    }
    
    // Action buttons
    printf("\x1b[%d;1H", row_y + 5);
    printf("[SHIFT] [SYM] [<-] [SEND]\n");
    
    // Space bar
    printf("\x1b[%d;1H", row_y + 6);
    printf("[          SPACE          ]\n");
}

char keyboard_get_char(int touch_x, int touch_y) {
    // Check if touch is in keyboard area
    if (touch_y < KB_Y_OFFSET) return 0;
    
    int rel_y = touch_y - KB_Y_OFFSET;
    int row = rel_y / KEY_H;
    
    if (row < 0) return 0;
    
    if (kb_page == 0) {
        // Letter page
        if (row >= 5) {
            // Action row
            int rel_x = touch_x - KB_X_OFFSET;
            if (rel_x >= 0 && rel_x < 56) {
                // SHIFT
                kb_shift = !kb_shift;
                return 0;
            } else if (rel_x >= 56 && rel_x < 112) {
                // SYM toggle
                kb_page = 1;
                return 0;
            } else if (rel_x >= 112 && rel_x < 168) {
                // Backspace - handled in main
                return 8; // ASCII backspace
            } else {
                // SEND - handled in main
                return 13; // ASCII enter/confirm
            }
        }
        
        if (row >= 5) return 0;
        
        // Check for space bar area
        if (touch_y > KB_Y_OFFSET + 5 * KEY_H + 10 && 
            touch_y < KB_Y_OFFSET + 6 * KEY_H + 10) {
            return ' ';
        }
        
        if (row < 5) {
            int rel_x = touch_x - KB_X_OFFSET - kb_row_offsets[row];
            int col = rel_x / KEY_W;
            
            if (col >= 0 && col < kb_row_lengths[row]) {
                char ch = kb_rows[row][col];
                if (kb_shift && ch >= 'a' && ch <= 'z') {
                    ch -= 32;
                    kb_shift = false; // Auto-unshift after one char
                }
                return ch;
            }
        }
    } else {
        // Symbol page
        if (row >= 4) {
            // Action row - check for page switch back
            kb_page = 0;
            return 0;
        }
        
        if (row < 4) {
            int rel_x = touch_x - KB_X_OFFSET;
            int col = rel_x / KEY_W;
            
            if (col >= 0 && col < kb_sym_row_lengths[row]) {
                return kb_sym_rows[row][col];
            }
        }
    }
    
    return 0;
}

void keyboard_reset(void) {
    kb_shift = false;
    kb_page = 0;
}
