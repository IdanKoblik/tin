#pragma once

#include <signal.h>
#include <stdint.h>

#define INPUT_PACKET_VERSION 1

#define INPUT_PACKET_FIELDS                                              \
    FIELD(uint8_t, version, )                                            \
    FIELD(uint32_t, seq, )                                               \
    FIELD(uint16_t, type, )                                              \
    FIELD(uint16_t, code, )                                              \
    FIELD(int32_t, value, )

typedef struct __attribute__((packed)) {
#define FIELD(type, name, dims) type name dims;
    INPUT_PACKET_FIELDS
#undef FIELD
} input_packet;

#define INPUT_PACKET_SIZE sizeof(input_packet)

_Static_assert(
#define FIELD(type, name, dims) sizeof(((input_packet *)0)->name) +
    INPUT_PACKET_FIELDS
#undef FIELD
        0 == INPUT_PACKET_SIZE,
    "input packet fields do not cover the whole struct");

void encode_input_packet(const input_packet *p, uint8_t *out);
void decode_input_packet(const uint8_t *data, input_packet *out);

struct Node;

int input_packet_send(struct Node *node, int fd, const input_packet *packet, volatile sig_atomic_t *running);
int input_packet_recv(struct Node *node, int fd, input_packet *out, volatile sig_atomic_t *running);
