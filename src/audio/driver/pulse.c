#ifdef USE_PULSE

#include "pulse/simple.h"
#include "audio/common.h"
#include "logging/log.h"
#include <pulse/def.h>
#include <stdlib.h>

static const pa_sample_spec SPEC = {
    .format = PA_SAMPLE_S16LE,
    .rate = 48000,
    .channels = 1,
};

// Latency tuning. PulseAudio's defaults favour throughput over latency, which
// adds delay we cannot afford on a live call.
static const pa_buffer_attr PLAYBACK_ATTR = {
    .maxlength = (uint32_t)-1,
    .tlength = AUDIO_FRAME_BYTES * 4, // keep ~80 ms queued to absorb jitter
    .prebuf = AUDIO_FRAME_BYTES * 2,  // start playing once ~40 ms has piled up
    .minreq = AUDIO_FRAME_BYTES,      // refill one frame at a time
    .fragsize = (uint32_t)-1,
};

static const pa_buffer_attr CAPTURE_ATTR = {
    .maxlength = (uint32_t)-1,
    .fragsize = AUDIO_FRAME_BYTES, // hand us exactly one frame per read
    .tlength = (uint32_t)-1,
    .prebuf = (uint32_t)-1,
    .minreq = (uint32_t)-1,
};

struct AudioDevice {
    pa_simple *playback;
    pa_simple *capture;
};

AudioDevice *audio_create(void) {
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
        "alsa_output.usb-HP__Inc_HyperX_Cloud_II_Core_Wireless-00.analog-stereo.monitor", // TODO hell nah
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

#endif // USE_PULSE
