#ifndef AUDIO_H
#define AUDIO_H

#include <3ds.h>

int audio_init(void);
void audio_exit(void);

/**
 * Record audio from 3DS microphone.
 * Returns number of bytes recorded, or negative on error.
 * Records 16kHz mono 16-bit PCM.
 * Stops early if user presses B or silence is detected.
 */
int audio_record(u8* buffer, int buffer_size, int max_seconds);

#endif // AUDIO_H
