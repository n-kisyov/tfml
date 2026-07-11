#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <sys/stat.h>

typedef enum { ENTRY_FILE, ENTRY_DIR, ENTRY_SYMLINK } EntryType;

typedef struct {
    char        name[256];
    EntryType   type;
    uint64_t    size;
    struct timespec modified;
    mode_t      mode;
} FileEntry;

typedef struct FsProvider {
    int  (*list_dir)(const char *path, FileEntry **entries, int *count);
    int  (*stat_path)(const char *path, FileEntry *out);
    int  (*exists)(const char *path);
    int  (*is_dir)(const char *path);
    void (*free_entries)(FileEntry *entries);
    int  (*mkdir)(const char *path);
} FsProvider;

extern FsProvider fs_local;

int fs_entries_sort(FileEntry *entries, int count, int sort_by, int reverse);

#endif
