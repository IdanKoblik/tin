#include "capability.h"
#include "logging/log.h"

static enum Capability name_to_capability(const char *name, size_t len) {
    if (len == strlen("mic") && !strncmp(name, "mic", len))
        return AUDIO_CAPTURE;

    if (len == strlen("speaker") && !strncmp(name, "speaker", len))
        return AUDIO_PLAYBACK;

    if (len == strlen("input-send") && !strncmp(name, "input-send", len))
        return INPUT_SEND;

    if (len == strlen("input-recv") && !strncmp(name, "input-recv", len))
        return INPUT_RECEIVE;

    return 0;
}

enum Capability string_to_capability(const char *str) {
    int caps = 0;

    while (*str) {
        const char *comma = strchr(str, ',');
        size_t len = comma ? (size_t)(comma - str) : strlen(str);

        enum Capability cap = name_to_capability(str, len);
        if (!cap)
            return 0;

        caps |= cap;
        str += len;
        if (comma)
            str++;
    }

    if (!capability_is_consistent(caps))
        return 0;

    return caps;
}

int capability_is_consistent(enum Capability caps) {
    if ((caps & AUDIO_ANY) == AUDIO_ANY) {
        ERROR("A node cannot be both mic and speaker, its peer takes the other end");
        return 0;
    }

    if ((caps & INPUT_ANY) == INPUT_ANY) {
        ERROR("A node cannot be both input-send and input-recv, its peer takes the other end");
        return 0;
    }

    return 1;
}
