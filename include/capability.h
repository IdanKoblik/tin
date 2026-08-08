#pragma once

#include <string.h>

enum Capability {
    AUDIO_CAPTURE = 1 << 0,
    AUDIO_PLAYBACK = 1 << 1,
    INPUT_SEND = 1 << 2,
    INPUT_RECEIVE = 1 << 3
};

#define AUDIO_ANY (AUDIO_CAPTURE | AUDIO_PLAYBACK)
#define INPUT_ANY (INPUT_SEND | INPUT_RECEIVE)

// Audio and input are two independent links, and a node only ever sits on one
// end of each: mic talks to the peer's speaker, input-send to its input-recv.
// Holding both ends of a link is always a misconfiguration.
int capability_is_consistent(enum Capability caps);

enum Capability string_to_capability(const char *str);
