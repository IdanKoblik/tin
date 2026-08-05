#include "handshake.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include "packet.h"
#include "../logging/log.h"

void encode_handshake_packet(const handshake_packet *p, uint8_t *out) {
    size_t offset = 0;

#define FIELD(type, name, dims) \
    PACK_ENCODE(out, offset, p->name);

    HANDSHAKE_PACKET_FIELDS

#undef FIELD
}

void decode_handshake_packet(const uint8_t *data, handshake_packet *out) {
    size_t offset = 0;

#define FIELD(type, name, dims) \
    PACK_DECODE(data, offset, out->name);

    HANDSHAKE_PACKET_FIELDS

#undef FIELD
}

int handshake_prepare(handshake_packet *out, const unsigned char *session_public_key, const struct Node *node) {
    if (!out || !session_public_key || !node) {
        ERROR("Cannot build a handshake packet from null arguments");
        return 0;
    }

    out->version = HANDSHAKE_PACKET_VERSION;
    memcpy(out->public_key, node->long_term_public_key, sizeof(out->public_key));
    memcpy(out->kx_public_key, session_public_key, sizeof(out->kx_public_key));

    if (crypto_sign_detached(out->signature, NULL, out->kx_public_key, crypto_kx_PUBLICKEYBYTES, node->long_term_private_key) != 0) {
        ERROR("Cannot sign session public key");
        return 0;
    }

    return 1;
}

int handshake_send(int fd, const handshake_packet *packet) {
    uint8_t raw_packet[HANDSHAKE_PACKET_SIZE];
    encode_handshake_packet(packet, raw_packet);

    size_t sent = 0;
    while (sent < HANDSHAKE_PACKET_SIZE) {
        ssize_t n = send(fd, raw_packet + sent, HANDSHAKE_PACKET_SIZE - sent, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            ERROR("Cannot send handshake packet to peer");
            return 0;
        }

        sent += (size_t)n;
    }

    return 1;
}

int handshake_recv(int fd, handshake_packet *out, volatile sig_atomic_t *running) {
    uint8_t raw_packet[HANDSHAKE_PACKET_SIZE];
    size_t received = 0;

    while (received < HANDSHAKE_PACKET_SIZE) {
        if (running && !*running) {
            WARN("Handshake aborted while waiting for peer");
            return 0;
        }

        ssize_t n = recv(fd, raw_packet + received, HANDSHAKE_PACKET_SIZE - received, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            ERROR("Cannot receive handshake packet from peer");
            return 0;
        }

        if (n == 0) {
            ERROR("Peer closed the connection during handshake");
            return 0;
        }

        received += (size_t)n;
    }

    decode_handshake_packet(raw_packet, out);
    return 1;
}

int handshake_verify(const handshake_packet *packet) {
    if (packet->version != HANDSHAKE_PACKET_VERSION) {
        ERROR("Unsupported handshake version: %u", packet->version);
        return 0;
    }

    if (crypto_sign_verify_detached(packet->signature, packet->kx_public_key, crypto_kx_PUBLICKEYBYTES, packet->public_key) != 0) {
        ERROR("Cannot verify peer public key");
        return 0;
    }

    return 1;
}
