#include "ui.h"

int select_menu(const char *title, const char *const *options, size_t count) {
    if (!options || count == 0)
        return -1;

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    size_t cursor = 0;
    int selected = -1;

    while (1) {
        erase();

        mvprintw(
            0,
            0,
            "%s (j/k or arrows, enter to confirm, q to cancel)",
            title ? title : "select an option"
        );

        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        (void)cols;

        size_t visible = (size_t)(rows - 2);
        if (visible == 0)
            visible = 1;

        size_t top = cursor < visible ? 0 : cursor - visible + 1;
        size_t end = top + visible < count ? top + visible : count;

        for (size_t i = top; i < end; i++) {
            int row = (int)(i - top) + 2;

            if (i == cursor)
                attron(A_REVERSE);

            mvprintw(row, 0, "[%zu] %s", i, options[i]);

            if (i == cursor)
                attroff(A_REVERSE);
        }

        refresh();

        int ch = getch();

        switch (ch) {
            case KEY_UP:
            case 'k':
                if (cursor > 0)
                    cursor--;
                break;

            case KEY_DOWN:
            case 'j':
                if (cursor + 1 < count)
                    cursor++;
                break;

            case '\n':
            case KEY_ENTER:
                selected = (int)cursor;
                goto done;

            case 'q':
            case 27:
                goto done;

            default:
                break;
        }
    }

done:
    curs_set(1);
    keypad(stdscr, FALSE);
    echo();
    nocbreak();
    endwin();

    return selected;
}

int multi_select_menu(
    const char *title,
    const char *const *options,
    size_t count,
    bool *selected
) {
    if (!options || !selected || count == 0)
        return -1;

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    size_t cursor = 0;
    int result = -1;

    while (1) {
        erase();

        mvprintw(
            0,
            0,
            "%s (j/k move, space toggle, a all, n none, enter confirm, q cancel)",
            title ? title : "select options"
        );

        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        (void)cols;

        size_t visible = (size_t)(rows - 2);
        if (visible == 0)
            visible = 1;

        size_t top = cursor < visible ? 0 : cursor - visible + 1;
        size_t end = top + visible < count ? top + visible : count;

        for (size_t i = top; i < end; i++) {
            int row = (int)(i - top) + 2;

            if (i == cursor)
                attron(A_REVERSE);

            mvprintw(row, 0, "[%c] %s", selected[i] ? 'x' : ' ', options[i]);

            if (i == cursor)
                attroff(A_REVERSE);
        }

        refresh();

        int ch = getch();

        switch (ch) {
            case KEY_UP:
            case 'k':
                if (cursor > 0)
                    cursor--;
                break;

            case KEY_DOWN:
            case 'j':
                if (cursor + 1 < count)
                    cursor++;
                break;

            case ' ':
                selected[cursor] = !selected[cursor];
                break;

            case 'a':
                for (size_t i = 0; i < count; i++)
                    selected[i] = true;
                break;

            case 'n':
                for (size_t i = 0; i < count; i++)
                    selected[i] = false;
                break;

            case '\n':
            case KEY_ENTER: {
                int n = 0;
                for (size_t i = 0; i < count; i++)
                    if (selected[i])
                        n++;
                result = n;
                goto multi_done;
            }

            case 'q':
            case 27:
                goto multi_done;

            default:
                break;
        }
    }

multi_done:
    curs_set(1);
    keypad(stdscr, FALSE);
    echo();
    nocbreak();
    endwin();

    return result;
}
