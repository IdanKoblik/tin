#include "protocols/audio.h"
#include <sys/socket.h>
#include "audio/common.h"
#include "net/node.h"
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

int audio_packet_send(struct Node *node, int fd, const audio_packet *packet, volatile sig_atomic_t *running) {
    if (running && !*running)
        return 0;

    uint8_t raw_packet[AUDIO_PACKET_SIZE];
    encode_audio_packet(packet, raw_packet);

    ssize_t sent = node_send_packet(node, fd, raw_packet, AUDIO_PACKET_SIZE, MSG_NOSIGNAL);
    if (running && !*running)
        return 0;

    return sent == (ssize_t)AUDIO_PACKET_SIZE;
}

int audio_packet_recv(struct Node *node, int fd, audio_packet *out, volatile sig_atomic_t *running) {
    if (running && !*running)
        return 0;

    uint8_t raw_packet[AUDIO_PACKET_SIZE];
    ssize_t received = node_recv_packet(node, fd, raw_packet, AUDIO_PACKET_SIZE, 0);

    // On our own shutdown the socket dies under us on purpose; that is not the
    // peer leaving, so do not treat it as a receive failure.
    if (running && !*running)
        return 0;

    if (received != (ssize_t)AUDIO_PACKET_SIZE)
        return 0;

    decode_audio_packet(raw_packet, out);
    return 1;
}
