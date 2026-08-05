#pragma once

#include "../net/node.h"
#include "../crypto/ed25519.h"
#include <signal.h>
#include <sodium.h>

#define HANDSHAKE_PACKET_VERSION 1

#define HANDSHAKE_PACKET_FIELDS                                          \
    FIELD(uint8_t, version, )                                            \
    FIELD(unsigned char, public_key, [crypto_sign_PUBLICKEYBYTES])       \
    FIELD(unsigned char, kx_public_key, [crypto_kx_PUBLICKEYBYTES])      \
    FIELD(unsigned char, signature, [crypto_sign_BYTES])

typedef struct __attribute__((packed)) {
#define FIELD(type, name, dims) type name dims;
    HANDSHAKE_PACKET_FIELDS
#undef FIELD
} handshake_packet;

#define HANDSHAKE_PACKET_SIZE sizeof(handshake_packet)

_Static_assert(
#define FIELD(type, name, dims) sizeof(((handshake_packet *)0)->name) +
    HANDSHAKE_PACKET_FIELDS
#undef FIELD
        0 == HANDSHAKE_PACKET_SIZE,
    "handshake packet fields do not cover the whole struct");

void encode_handshake_packet(const handshake_packet *p, uint8_t *out);
void decode_handshake_packet(const uint8_t *data, handshake_packet *out);

int handshake_prepare(handshake_packet *out, const unsigned char *session_public_key, const struct Node *node);
int handshake_send(int fd, const handshake_packet *packet);
int handshake_recv(int fd, handshake_packet *out, volatile sig_atomic_t *running);
int handshake_verify(const handshake_packet *packet);
