#ifndef CMDLINE_H
#define CMDLINE_H

#include "theme.h"

#define CMDLINE_MAX_LEN 4096
#define CMDLINE_HISTORY_MAX 100

typedef struct {
    char   buffer[CMDLINE_MAX_LEN];
    int    cursor, length;
    char  *history[CMDLINE_HISTORY_MAX];
    int    history_count, history_pos;
    char  *last_output;
    int    show_output, output_scroll;
} CmdLine;

void cmdline_init(CmdLine *c);
void cmdline_free(CmdLine *c);
void cmdline_insert(CmdLine *c, char ch);
void cmdline_backspace(CmdLine *c);
void cmdline_delete(CmdLine *c);
void cmdline_cursor_left(CmdLine *c);
void cmdline_cursor_right(CmdLine *c);
void cmdline_cursor_home(CmdLine *c);
void cmdline_cursor_end(CmdLine *c);
void cmdline_history_prev(CmdLine *c);
void cmdline_history_next(CmdLine *c);
int  cmdline_execute(CmdLine *c);
void cmdline_clear(CmdLine *c);
void cmdline_render(const CmdLine *c, const Theme *theme, int x, int y, int w, int focused);

#endif
