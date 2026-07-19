/**
 * Audio recording module for Hermes 3DS Client
 * Records from 3DS microphone (16kHz mono 16-bit PCM)
 */

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "audio.h"

#define SAMPLE_RATE 16360
#define SAMPLES_PER_FRAME 327

static bool g_audio_initialized = false;
static u8* g_mic_buffer = NULL;
static u32 g_mic_buffer_size = 0;

int audio_init(void) {
    // Allocate microphone buffer (2 seconds at 16kHz, 16-bit mono)
    g_mic_buffer_size = SAMPLE_RATE * 2 * 2; // 2 seconds, 16-bit
    g_mic_buffer = linearAlloc(g_mic_buffer_size);
    if (!g_mic_buffer) return -1;
    
    Result ret = micInit(SAMPLE_RATE, SAMPLES_PER_FRAME);
    if (R_FAILED(ret)) {
        linearFree(g_mic_buffer);
        g_mic_buffer = NULL;
        return -1;
    }
    
    g_audio_initialized = true;
    return 0;
}

void audio_exit(void) {
    if (g_audio_initialized) {
        micExit();
        if (g_mic_buffer) {
            linearFree(g_mic_buffer);
            g_mic_buffer = NULL;
        }
        g_audio_initialized = false;
    }
}

int audio_record(u8* buffer, int buffer_size, int max_seconds) {
    if (!g_audio_initialized || !g_mic_buffer) return -1;
    
    u32 total_samples = SAMPLE_RATE * max_seconds;
    u32 total_bytes = 0;
    u32 read_pos = 0;
    
    // Start recording
    Result ret = micStartRecording(g_mic_buffer, g_mic_buffer_size, SAMPLE_RATE, 0, 0, NULL);
    if (R_FAILED(ret)) return -2;
    
    int silence_count = 0;
    bool has_audio = false;
    
    while (total_bytes < (u32)buffer_size && total_bytes < total_samples * 2) {
        svcSleepThread(20000000ULL); // 20ms
        
        // Check if user pressed B to stop
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_B && has_audio) break;
        
        // Get current write position
        u32 write_pos = micGetSamplePos();
        
        // Calculate how much new data is available
        u32 available;
        if (write_pos >= read_pos) {
            available = write_pos - read_pos;
        } else {
            available = g_mic_buffer_size - read_pos + write_pos;
        }
        
        if (available > 0) {
            // Copy samples
            u32 bytes_to_copy = available * 2; // 16-bit samples
            if (total_bytes + bytes_to_copy > (u32)buffer_size) {
                bytes_to_copy = buffer_size - total_bytes;
            }
            
            // Copy with wrap-around handling
            u32 first_chunk = g_mic_buffer_size - read_pos;
            if (first_chunk > available * 2) first_chunk = available * 2;
            
            memcpy(buffer + total_bytes, g_mic_buffer + read_pos, first_chunk);
            total_bytes += first_chunk;
            read_pos = (read_pos + first_chunk) % g_mic_buffer_size;
            
            if (first_chunk < bytes_to_copy) {
                u32 second_chunk = bytes_to_copy - first_chunk;
                memcpy(buffer + total_bytes, g_mic_buffer, second_chunk);
                total_bytes += second_chunk;
                read_pos = second_chunk;
            }
            
            // Simple voice activity detection
            s16* samples = (s16*)(buffer + total_bytes - bytes_to_copy);
            for (u32 i = 0; i < bytes_to_copy / 2; i++) {
                if (samples[i] > 500 || samples[i] < -500) {
                    has_audio = true;
                    silence_count = 0;
                    break;
                }
            }
            
            if (has_audio) {
                silence_count++;
                if (silence_count > 50) break; // 1 second of silence
            }
        }
        
        gspWaitForVBlank();
    }
    
    micStopRecording();
    
    return total_bytes;
}
