#include "protocols/audio.h"
#include <errno.h>
#include <sys/socket.h>
#include "audio/common.h"
#include "logging/log.h"
#include "protocols/packet.h"

_Static_assert(AUDIO_SIZE == AUDIO_FRAME_BYTES, "audio packet payload must hold exactly one audio frame");

void encode_audio_packet(const audio_packet *p, uint8_t *out) {
    size_t offset = 0;

#define FIELD(type, name, dims) \
    PACK_ENCODE(out, offset, p->name);

    AUDIO_PACKET_FIELDS

#undef FIELD
}

void decode_audio_packet(const uint8_t *data, audio_packet *out) {
    size_t offset = 0;

#define FIELD(type, name, dims) \
    PACK_DECODE(data, offset, out->name);

    AUDIO_PACKET_FIELDS

#undef FIELD
}

int audio_packet_send(int fd, const audio_packet *packet, volatile sig_atomic_t *running) {
    uint8_t raw_packet[AUDIO_PACKET_SIZE];
    encode_audio_packet(packet, raw_packet);

    size_t sent = 0;
    while (sent < AUDIO_PACKET_SIZE) {
        if (running && !*running)
            return 0;

        // MSG_NOSIGNAL so a peer that hangs up mid-call hands us EPIPE instead
        // of killing the process with SIGPIPE.
        ssize_t n = send(fd, raw_packet + sent, AUDIO_PACKET_SIZE - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;

            // Quitting shuts the socket down under us, so EPIPE here is our own
            // doing rather than a peer that vanished.
            if (running && !*running)
                return 0;

            ERROR("Cannot send audio packet to peer");
            return 0;
        }

        sent += (size_t)n;
    }

    return 1;
}

int audio_packet_recv(int fd, audio_packet *out, volatile sig_atomic_t *running) {
    uint8_t raw_packet[AUDIO_PACKET_SIZE];
    size_t received = 0;

    while (received < AUDIO_PACKET_SIZE) {
        if (running && !*running)
            return 0;

        ssize_t n = recv(fd, raw_packet + received, AUDIO_PACKET_SIZE - received, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            ERROR("Cannot receive audio packet from peer");
            return 0;
        }

        if (n == 0) {
            // On our own shutdown the socket dies under us on purpose; that is
            // not the peer leaving, so do not report it as such.
            if (running && !*running)
                return 0;

            INFO("Peer closed the connection");
            return 0;
        }

        received += (size_t)n;
    }

    decode_audio_packet(raw_packet, out);
    return 1;
}
