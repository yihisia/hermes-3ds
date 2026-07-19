/**
 * UI rendering for Hermes 3DS Client
 * Upper screen: response display
 * Lower screen: virtual keyboard + status
 */

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "keyboard.h"

static PrintConsole topConsole;
static PrintConsole bottomConsole;

void ui_init(void) {
    consoleInit(GFX_TOP, &topConsole);
    consoleInit(GFX_BOTTOM, &bottomConsole);
}

void ui_draw(int state, const char* input, const char* response, 
             const char* status, int scroll_offset) {
    
    // --- Upper screen: Response ---
    consoleSelect(&topConsole);
    consoleClear();
    
    printf("\x1b[1;1H"); // Move to top-left
    printf("\x1b[36m"); // Cyan
    printf("===== Hermes 3DS =====\n");
    printf("\x1b[37m"); // White
    
    if (strlen(response) == 0) {
        printf("\n\n  No response yet.\n");
        printf("  Type a message on the\n");
        printf("  bottom screen and press A.\n\n");
        printf("  Or press Y for voice input.\n");
    } else {
        // Display response with scrolling
        int line = 0;
        const char* p = response;
        while (*p && line < scroll_offset) {
            if (*p == '\n') line++;
            p++;
        }
        
        line = 0;
        while (*p && line < 20) {
            putchar(*p);
            if (*p == '\n') line++;
            p++;
        }
    }
    
    // Scroll indicator
    if (scroll_offset > 0) {
        printf("\x1b[24;1H\x1b[33m[Scroll UP for more]\x1b[37m");
    }
    
    // --- Lower screen: Input + Keyboard ---
    consoleSelect(&bottomConsole);
    consoleClear();
    
    // Status bar
    printf("\x1b[1;1H\x1b[32m");
    printf("[%s]\x1b[37m\n", status);
    
    // Input field
    printf("\x1b[2;1H> ");
    if (strlen(input) > 36) {
        // Show last 36 chars
        printf("%s", input + strlen(input) - 36);
    } else {
        printf("%s", input);
    }
    printf("_\n");
    
    // Separator
    printf("\x1b[4;1H\x1b[33m");
    printf("----------------------------\n");
    printf("\x1b[37m");
    
    // State-dependent info
    if (state == 3) { // STATE_SENDING
        printf("\n  Sending to Hermes...\n");
        printf("  Please wait.\n");
    } else if (state == 2) { // STATE_RECORDING
        printf("\n  Recording audio...\n");
        printf("  Press B to stop.\n");
    } else {
        // Draw virtual keyboard
        keyboard_draw();
    }
    
    // Button hints at bottom
    printf("\x1b[28;1H\x1b[36m");
    printf("A:Send B:Del Y:Voice START:Quit\n");
    printf("\x1b[37m");
}

void ui_show_error(const char* msg) {
    consoleSelect(&topConsole);
    printf("\x1b[31mERROR: %s\x1b[37m\n", msg);
}
