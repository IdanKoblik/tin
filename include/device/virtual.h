#pragma once

#include <signal.h>
#include "display/common.h"
#include "net/node.h"

int create_virtual_device(void);
void destroy_virtual_device(int fd);

void emit_event(int fd, int type, int code, int val);

int handle_input(struct Node *node, int mouse_fd, int keyboard_fd, const struct Display *display, enum Edge edge, volatile sig_atomic_t *running);
