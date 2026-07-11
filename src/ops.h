#ifndef OPS_H
#define OPS_H

#include "theme.h"
#include "fs.h"

int ops_copy_files(const char **paths, int count, const char *dest_dir, const Theme *theme);
int ops_move_files(const char **paths, int count, const char *dest_dir, const Theme *theme);
int ops_delete_files(const char **paths, int count, const Theme *theme);
int ops_mkdir_dialog(const char *parent_dir, const Theme *theme, const FsProvider *fs,
                     char *new_name_out, int name_sz);

#endif
