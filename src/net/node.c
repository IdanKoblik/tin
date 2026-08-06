#include "net/node.h"
#include <errno.h>
#include <sodium/crypto_aead_chacha20poly1305.h>
#include <sodium/utils.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sodium.h>
#include "crypto/buffer.h"
#include "logging/log.h"
#include "net/client.h"
#include "net/role.h"
#include "net/server.h"

// Audio frames are small and paced at 20 ms, so Nagle's algorithm would hold
// each one back waiting for the previous frame's ACK.
static void disable_nagle(int fd) {
    int on = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) != 0)
        WARN("Failed to disable Nagle on peer socket; audio latency may suffer");
}

int node_init(struct Node *node, const char *addr, uint16_t port) {
    if (!node) {
        ERROR("Node cannot be null!");
        return 0;
    }

    sodium_memzero(node, sizeof(*node));
    node->peer_fd = -1;

    node->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (node->sock_fd == -1) {
        ERROR("Failed to create tcp socket");
        return 0;
    }

    node->server_addr.sin_family = AF_INET;
    node->server_addr.sin_port = htons(port);
    if (!strcmp(addr, "0.0.0.0"))
        node->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (inet_pton(AF_INET, addr, &(node->server_addr.sin_addr.s_addr)) < 0) {
        ERROR("failed to parse addr");
        return 0;
    }

    node->server_ip = addr;
    node->server_port = port;
    return 1;
}

int node_listen(struct Node *node) {
    INFO("Binding socket to: %s:%d\n", node->server_ip, node->server_port);

    if (!node) {
        ERROR("Node cannot be null!");
        return 0;
    }

    if ((bind(node->sock_fd, (struct sockaddr *)&node->server_addr, sizeof(node->server_addr))) != 0) {
        ERROR("Failed to bind socket");
        return 0;
    }

    if (listen(node->sock_fd, 1) != 0) {
        ERROR("Failed to listen to the socket");
        return 0;
    }

    if (node->peer_fd != -1) {
        WARN("Someone tried to also connect");
        return 0;
    }

    socklen_t peer_len = sizeof(node->peer);
    node->peer_fd = accept(node->sock_fd, (struct sockaddr *)&node->peer, &peer_len);
    if (node->peer_fd == -1) {
        ERROR("Failed to accept a peer connection");
        return 0;
    }

    disable_nagle(node->peer_fd);

    INFO("Peer connected from: %s:%d", inet_ntoa(node->peer.sin_addr), ntohs(node->peer.sin_port));
    return 1;
}

int node_connect(struct Node *node) {
    INFO("Connecting to: %s:%d\n", node->server_ip, node->server_port);

    if (!node) {
        ERROR("Node cannot be null!");
        return 0;
    }

    sodium_memzero(&node->server_addr, sizeof(node->server_addr));
    sodium_memzero(&node->peer, sizeof(node->peer));

    if (node->sock_fd != -1)
        close(node->sock_fd);

    node->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (node->sock_fd == -1) {
        ERROR("Failed to create tcp socket");
        return 0;
    }

    node->server_addr.sin_family = AF_INET;
    node->server_addr.sin_port = htons(node->server_port);
    if (!strcmp(node->server_ip, "0.0.0.0"))
        node->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (inet_pton(AF_INET, node->server_ip, &(node->server_addr.sin_addr.s_addr)) < 0) {
        ERROR("failed to parse addr");
        return 0;
    }

    if (connect(node->sock_fd, (struct sockaddr *)&node->server_addr, sizeof(node->server_addr)) != 0) {
        ERROR("Failed to connect to the host");
        return 0;
    }

    node->peer = node->server_addr;
    node->peer_fd = node->sock_fd;

    disable_nagle(node->peer_fd);

    INFO("Connected to peer: %s:%d", inet_ntoa(node->peer.sin_addr), ntohs(node->peer.sin_port));
    return 1;
}

int node_handshake(struct Node *node, volatile sig_atomic_t *running) {
    switch (node->role) {
        case SERVER: return server_handshake(node, running);
        case CLIENT: return client_handshake(node, running);
        default: return 0;
    }

    return 1;
}

static int send_all(int fd, const uint8_t *buf, size_t n, int flags) {
    size_t sent = 0;

    while (sent < n) {
        ssize_t written = send(fd, buf + sent, n - sent, flags);
        if (written < 0) {
            if (errno == EINTR)
                continue;

            if (errno == EPIPE)
                return 0;

            return -1;
        }

        sent += (size_t)written;
    }

    return 1;
}

static int recv_all(int fd, uint8_t *buf, size_t n, int flags) {
    size_t received = 0;

    while (received < n) {
        ssize_t got = recv(fd, buf + received, n - received, flags);
        if (got < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (got == 0)
            return 0;

        received += (size_t)got;
    }

    return 1;
}

ssize_t node_send_packet(struct Node *node, int fd, const uint8_t *buf, size_t n, int flags) {
    if (!node || !buf || n == 0) {
        ERROR("Cannot send a packet from null or empty arguments");
        return -1;
    }

    uint8_t packet[NODE_PACKET_OVERHEAD + n];
    uint8_t *nonce = packet;
    uint8_t *ciphertext = packet + NODE_PACKET_NONCE_BYTES;

    randombytes_buf(nonce, NODE_PACKET_NONCE_BYTES);

    unsigned long long cipher_len = 0;
    if (encrypt_buffer(ciphertext, &cipher_len, buf, n, nonce, node->session_tx) != 0) {
        ERROR("Failed to encrypt buffer");
        return -1;
    }

    int rc = send_all(fd, packet, NODE_PACKET_NONCE_BYTES + (size_t)cipher_len, flags);
    if (rc < 0) {
        ERROR("Cannot send packet to peer");
        return -1;
    }

    if (rc == 0) {
        INFO("Peer closed the connection");
        return 0;
    }

    return (ssize_t)n;
}

ssize_t node_recv_packet(struct Node *node, int fd, uint8_t *buf, size_t n, int flags) {
    if (!node || !buf || n == 0) {
        ERROR("Cannot receive a packet into null or empty arguments");
        return -1;
    }

    uint8_t packet[NODE_PACKET_OVERHEAD + n];
    int rc = recv_all(fd, packet, sizeof(packet), flags);
    if (rc < 0) {
        ERROR("Cannot receive packet from peer");
        return -1;
    }

    if (rc == 0) {
        INFO("Peer closed the connection");
        return 0;
    }

    unsigned long long plaintext_len = 0;
    if (decrypt_buffer(buf, &plaintext_len, packet + NODE_PACKET_NONCE_BYTES, sizeof(packet) - NODE_PACKET_NONCE_BYTES, packet, node->session_rx) != 0) {
        ERROR("Failed to decrypt packet from peer");
        return -1;
    }

    return (ssize_t)plaintext_len;
}

void node_close_peer(struct Node *node) {
    if (!node || node->peer_fd == -1)
        return;

    DEBUG("Closing peer connection");
    shutdown(node->peer_fd, SHUT_RDWR);
    close(node->peer_fd);

    if (node->sock_fd == node->peer_fd)
        node->sock_fd = -1;

    node->peer_fd = -1;
}

void node_cleanup(struct Node *node) {
    if (!node) {
        ERROR("Failed to cleanup node. NODE CANNOT BE NULL!!!");
        return;
    }

    node_close_peer(node);

    if (node->sock_fd != -1) {
        DEBUG("Closing node socket");
        close(node->sock_fd);
        node->sock_fd = -1;
    }
}
