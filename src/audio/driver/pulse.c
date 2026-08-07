#ifdef USE_PULSE

#include "pulse/simple.h"
#include "audio/common.h"
#include "logging/log.h"
#include <pulse/def.h>
#include <stdlib.h>

#include <pulse/error.h>
#include <pulse/mainloop.h>
#include <pulse/context.h>
#include <pulse/introspect.h>

static const pa_sample_spec SPEC = {
    .format = PA_SAMPLE_S16LE,
    .rate = 48000,
    .channels = 1,
};

static const pa_buffer_attr PLAYBACK_ATTR = {
    .maxlength = (uint32_t)-1,
    .tlength = AUDIO_FRAME_BYTES * 4, // keep ~80 ms queued to absorb jitter
    .prebuf = AUDIO_FRAME_BYTES * 2,  // start playing once ~40 ms has piled up
    .minreq = AUDIO_FRAME_BYTES,      // refill one frame at a time
    .fragsize = (uint32_t)-1,
};

static const pa_buffer_attr CAPTURE_ATTR = {
    .maxlength = (uint32_t)-1,
    .fragsize = AUDIO_FRAME_BYTES, // exactly one frame per read
    .tlength = (uint32_t)-1,
    .prebuf = (uint32_t)-1,
    .minreq = (uint32_t)-1,
};

struct AudioDevice {
    pa_simple *playback;
    pa_simple *capture;
};

AudioDevice *audio_create(const char *source) {
    AudioDevice *device = malloc(sizeof(*device));
    if (!device) {
        ERROR("Failed to allocate audio device");
        return NULL;
    }

    int error;

    device->playback = pa_simple_new(
        NULL,
        "Tin",
        PA_STREAM_PLAYBACK,
        NULL,
        "Playback",
        &SPEC,
        NULL,
        &PLAYBACK_ATTR,
        &error
    );

    if (!device->playback) {
        ERROR("Failed to create pulse audio playback stream");
        free(device);
        return NULL;
    }

    device->capture = pa_simple_new(
        NULL,
        "Tin",
        PA_STREAM_RECORD,
        source,
        "Capture",
        &SPEC,
        NULL,
        &CAPTURE_ATTR,
        &error
    );

    if (!device->capture) {
        ERROR("Failed to create pulse audio capture stream");
        pa_simple_free(device->playback);
        free(device);
        return NULL;
    }

    return device;
}


int audio_send(AudioDevice *device, const void *buffer, size_t bytes) {
    return pa_simple_write(device->playback, buffer, bytes, NULL);
}

int audio_receive(AudioDevice *device, void *buffer, size_t bytes) {
    return pa_simple_read(device->capture, buffer, bytes, NULL);
}

void audio_destroy(AudioDevice *device) {
    pa_simple_drain(device->playback, NULL);

    pa_simple_free(device->playback);
    pa_simple_free(device->capture);
    free(device);
}

struct SourceCollect {
    struct AudioSource *list;
    size_t count;
    size_t cap;
    int failed; // set when the enumeration could not be completed
};

static void on_source_info(pa_context *c, const pa_source_info *info, int eol, void *userdata) {
    (void)c;
    struct SourceCollect *d = (struct SourceCollect *)userdata;

    if (eol) {
        if (eol < 0)
            d->failed = 1;
        return;
    }

    if (d->count == d->cap) {
        size_t new_cap = d->cap == 0 ? 8 : d->cap * 2;
        struct AudioSource *tmp = realloc(d->list, new_cap * sizeof(*d->list));
        if (!tmp) {
            ERROR("Failed to grow audio source list");
            d->failed = 1;
            return;
        }
        d->list = tmp;
        d->cap = new_cap;
    }

    struct AudioSource *dst = &d->list[d->count];
    snprintf(dst->name, sizeof(dst->name), "%s", info->name ? info->name : "");
    snprintf(dst->description, sizeof(dst->description), "%s", info->description ? info->description : "");
    d->count++;
}

static void on_context_state(pa_context *c, void *userdata) {
    int *state = (int *)userdata;
    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY:
        *state = 1;
        break;
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
        *state = -1;
        break;
    default:
        break;
    }
}

struct AudioSource *fetch_audio_sources(size_t *out_count) {
    if (out_count)
        *out_count = 0;

    pa_mainloop *m = pa_mainloop_new();
    if (!m) {
        ERROR("pa_mainloop_new failed");
        return NULL;
    }

    pa_context *ctx = pa_context_new(pa_mainloop_get_api(m), "tin-discover");
    if (!ctx) {
        ERROR("pa_context_new failed");
        pa_mainloop_free(m);
        return NULL;
    }

    int state = 0;
    pa_context_set_state_callback(ctx, on_context_state, &state);

    if (pa_context_connect(ctx, NULL, 0, NULL) < 0) {
        ERROR("pa_context_connect failed");
        pa_context_unref(ctx);
        pa_mainloop_free(m);
        return NULL;
    }

    while (state == 0) {
        if (pa_mainloop_iterate(m, 1, NULL) < 0) {
            ERROR("pa_mainloop_iterate failed while connecting");
            state = -1;
        }
    }

    struct SourceCollect data = {0};
    if (state > 0) {
        pa_operation *op = pa_context_get_source_info_list(ctx, on_source_info, &data);
        if (op) {
            while (pa_operation_get_state(op) == PA_OPERATION_RUNNING && state > 0) {
                if (pa_mainloop_iterate(m, 1, NULL) < 0) {
                    ERROR("pa_mainloop_iterate failed while listing sources");
                    break;
                }
            }
            if (pa_operation_get_state(op) != PA_OPERATION_DONE) {
                ERROR("Failed to list pulse audio sources");
                data.failed = 1;
            }
            pa_operation_unref(op);
        } else {
            ERROR("pa_context_get_source_info_list failed");
            data.failed = 1;
        }
    } else {
        ERROR("pa_context_connect failed: %s", pa_strerror(pa_context_errno(ctx)));
        data.failed = 1;
    }

    pa_context_disconnect(ctx);
    pa_context_unref(ctx);
    pa_mainloop_free(m);

    if (data.failed || data.count == 0) {
        free(data.list);
        return NULL;
    }

    if (out_count)
        *out_count = data.count;

    return data.list;
}

#endif // USE_PULSE
