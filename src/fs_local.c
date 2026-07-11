#include "fs.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static int fs_local_list_dir(const char *path, FileEntry **entries, int *count) {
    *entries=NULL; *count=0;
    DIR *d=opendir(path);
    if(!d) return -1;
    int cap=256,cnt=0;
    FileEntry *list=(FileEntry*)xcalloc(cap,sizeof(FileEntry));
    struct dirent *de;
    while((de=readdir(d))!=NULL) {
        if(strcmp(de->d_name,".")==0||strcmp(de->d_name,"..")==0) continue;
        if(cnt>=cap){cap*=2;list=(FileEntry*)xrealloc(list,cap*sizeof(FileEntry));}
        memcpy(list[cnt].name,de->d_name,255);list[cnt].name[255]=0;
        char full[4096];
        snprintf(full,4096,"%s/%s",path,de->d_name);
        struct stat st;
        if(lstat(full,&st)==0) {
            list[cnt].size=st.st_size;
            list[cnt].modified=st.st_mtim;
            list[cnt].mode=st.st_mode;
            if(S_ISDIR(st.st_mode)) list[cnt].type=ENTRY_DIR;
            else if(S_ISLNK(st.st_mode)) list[cnt].type=ENTRY_SYMLINK;
            else list[cnt].type=ENTRY_FILE;
        }
        cnt++;
    }
    closedir(d);
    *entries=list; *count=cnt;
    return 0;
}

static int fs_local_stat(const char *path, FileEntry *out) {
    struct stat st;
    if(stat(path,&st)!=0) return -1;
    memset(out,0,sizeof(FileEntry));
    const char *name=strrchr(path,'/');
    if(!name) name=path; else name++;
    strncpy(out->name,name,255);
    out->size=st.st_size;
    out->modified=st.st_mtim;
    out->mode=st.st_mode;
    if(S_ISDIR(st.st_mode)) out->type=ENTRY_DIR;
    else if(S_ISLNK(st.st_mode)) out->type=ENTRY_SYMLINK;
    else out->type=ENTRY_FILE;
    return 0;
}

static int fs_local_exists(const char *path) { return access(path,F_OK)==0; }
static int fs_local_is_dir(const char *path) { struct stat st; return stat(path,&st)==0&&S_ISDIR(st.st_mode); }
static void fs_local_free(FileEntry *entries) { free(entries); }
static int fs_local_mkdir(const char *path) { return mkdir(path,0755); }

FsProvider fs_local = { fs_local_list_dir, fs_local_stat, fs_local_exists, fs_local_is_dir, fs_local_free, fs_local_mkdir };
