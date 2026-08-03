#include "node.h"
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../logging/log.h"
#include "role.h"

int node_init(struct Node *node, const char *addr, uint16_t port) {
    if (!node) {
        ERROR("Node cannot be null!");
        return 0;
    }

    memset(node, 0, sizeof(*node));
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

    return 1;
}

int node_connect(struct Node *node) {
    INFO("Connecting to: %s:%d\n", node->server_ip, node->server_port);

    if (!node) {
        ERROR("Node cannot be null!");
        return 0;
    }

    memset(&node->server_addr, 0, sizeof(node->server_addr));
    memset(&node->peer, 0, sizeof(node->peer));
    
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

    return 1; 
}

void node_cleanup(struct Node *node) {
    if (!node) {
        ERROR("Failed to cleanup node. NODE CANNOT BE NULL!!!");
        return;
    }

    if (node->sock_fd == -1) {
        DEBUG("Cannot close node socket");
        close(node->sock_fd);
    }

    if (node->peer_fd == -1 && node->role == SERVER) {
        DEBUG("Cannot close peer socket");
        close(node->peer_fd);
    }
}
