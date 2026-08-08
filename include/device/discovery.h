#pragma once

#include <stdint.h>
#include <stdio.h>

enum device_type {
    MOUSE,
    KEYBOARD
};

struct device_entry {
    char name[256];
    char devnode[256];
};

struct device_entry *fetch_devices(enum device_type type, size_t *out_count);
int open_selected_device(enum device_type type, const char *label);
