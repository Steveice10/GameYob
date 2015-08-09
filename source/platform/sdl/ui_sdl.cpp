#ifdef BACKEND_SDL

#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>

#include "platform/ui.h"

#include <ncurses.h>

WINDOW* window;

TextColor textColor = TEXT_COLOR_NONE;
bool highlighted = false;

void uiInit() {
    initscr();
    start_color();

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    window = newwin(w.ws_row, w.ws_col, 0, 0);

    noecho();
    cbreak();
    nodelay(window, TRUE);
    keypad(window, TRUE);
    scrollok(window, TRUE);

    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(5, COLOR_WHITE, COLOR_BLACK);
    init_pair(6, COLOR_WHITE, COLOR_BLACK);

    init_pair(7, COLOR_YELLOW, COLOR_WHITE);
    init_pair(8, COLOR_RED, COLOR_WHITE);
    init_pair(9, COLOR_GREEN, COLOR_WHITE);
    init_pair(10, COLOR_MAGENTA, COLOR_WHITE);
    init_pair(11, COLOR_BLACK, COLOR_WHITE);
    init_pair(12, COLOR_BLACK, COLOR_WHITE);

    refresh();
}

void uiCleanup() {
    delwin(window);
    endwin();
}

void uiUpdateScreen() {
}

int uiGetWidth() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    return w.ws_col;
}

int uiGetHeight() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    return w.ws_row;
}

void uiClear() {
    wclear(window);
}

void uiClearAttr() {
    if(textColor != TEXT_COLOR_NONE) {
        if(textColor == TEXT_COLOR_GRAY) {
            wattroff(window, A_DIM);
        }

        wattroff(window, COLOR_PAIR((highlighted ? 6 : 0) + textColor));
    } else if(highlighted) {
        wattroff(window, COLOR_PAIR(12));
    }
}

void uiUpdateAttr() {
    if(textColor != TEXT_COLOR_NONE) {
        if(textColor == TEXT_COLOR_GRAY) {
            wattron(window, A_DIM);
        }

        wattron(window, COLOR_PAIR((highlighted ? 6 : 0) + textColor));
    } else if(highlighted) {
        wattron(window, COLOR_PAIR(12));
    }
}

void uiSetLineHighlighted(bool highlight) {
    uiClearAttr();
    highlighted = highlight;
    uiUpdateAttr();
}

void uiSetTextColor(TextColor color) {
    uiClearAttr();
    textColor = color;
    uiUpdateAttr();
}

void uiPrint(const char* str, ...) {
    va_list list;
    va_start(list, str);
    uiPrintv(str, list);
    va_end(list);
}

void uiPrintv(const char* str, va_list list) {
    vw_printw(window, str, list);
}

void uiFlush() {
    wrefresh(window);
}

void uiWaitForVBlank() {
}

UIKey uiReadKey() {
    switch(wgetch(window)) {
        case '\n':
            return UI_KEY_A;
        case KEY_BACKSPACE:
            return UI_KEY_B;
        case KEY_UP:
            return UI_KEY_UP;
        case KEY_DOWN:
            return UI_KEY_DOWN;
        case KEY_LEFT:
            return UI_KEY_LEFT;
        case KEY_RIGHT:
            return UI_KEY_RIGHT;
        case KEY_PPAGE:
            return UI_KEY_L;
        case KEY_NPAGE:
            return UI_KEY_R;
        case '=':
            return UI_KEY_X;
        case '-':
            return UI_KEY_Y;
        default:
            return UI_KEY_NONE;
    }
}

#endif