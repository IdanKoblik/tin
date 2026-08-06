#pragma once

#include <stdint.h>
#include <stdio.h>

enum devices {
    MOUSE = 1 << 0,
    KEYBOARD = 1 << 1
};

struct device_entry {
    char name[256];
    char devnode[256];
};

struct device_entry *fetch_devices(enum devices types, size_t *out_count);
