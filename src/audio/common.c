#include "audio/common.h"
#include "protocols/audio.h"
#include "net/node.h"
#include "logging/log.h"
#include <signal.h>

#ifndef USE_PULSE
#error "No audio backend selected (define USE_PULSE)"
#endif

#define SILENCE_HANGOVER_FRAMES 10 // 200 ms

static void run_capture_loop(struct Node *node, AudioDevice *dev, volatile sig_atomic_t *running) {
    uint32_t seq = 1;
    int hangover = 0;

    while (running && *running) {
        int16_t buff[AUDIO_FRAME_SAMPLES];
        if (audio_receive(dev, buff, sizeof(buff)) < 0) {
            ERROR("Failed to capture an audio frame");
            return;
        }

        if (!audio_frame_is_silent(buff, AUDIO_FRAME_SAMPLES)) {
            hangover = SILENCE_HANGOVER_FRAMES;
        } else if (hangover > 0) {
            hangover--;
        } else {
            continue;
        }

        audio_packet packet;
        packet.version = AUDIO_PACKET_VERSION;
        packet.seq = seq++;
        memcpy(packet.data, buff, AUDIO_FRAME_BYTES);

        if (!audio_packet_send(node, node->peer_fd, &packet, running))
            return;

        // Frames are paced at 20 ms, so this is one line per second of speech.
        if (packet.seq % 50 == 0)
            INFO("Sent %u frames, %zu bytes each", packet.seq, (size_t)AUDIO_PACKET_SIZE);
    }
}

static void run_playback_loop(struct Node *node, AudioDevice *dev, volatile sig_atomic_t *running) {
    uint32_t expected = 0;
    uint32_t played = 0;

    while (running && *running) {
        audio_packet packet;
        if (!audio_packet_recv(node, node->peer_fd, &packet, running))
            return;

        if (packet.version != AUDIO_PACKET_VERSION) {
            WARN("Dropping audio packet with unsupported version: %u", packet.version);
            continue;
        }

        if (expected && packet.seq != expected)
            WARN("Sequence gap: expected %u, got %u", expected, packet.seq);
        expected = packet.seq + 1;

        if (audio_send(dev, packet.data, AUDIO_FRAME_BYTES) < 0) {
            ERROR("Failed to play an audio frame");
            return;
        }

        if (++played % 50 == 0)
            INFO("Played %u frames, %zu bytes each", played, (size_t)AUDIO_PACKET_SIZE);
    }
}

void *run_audio(void *arg, volatile sig_atomic_t *running) {
    struct AudioThread *audio = arg;

    if (audio->node->cap & AUDIO_CAPTURE)
        run_capture_loop(audio->node, audio->dev, running);
    else
        run_playback_loop(audio->node, audio->dev, running);

    return NULL;
}

int audio_frame_is_silent(const int16_t *samples, size_t count) {
    if (count == 0)
        return 1;

    double energy = 0.0;
    for (size_t i = 0; i < count; i++) {
        // INT16_MIN maps to exactly -1.0, so full scale is [-1, 1].
        double sample = (double)samples[i] / 32768.0;
        energy += sample * sample;
    }

    return energy / (double)count <= AUDIO_SILENCE_RMS * AUDIO_SILENCE_RMS;
}
