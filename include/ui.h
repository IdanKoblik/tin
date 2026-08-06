#pragma once

#include <ncurses.h>
#include <stdbool.h>
#include <stddef.h>

int select_menu(const char *title, const char *const *options, size_t count);

int multi_select_menu(
    const char *title,
    const char *const *options,
    size_t count,
    bool *selected
);
