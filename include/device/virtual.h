#pragma once

#include <signal.h>
#include "net/node.h"

int create_virtual_device(void);
void destroy_virtual_device(int fd);

void emit_event(int fd, int type, int code, int val);

// Blocks until the link drops or *running clears. An INPUT_SEND node reads the
// two evdev fds, one thread each, and forwards what they produce; an
// INPUT_RECEIVE node ignores them and replays the peer's events into a uinput
// device instead.
int handle_input(struct Node *node, int mouse_fd, int keyboard_fd, volatile sig_atomic_t *running);
