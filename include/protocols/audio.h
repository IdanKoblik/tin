#pragma once

#include <signal.h>
#include <stdint.h>

#define AUDIO_PACKET_VERSION 1
#define AUDIO_SIZE 1920 // 20 ms of 48 kHz, 16-bit mono PCM audio

#define AUDIO_PACKET_FIELDS                                              \
    FIELD(uint8_t, version, )                                            \
    FIELD(uint32_t, seq, )                                               \
    FIELD(uint8_t, data, [AUDIO_SIZE])

typedef struct __attribute__((packed)) {
#define FIELD(type, name, dims) type name dims;
    AUDIO_PACKET_FIELDS
#undef FIELD
} audio_packet;

#define AUDIO_PACKET_SIZE sizeof(audio_packet)

_Static_assert(
#define FIELD(type, name, dims) sizeof(((audio_packet *)0)->name) +
    AUDIO_PACKET_FIELDS
#undef FIELD
        0 == AUDIO_PACKET_SIZE,
    "audio packet fields do not cover the whole struct");

void encode_audio_packet(const audio_packet *p, uint8_t *out);
void decode_audio_packet(const uint8_t *data, audio_packet *out);

struct Node;

int audio_packet_send(struct Node *node, int fd, const audio_packet *packet, volatile sig_atomic_t *running);
int audio_packet_recv(struct Node *node, int fd, audio_packet *out, volatile sig_atomic_t *running);
