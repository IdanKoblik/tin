#pragma once

#include "net/role.h"
#include "crypto/ed25519.h"
#include <sodium.h>
#include <netinet/in.h>
#include <signal.h>

struct Node {
    struct sockaddr_in server_addr;
    struct sockaddr_in peer;

    int sock_fd;
    int peer_fd;

    const char *server_ip;
    uint16_t server_port;

    enum Role role;

    unsigned char long_term_private_key[crypto_sign_SECRETKEYBYTES];
    unsigned char long_term_public_key[crypto_sign_PUBLICKEYBYTES];

    unsigned char kx_private_key[crypto_kx_SECRETKEYBYTES];
    unsigned char kx_public_key[crypto_kx_PUBLICKEYBYTES];

    unsigned char session_rx[crypto_kx_SESSIONKEYBYTES];
    unsigned char session_tx[crypto_kx_SESSIONKEYBYTES];
};

int node_init(struct Node *node, const char *addr, uint16_t port);
int node_listen(struct Node *node);
int node_connect(struct Node *node);

int node_handshake(struct Node *node, volatile sig_atomic_t *running);

void node_close_peer(struct Node *node);

void node_cleanup(struct Node *node);
