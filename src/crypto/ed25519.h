#pragma once

#include <sodium.h>

struct Node;

int key_pair_load(const char *path, struct Node *node);
