#pragma once

int create_virtual_device(void);

void emit_event(int fd, int type, int code, int val);
