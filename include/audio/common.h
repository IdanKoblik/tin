#pragma once

#include <signal.h>
#include <stddef.h>
#include <stdint.h>

// One frame of 20 ms, 48 kHz, 16-bit mono PCM audio. Every read and write
// moves exactly this many bytes.
#define AUDIO_FRAME_BYTES 1920
#define AUDIO_FRAME_SAMPLES (AUDIO_FRAME_BYTES / (int)sizeof(int16_t))

typedef struct AudioDevice AudioDevice;

AudioDevice *audio_create(void);

int audio_send(AudioDevice *device, const void *buffer, size_t bytes);
int audio_receive(AudioDevice *device, void *buffer, size_t byte);

// RMS amplitude at or below which a frame counts as silence, as a fraction of
// full scale. 0.00316 is -50 dBFS: far below speech, comfortably above the
// noise floor of a typical microphone. Linear so the check needs no libm.
#define AUDIO_SILENCE_RMS 0.00316

int audio_frame_is_silent(const int16_t *samples, size_t count);

void audio_destroy(AudioDevice *device);

struct AudioThread {
    struct Node *node;
    AudioDevice *dev;
};

void *run_audio(void *arg, volatile sig_atomic_t *running);
