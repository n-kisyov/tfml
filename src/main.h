#ifndef MAIN_H
#define MAIN_H

#include "panel.h"
#include "cmdline.h"
#include "theme.h"
#include "config.h"
#include "input.h"
#include "fs.h"
#include "bgop.h"

typedef enum { FOCUS_LEFT, FOCUS_RIGHT, FOCUS_CMDLINE } FocusTarget;

typedef struct {
    Panel        panels[2];
    CmdLine      cmdline;
    FocusTarget  focus;
    Theme        theme;
    Config       config;
    int          running, tw, th, left_w, needs_redraw, show_help;
    const FsProvider *fs;
    BgTask       bgtask;
    BgOpRecord   history[BGOP_HISTORY];
    int          history_count, history_visible;
    int          panel_locked[2];
} AppState;

extern AppState g_app;

#endif
