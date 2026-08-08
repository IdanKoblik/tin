#include "protocols/input.h"
#include <sys/socket.h>
#include <linux/input.h>
#include "net/node.h"
#include "protocols/packet.h"

_Static_assert(sizeof(((struct input_event *)0)->type) <= sizeof(((input_packet *)0)->type),
               "input packet cannot hold an evdev event type");
_Static_assert(sizeof(((struct input_event *)0)->code) <= sizeof(((input_packet *)0)->code),
               "input packet cannot hold an evdev event code");
_Static_assert(sizeof(((struct input_event *)0)->value) <= sizeof(((input_packet *)0)->value),
               "input packet cannot hold an evdev event value");

void encode_input_packet(const input_packet *p, uint8_t *out) {
    size_t offset = 0;

#define FIELD(type, name, dims) \
    PACK_ENCODE(out, offset, p->name);

    INPUT_PACKET_FIELDS

#undef FIELD
}

void decode_input_packet(const uint8_t *data, input_packet *out) {
    size_t offset = 0;

#define FIELD(type, name, dims) \
    PACK_DECODE(data, offset, out->name);

    INPUT_PACKET_FIELDS

#undef FIELD
}

int input_packet_send(struct Node *node, int fd, const input_packet *packet, volatile sig_atomic_t *running) {
    if (running && !*running)
        return 0;

    uint8_t raw_packet[INPUT_PACKET_SIZE];
    encode_input_packet(packet, raw_packet);

    ssize_t sent = node_send_packet(node, fd, raw_packet, INPUT_PACKET_SIZE, MSG_NOSIGNAL);
    if (running && !*running)
        return 0;

    return sent == (ssize_t)INPUT_PACKET_SIZE;
}

int input_packet_recv(struct Node *node, int fd, input_packet *out, volatile sig_atomic_t *running) {
    if (running && !*running)
        return 0;

    uint8_t raw_packet[INPUT_PACKET_SIZE];
    ssize_t received = node_recv_packet(node, fd, raw_packet, INPUT_PACKET_SIZE, 0);

    if (running && !*running)
        return 0;

    if (received != (ssize_t)INPUT_PACKET_SIZE)
        return 0;

    decode_input_packet(raw_packet, out);
    return 1;
}
