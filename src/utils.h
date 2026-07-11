#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <sys/stat.h>

#define MAX_PATH_LEN 4096
#define MAX_CMDLINE_LEN 4096
#define MAX_HISTORY 100
#define MAX_TABS 5
#define MAX_ENTRIES_PER_DIR 4096
#define MAX_ENTRY_STRING 256

typedef struct timespec FileTime;

char  *strdup_safe(const char *src);
void  *xcalloc(size_t n, size_t sz);
void  *xrealloc(void *p, size_t sz);
int    str_ends_with(const char *str, const char *suffix);
char  *path_join(const char *dir, const char *name);
char  *get_parent_path(const char *path);
int    is_root_path(const char *path);
const char *get_file_ext(const char *name);
int    str_compare_natural(const char *a, const char *b);
char  *get_home_dir(void);
char  *get_config_dir(void);
char  *format_file_size(uint64_t bytes, char *buf, size_t buf_size);
char  *format_file_time(const FileTime *ft, char *buf, size_t buf_size);

#endif
