/**
 * Audio recording module for Hermes 3DS Client
 * Records from 3DS microphone (16kHz mono 16-bit PCM)
 */

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "audio.h"

#define SAMPLE_RATE 16000
#define SAMPLES_PER_FRAME (SAMPLE_RATE / 50) // 50fps = 320 samples/frame

static bool g_audio_initialized = false;

int audio_init(void) {
    Result ret = micInit();
    if (R_FAILED(ret)) {
        return -1;
    }
    g_audio_initialized = true;
    return 0;
}

void audio_exit(void) {
    if (g_audio_initialized) {
        micExit();
        g_audio_initialized = false;
    }
}

int audio_record(u8* buffer, int buffer_size, int max_seconds) {
    if (!g_audio_initialized) return -1;
    
    u32 sample_pos = 0;
    u32 total_samples = SAMPLE_RATE * max_seconds;
    
    // Start mic
    MICU_SetPower(true);
    MICU_SetGain(3); // Medium-high gain
    
    // Configure sampling
    u32 buf_size = SAMPLES_PER_FRAME * 2 * 4; // 16-bit, 4 frames buffer
    u32* mic_buf = linearAlloc(buf_size);
    if (!mic_buf) return -2;
    
    Result ret = MICU_StartSampling(
        MICU_SAMPLE_RATE_16384, // Closest to 16kHz
        MICU_MONO,
        0, // offset
        SAMPLES_PER_FRAME * 2 * 4,
        false // loop
    );
    
    if (R_FAILED(ret)) {
        linearFree(mic_buf);
        return -3;
    }
    
    int total_bytes = 0;
    int silence_frames = 0;
    bool started = false;
    
    while (total_bytes < buffer_size && (total_bytes / 2) < (int)total_samples) {
        // Check if user pressed B to stop
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_B && started) break;
        
        // Wait for audio data
        svcSleepThread(20000000ULL); // 20ms
        
        u32 samples_read;
        ret = MICU_Read((void*)mic_buf, buf_size, &samples_read);
        
        if (R_SUCCEEDED(ret) && samples_read > 0) {
            // Copy samples to output buffer
            int bytes_to_copy = samples_read * 2; // 16-bit samples
            if (total_bytes + bytes_to_copy > buffer_size) {
                bytes_to_copy = buffer_size - total_bytes;
            }
            
            // Convert from mic buffer (might be interleaved differently)
            u16* src = (u16*)mic_buf;
            int num_samples = samples_read;
            
            for (int i = 0; i < num_samples && total_bytes + 2 <= buffer_size; i++) {
                s16 sample = (s16)src[i];
                
                // Simple voice activity detection
                if (sample > 500 || sample < -500) {
                    started = true;
                    silence_frames = 0;
                } else if (started) {
                    silence_frames++;
                }
                
                // Stop after 1 second of silence
                if (silence_frames > 50) break;
                
                // Write sample (little-endian 16-bit)
                buffer[total_bytes] = sample & 0xFF;
                buffer[total_bytes + 1] = (sample >> 8) & 0xFF;
                total_bytes += 2;
            }
            
            if (silence_frames > 50) break;
        }
        
        // Yield to system
        gspWaitForVBlank();
    }
    
    // Stop sampling
    MICU_StopSampling();
    MICU_SetPower(false);
    
    linearFree(mic_buf);
    
    return total_bytes;
}
