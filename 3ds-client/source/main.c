/**
 * Hermes 3DS Client - Connect your 3DS to Hermes AI
 * 
 * Upper screen: Display Hermes responses
 * Lower screen: Virtual keyboard + voice input button
 * 
 * Build: make (requires devkitPro + libctru)
 */

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "ui.h"
#include "net.h"
#include "audio.h"
#include "keyboard.h"

// Forward declarations
void unescape_newlines(char* str);

// Server address (change to your Mac's IP)
#define SERVER_HOST "192.168.1.100"
#define SERVER_PORT 8333

// App state
typedef enum {
    STATE_IDLE,
    STATE_TYPING,
    STATE_RECORDING,
    STATE_SENDING,
    STATE_ERROR
} AppState;

static AppState g_state = STATE_IDLE;
static char g_input_buffer[512] = {0};
static int g_input_pos = 0;
static char g_response[2048] = {0};
static char g_status[128] = "Ready";
static bool g_running = true;

// Response scroll offset
static int g_scroll_offset = 0;
static int g_response_lines = 0;

void update_status(const char* msg) {
    strncpy(g_status, msg, sizeof(g_status) - 1);
}

void send_message(const char* message) {
    if (strlen(message) == 0) return;
    
    g_state = STATE_SENDING;
    update_status("Sending...");
    
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/chat", SERVER_HOST, SERVER_PORT);
    
    char post_data[600];
    snprintf(post_data, sizeof(post_data), "message=%s&history=true", message);
    
    int ret = http_post(url, post_data, g_response, sizeof(g_response));
    
    if (ret == 0) {
        // Parse JSON response to extract "response" field
        char* resp_start = strstr(g_response, "\"response\":");
        if (resp_start) {
            resp_start += 12; // skip "response":"
            char* resp_end = strstr(resp_start, "\",\"raw\"");
            if (resp_end) {
                int len = resp_end - resp_start;
                memmove(g_response, resp_start, len);
                g_response[len] = '\0';
                // Unescape \\n
                unescape_newlines(g_response);
            }
        }
        
        // Count lines for scrolling
        g_response_lines = 1;
        for (int i = 0; g_response[i]; i++) {
            if (g_response[i] == '\n') g_response_lines++;
        }
        g_scroll_offset = 0;
        update_status("Done");
    } else {
        snprintf(g_response, sizeof(g_response), "Network error (%d)", ret);
        update_status("Error");
        g_state = STATE_ERROR;
        return;
    }
    
    g_state = STATE_IDLE;
    g_input_buffer[0] = '\0';
    g_input_pos = 0;
}

void send_voice(void) {
    g_state = STATE_RECORDING;
    update_status("Recording... Press B to stop");
    
    // Record audio (up to 5 seconds)
    static u8 audio_buffer[160000]; // 5 sec at 16kHz mono 16bit
    int audio_len = audio_record(audio_buffer, sizeof(audio_buffer), 5);
    
    if (audio_len <= 0) {
        update_status("Record failed");
        g_state = STATE_IDLE;
        return;
    }
    
    g_state = STATE_SENDING;
    update_status("Transcribing...");
    
    // Send audio to server
    char url[256];
    snprintf(url, sizeof(url), "http://%s:%d/voice", SERVER_HOST, SERVER_PORT);
    
    int ret = http_post_audio(url, audio_buffer, audio_len, g_response, sizeof(g_response));
    
    if (ret == 0) {
        // Parse response
        char* resp_start = strstr(g_response, "\"response\":");
        if (resp_start) {
            resp_start += 12;
            char* resp_end = strstr(resp_start, "\",\"raw\"");
            if (resp_end) {
                int len = resp_end - resp_start;
                memmove(g_response, resp_start, len);
                g_response[len] = '\0';
                unescape_newlines(g_response);
            }
        }
        
        g_response_lines = 1;
        for (int i = 0; g_response[i]; i++) {
            if (g_response[i] == '\n') g_response_lines++;
        }
        g_scroll_offset = 0;
        update_status("Done (voice)");
    } else {
        snprintf(g_response, sizeof(g_response), "Voice send error (%d)", ret);
        update_status("Error");
    }
    
    g_state = STATE_IDLE;
}

void unescape_newlines(char* str) {
    int r = 0, w = 0;
    while (str[r]) {
        if (str[r] == '\\' && str[r+1] == 'n') {
            str[w++] = '\n';
            r += 2;
        } else {
            str[w++] = str[r++];
        }
    }
    str[w] = '\0';
}

int main(int argc, char** argv) {
    // Initialize services
    gfxInitDefault();
    ui_init();
    
    // Initialize networking
    if (net_init() != 0) {
        printf("Failed to init network!\n");
        printf("Check WiFi connection.\n");
        svcSleepThread(3000000000ULL);
        goto cleanup;
    }
    
    // Initialize audio
    audio_init();
    
    // Check server connectivity
    update_status("Checking server...");
    if (http_health_check(SERVER_HOST, SERVER_PORT) != 0) {
        snprintf(g_status, sizeof(g_status), "Server offline! Check IP.");
    } else {
        update_status("Connected! Type or press Y for voice.");
    }
    
    g_state = STATE_IDLE;
    
    // Main loop
    while (aptMainLoop() && g_running) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        u32 kUp = hidKeysUp();
        
        // Exit
        if (kDown & KEY_START) {
            g_running = false;
            break;
        }
        
        // Handle input based on state
        switch (g_state) {
            case STATE_IDLE:
            case STATE_TYPING:
                // Virtual keyboard input
                if (kDown & KEY_A) {
                    // Confirm / send
                    if (g_input_pos > 0) {
                        send_message(g_input_buffer);
                    }
                }
                // Voice recording
                else if (kDown & KEY_Y) {
                    send_voice();
                }
                // Backspace
                else if (kDown & KEY_B) {
                    if (g_input_pos > 0) {
                        g_input_pos--;
                        g_input_buffer[g_input_pos] = '\0';
                    }
                }
                // Scroll response
                else if (kHeld & KEY_DUP) {
                    if (g_scroll_offset > 0) g_scroll_offset--;
                }
                else if (kHeld & KEY_DDOWN) {
                    if (g_scroll_offset < g_response_lines - 15) {
                        g_scroll_offset++;
                    }
                }
                // Touch keyboard
                else if (kDown & KEY_TOUCH || kHeld & KEY_TOUCH) {
                    touchPosition touch;
                    hidTouchRead(&touch);
                    char ch = keyboard_get_char(touch.px, touch.py);
                    if (ch != 0 && g_input_pos < (int)sizeof(g_input_buffer) - 1) {
                        g_input_buffer[g_input_pos++] = ch;
                        g_input_buffer[g_input_pos] = '\0';
                        g_state = STATE_TYPING;
                    }
                }
                break;
                
            case STATE_RECORDING:
                if (kDown & KEY_B) {
                    // Stop recording (handled in send_voice)
                }
                break;
                
            case STATE_SENDING:
                // Just show loading, wait
                break;
                
            case STATE_ERROR:
                if (kDown & KEY_A || kDown & KEY_B) {
                    g_state = STATE_IDLE;
                    update_status("Ready");
                }
                break;
        }
        
        // Render
        ui_draw(g_state, g_input_buffer, g_response, g_status, g_scroll_offset);
        
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

cleanup:
    audio_exit();
    net_exit();
    gfxExit();
    return 0;
}
