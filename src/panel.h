#ifndef PANEL_H
#define PANEL_H

#include "fs.h"
#include "theme.h"

#define PANEL_MAX_TABS 5

typedef struct {
    char path[4096];
    char display_name[64];
    int  scroll_offset;
    int  cursor;
} PanelTab;

typedef struct {
    PanelTab  tabs[PANEL_MAX_TABS];
    int       tab_count, active_tab;
    FileEntry *entries;
    int        entry_count;
    int       *tagged, tagged_count;
    int        needs_refresh, sort_by, sort_reverse, show_hidden, dirty;
    int        in_drive_list;
    char       drive_paths[26][4096];
    FileEntry  saved_entries[4096];
    int        saved_entry_count, saved_cursor;
    char       saved_path[4096];
} Panel;

void           panel_init(Panel *p, const char *start_dir);
void           panel_free(Panel *p);
void           panel_refresh(Panel *p, const FsProvider *fs);
void           panel_enter_dir(Panel *p, const FsProvider *fs);
void           panel_go_parent(Panel *p, const FsProvider *fs);
void           panel_go_drives(Panel *p, const FsProvider *fs);
void           panel_exit_drives(Panel *p, const FsProvider *fs);
void           panel_enter_on_drive(Panel *p, const FsProvider *fs);
void           panel_cursor_up(Panel *p);
void           panel_cursor_down(Panel *p);
void           panel_page_up(Panel *p, int page_h);
void           panel_page_down(Panel *p, int page_h);
void           panel_cursor_home(Panel *p);
void           panel_cursor_end(Panel *p);
void           panel_toggle_tag(Panel *p);
void           panel_clear_tags(Panel *p);
int            panel_tagged_or_current(const Panel *p, FileEntry **out, int *count);
int            panel_tab_new(Panel *p);
int            panel_tab_close(Panel *p);
int            panel_tab_next(Panel *p);
int            panel_tab_prev(Panel *p);
void           panel_tab_rename(Panel *p);
void           panel_render(const Panel *p, const Theme *theme, int x, int y, int w, int h, int focused);
const char    *panel_current_path(const Panel *p);
int            panel_list_height(const Panel *p);
int            panel_is_tagged(const Panel *p, int idx);

#endif
