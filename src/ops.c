#include "ops.h"
#include "ui.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

static int confirm_overwrite(const Theme *theme, const char *name) {
    char msg[1024]; snprintf(msg,1024,"Overwrite \"%s\"?",name);
    return ui_confirm_dialog(theme,"File exists",msg); }

static int copy_file(const char *src, const char *dest) {
    int sfd=open(src,O_RDONLY); if(sfd<0) return -1;
    int dfd=open(dest,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(dfd<0) { close(sfd); return -1; }
    char buf[65536]; ssize_t n; int ok=0;
    while((n=read(sfd,buf,sizeof(buf)))>0) { if(write(dfd,buf,n)!=n) break; ok=1; }
    close(sfd); close(dfd); if(!ok) { unlink(dest); return -1; } return 0;
}

static int copy_dir_tree(const char *src, const char *dest, const Theme *theme, int *copied) {
    mkdir(dest,0755);
    DIR *d=opendir(src); if(!d) return -1;
    struct dirent *de;
    while((de=readdir(d))!=NULL) {
        if(strcmp(de->d_name,".")==0||strcmp(de->d_name,"..")==0) continue;
        char *sf=path_join(src,de->d_name),*df=path_join(dest,de->d_name);
        struct stat st;
        if(lstat(sf,&st)==0) {
            if(S_ISDIR(st.st_mode)) copy_dir_tree(sf,df,theme,copied);
            else {
                if(access(df,F_OK)==0) { if(!confirm_overwrite(theme,de->d_name)) {free(sf);free(df);continue;} unlink(df); }
                if(copy_file(sf,df)==0) (*copied)++;
            }
        }
        free(sf); free(df);
    }
    closedir(d);
    return 0;
}

int ops_copy_files(const char **paths, int count, const char *dest_dir, const Theme *theme) {
    if(!paths||count<=0||!dest_dir) return -1; int copied=0;
    for(int i=0;i<count;i++) {
        const char *name=strrchr(paths[i],'/'); if(!name) name=paths[i]; else name++;
        char *dest=path_join(dest_dir,name); if(!dest) continue;
        struct stat st; if(lstat(paths[i],&st)!=0) { free(dest); continue; }
        if(S_ISDIR(st.st_mode)) { copy_dir_tree(paths[i],dest,theme,&copied); }
        else {
            if(access(dest,F_OK)==0) { if(!confirm_overwrite(theme,name)) { free(dest); continue; } unlink(dest); }
            if(copy_file(paths[i],dest)==0) copied++;
        }
        free(dest);
    }
    return copied;
}

static int delete_dir_tree(const char *path) {
    DIR *d=opendir(path); if(!d) { rmdir(path); return 0; }
    struct dirent *de;
    while((de=readdir(d))!=NULL) {
        if(strcmp(de->d_name,".")==0||strcmp(de->d_name,"..")==0) continue;
        char *full=path_join(path,de->d_name);
        struct stat st; if(lstat(full,&st)==0) {
            if(S_ISDIR(st.st_mode)) delete_dir_tree(full); else { chmod(full,0644); unlink(full); }
        }
        free(full);
    }
    closedir(d);
    rmdir(path); return 0;
}

int ops_move_files(const char **paths, int count, const char *dest_dir, const Theme *theme) {
    if(!paths||count<=0||!dest_dir) return -1; int moved=0;
    for(int i=0;i<count;i++) {
        const char *name=strrchr(paths[i],'/'); if(!name) name=paths[i]; else name++;
        char *dest=path_join(dest_dir,name); if(!dest) continue;
        struct stat st; if(lstat(paths[i],&st)!=0) { free(dest); continue; }
        if(rename(paths[i],dest)==0) { moved++; free(dest); continue; }
        /* cross-filesystem: copy + delete */
        if(S_ISDIR(st.st_mode)) { int c=0; copy_dir_tree(paths[i],dest,theme,&c); if(c>0){delete_dir_tree(paths[i]);moved++;} }
        else { if(access(dest,F_OK)==0) { if(!confirm_overwrite(theme,name)){free(dest);continue;} unlink(dest); }
               if(copy_file(paths[i],dest)==0) { unlink(paths[i]); moved++; } }
        free(dest);
    }
    return moved;
}

int ops_delete_files(const char **paths, int count, const Theme *theme) {
    (void)theme; if(!paths||count<=0) return -1; int deleted=0;
    for(int i=0;i<count;i++) {
        struct stat st; if(lstat(paths[i],&st)!=0) continue;
        if(S_ISDIR(st.st_mode)) { delete_dir_tree(paths[i]); deleted++; }
        else { chmod(paths[i],0644); if(unlink(paths[i])==0) deleted++; }
    }
    return deleted;
}

int ops_mkdir_dialog(const char *parent_dir, const Theme *theme, const FsProvider *fs,
                     char *new_name_out, int name_sz) {
    char name[256]={0};
    if(!ui_input_dialog(theme,"Create directory name:",name,256)) return 0;
    if(!name[0]) return 0;
    char *full=path_join(parent_dir,name);
    int ok=fs->mkdir(full);
    if(new_name_out&&name_sz>0) strncpy(new_name_out,name,name_sz-1);
    free(full);
    if(ok!=0) { char msg[512]; snprintf(msg,512,"Failed to create \"%s\"",name);
        ui_message_box(theme,"Error",msg); return 0; }
    return 1;
}
