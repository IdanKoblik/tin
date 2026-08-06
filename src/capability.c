#include "capability.h"

static enum Capability name_to_capability(const char *name, size_t len) {
    if (len == strlen("mic") && !strncmp(name, "mic", len))
        return AUDIO_CAPTURE;

    if (len == strlen("speaker") && !strncmp(name, "speaker", len))
        return AUDIO_PLAYBACK;

    if (len == strlen("input") && !strncmp(name, "input", len))
        return INPUT;

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

    return caps;
}
