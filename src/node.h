#pragma once

#include "role.h"
#include <netinet/in.h>

struct Node {
    struct sockaddr_in server_addr;
    struct sockaddr_in peer;
 
    int sock_fd; 
    int peer_fd;

    const char *server_ip;
    uint16_t server_port; 

    enum Role role;
};

int node_init(struct Node *node, const char *addr, uint16_t port);
int node_listen(struct Node *node);
int node_connect(struct Node *nodet);

void node_cleanup(struct Node *node);
