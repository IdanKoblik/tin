#pragma once

#include <string.h>

enum Capability {
    AUDIO_CAPTURE = 1 << 0,
    AUDIO_PLAYBACK = 1 << 1,
    INPUT = 1 << 2
};

#define AUDIO_ANY (AUDIO_CAPTURE | AUDIO_PLAYBACK)

enum Capability string_to_capability(const char *str);
