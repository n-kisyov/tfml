#ifndef CONFIG_H
#define CONFIG_H

#include "utils.h"

#define CONFIG_MAX_PATH 4096

typedef struct {
    char     theme_path[CONFIG_MAX_PATH];
    char     startup_dirs[2][MAX_TABS][CONFIG_MAX_PATH];
    int      startup_tab_counts[2];
    char     drive_paths[2][26][CONFIG_MAX_PATH];
    int      show_hidden;
    int      sort_by;
    int      sort_reverse;
    int      confirm_delete;
    int      panel_split_pct;
} Config;

void            config_set_defaults(Config *c);
int             config_load(Config *c, const char *path);
int             config_save(const Config *c, const char *path);
const char     *config_get_path(void);

#endif
