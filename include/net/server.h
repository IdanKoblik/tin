#pragma once

#include <signal.h>
#include "net/node.h"

int server_handshake(struct Node *node, volatile sig_atomic_t *running);
