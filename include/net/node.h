#pragma once

#include "net/role.h"
#include "crypto/ed25519.h"
#include "capability.h"
#include <sodium.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>

#define NODE_PACKET_NONCE_BYTES crypto_aead_chacha20poly1305_ietf_NPUBBYTES
#define NODE_PACKET_OVERHEAD (NODE_PACKET_NONCE_BYTES + crypto_aead_chacha20poly1305_ietf_ABYTES)

struct Node {
    struct sockaddr_in server_addr;
    struct sockaddr_in peer;

    int sock_fd;
    int peer_fd;

    const char *server_ip;
    uint16_t server_port;

    enum Role role;
    enum Capability cap;

    unsigned char long_term_private_key[crypto_sign_SECRETKEYBYTES];
    unsigned char long_term_public_key[crypto_sign_PUBLICKEYBYTES];

    unsigned char kx_private_key[crypto_kx_SECRETKEYBYTES];
    unsigned char kx_public_key[crypto_kx_PUBLICKEYBYTES];

    unsigned char session_rx[crypto_kx_SESSIONKEYBYTES];
    unsigned char session_tx[crypto_kx_SESSIONKEYBYTES];

    pthread_mutex_t send_lock;

    volatile sig_atomic_t *running;
};

int node_init(struct Node *node, const char *addr, uint16_t port);
void node_set_running(struct Node *node, volatile sig_atomic_t *running);
int node_listen(struct Node *node);
int node_connect(struct Node *node);

int node_handshake(struct Node *node, volatile sig_atomic_t *running);

ssize_t node_send_packet(struct Node *node, int fd, const uint8_t *buf, size_t n, int flags);
ssize_t node_recv_packet(struct Node *node, int fd, uint8_t *buf, size_t n, int flags);

void node_shutdown_peer(struct Node *node);

void node_close_peer(struct Node *node);
void node_cleanup(struct Node *node);
