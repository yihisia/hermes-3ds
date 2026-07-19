/**
 * Audio recording module for Hermes 3DS Client
 * STUB: voice recording disabled for now
 */

#include <3ds.h>
#include <string.h>
#include "audio.h"

int audio_init(void) {
    // Stub: voice not implemented yet
    return 0;
}

void audio_exit(void) {
    // Stub
}

int audio_record(u8* buffer, int buffer_size, int max_seconds) {
    (void)buffer;
    (void)buffer_size;
    (void)max_seconds;
    // Stub: return -1 to indicate not available
    return -1;
}
