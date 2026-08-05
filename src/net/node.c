#include "node.h"
#include <sodium/utils.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sodium.h>
#include "../logging/log.h"
#include "client.h"
#include "role.h"
#include "server.h"

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

    if ((listen(node->sock_fd, 1)) != 0) {
        ERROR("Failed to listen to the socket");
        return 0;
    }

    socklen_t peer_len = sizeof(node->peer);
    node->peer_fd = accept(node->sock_fd, (struct sockaddr *)&node->peer, &peer_len);
    if (node->peer_fd == -1) {
        ERROR("Failed to accept a peer connection");
        return 0;
    }

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
