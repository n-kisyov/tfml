#include "bgop.h"
#include "utils.h"
#include "fs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

uint64_t bgop_tick_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    return (uint64_t)ts.tv_sec*1000+ts.tv_nsec/1000000; }

void bgop_init(BgTask *t) { memset(t,0,sizeof(BgTask)); pthread_mutex_init(&t->lock,NULL); }

void bgop_free(BgTask *t) {
    pthread_mutex_lock(&t->lock);
    if(t->thread) pthread_detach(t->thread);
    for(int i=0;i<t->path_count;i++) free(t->paths[i]);
    t->path_count=0; pthread_mutex_unlock(&t->lock);
    pthread_mutex_destroy(&t->lock); memset(t,0,sizeof(BgTask)); }

int bgop_is_active(const BgTask *t) { return __atomic_load_n(&t->active,__ATOMIC_SEQ_CST)!=0; }
void bgop_lock(BgTask *t)  { pthread_mutex_lock(&t->lock); }
void bgop_unlock(BgTask *t){ pthread_mutex_unlock(&t->lock); }

void bgop_history_push(BgOpRecord *hist, int *count, int cap,
                       BgOpType op, int total, int done, const char *desc, int status) {
    if(*count>=cap){memmove(hist,hist+1,(cap-1)*sizeof(BgOpRecord));*count=cap-1;}
    BgOpRecord *r=&hist[*count]; r->op_type=op; r->total=total; r->done=done; r->status=status;
    if(desc) strncpy(r->desc,desc,127); else r->desc[0]=0; (*count)++; }

static void update_progress(BgTask *t, const char *file) {
    pthread_mutex_lock(&t->lock);
    if(file) strncpy(t->current_file,file,259);
    __atomic_fetch_add(&t->done_items,1,__ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&t->lock); }

static int bg_copy_file(const char *src, const char *dest) {
    int sfd=open(src,O_RDONLY); if(sfd<0) return -1;
    int dfd=open(dest,O_WRONLY|O_CREAT|O_TRUNC,0644); if(dfd<0){close(sfd);return -1;}
    char buf[65536]; ssize_t n; int ok=0;
    while((n=read(sfd,buf,sizeof(buf)))>0){if(write(dfd,buf,n)!=n)break;ok=1;}
    close(sfd);close(dfd);if(!ok){unlink(dest);return -1;}return 0; }

static int bg_copy_dir_tree(const char *src, const char *dest) {
    mkdir(dest,0755); DIR *d=opendir(src); if(!d) return -1;
    struct dirent *de;
    while((de=readdir(d))!=NULL) { if(strcmp(de->d_name,".")==0||strcmp(de->d_name,"..")==0)continue;
        char *sf=path_join(src,de->d_name),*df=path_join(dest,de->d_name);
        struct stat st; if(lstat(sf,&st)==0){if(S_ISDIR(st.st_mode))bg_copy_dir_tree(sf,df);else bg_copy_file(sf,df);}
        free(sf);free(df); }
    closedir(d); return 0; }

static int bg_delete_dir_tree(const char *path) {
    DIR *d=opendir(path); if(!d){rmdir(path);return 0;}
    struct dirent *de;
    while((de=readdir(d))!=NULL){if(strcmp(de->d_name,".")==0||strcmp(de->d_name,"..")==0)continue;
        char *full=path_join(path,de->d_name); struct stat st;
        if(lstat(full,&st)==0){if(S_ISDIR(st.st_mode))bg_delete_dir_tree(full);else{chmod(full,0644);unlink(full);}}
        free(full); } closedir(d); rmdir(path); return 0; }

static void *bg_thread_copy(void *param) {
    BgTask *t=(BgTask*)param; t->start_time.tv_sec=0; clock_gettime(CLOCK_MONOTONIC,&t->start_time);
    strncpy(t->title,"Copying files...",63);
    for(int i=0;i<t->path_count&&!t->error;i++) {
        const char *src=t->paths[i],*name=strrchr(src,'/');if(!name)name=src;else name++;
        char *dest=path_join(t->dest_dir,name);if(!dest)continue;
        struct stat st;if(lstat(src,&st)!=0){free(dest);continue;}
        if(S_ISDIR(st.st_mode)) bg_copy_dir_tree(src,dest); else {if(access(dest,F_OK)==0)unlink(dest);bg_copy_file(src,dest);}
        update_progress(t,name);free(dest); }
    __atomic_store_n(&t->finished,1,__ATOMIC_SEQ_CST);__atomic_store_n(&t->active,0,__ATOMIC_SEQ_CST);
    return NULL; }

static void *bg_thread_move(void *param) {
    BgTask *t=(BgTask*)param; clock_gettime(CLOCK_MONOTONIC,&t->start_time);
    strncpy(t->title,"Moving files...",63);
    for(int i=0;i<t->path_count&&!t->error;i++) {
        const char *src=t->paths[i],*name=strrchr(src,'/');if(!name)name=src;else name++;
        char *dest=path_join(t->dest_dir,name);if(!dest)continue;
        struct stat st;if(lstat(src,&st)!=0){free(dest);continue;}
        if(S_ISDIR(st.st_mode)){if(rename(src,dest)!=0){bg_copy_dir_tree(src,dest);bg_delete_dir_tree(src);}}
        else{if(access(dest,F_OK)==0)unlink(dest);if(rename(src,dest)!=0){bg_copy_file(src,dest);unlink(src);}}
        update_progress(t,name);free(dest); }
    __atomic_store_n(&t->finished,1,__ATOMIC_SEQ_CST);__atomic_store_n(&t->active,0,__ATOMIC_SEQ_CST);
    return NULL; }

static void *bg_thread_delete(void *param) {
    BgTask *t=(BgTask*)param; clock_gettime(CLOCK_MONOTONIC,&t->start_time);
    strncpy(t->title,"Deleting files...",63);
    for(int i=0;i<t->path_count&&!t->error;i++) {
        const char *src=t->paths[i],*name=strrchr(src,'/');if(!name)name=src;else name++;
        struct stat st;if(lstat(src,&st)!=0)continue;
        if(S_ISDIR(st.st_mode)) bg_delete_dir_tree(src); else {chmod(src,0644);unlink(src);}
        update_progress(t,name); }
    __atomic_store_n(&t->finished,1,__ATOMIC_SEQ_CST);__atomic_store_n(&t->active,0,__ATOMIC_SEQ_CST);
    return NULL; }

int bgop_start_copy(BgTask *t) {
    if(bgop_is_active(t))return 0;t->op_type=BGOP_COPY;t->active=1;t->finished=0;t->visible=1;
    t->error=0;t->done_items=0;t->total_items=t->path_count;t->current_file[0]=0;
    return pthread_create(&t->thread,NULL,bg_thread_copy,t)==0; }

int bgop_start_move(BgTask *t) {
    if(bgop_is_active(t))return 0;t->op_type=BGOP_MOVE;t->active=1;t->finished=0;t->visible=1;
    t->error=0;t->done_items=0;t->total_items=t->path_count;t->current_file[0]=0;
    return pthread_create(&t->thread,NULL,bg_thread_move,t)==0; }

int bgop_start_delete(BgTask *t) {
    if(bgop_is_active(t))return 0;t->op_type=BGOP_DELETE;t->active=1;t->finished=0;t->visible=1;
    t->error=0;t->done_items=0;t->total_items=t->path_count;t->current_file[0]=0;
    return pthread_create(&t->thread,NULL,bg_thread_delete,t)==0; }
