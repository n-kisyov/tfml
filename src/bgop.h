#ifndef BGOP_H
#define BGOP_H

#include <pthread.h>
#include <stdint.h>

#define BGOP_PATH_MAX 4096
#define BGOP_HISTORY  30

typedef enum { BGOP_NONE = 0, BGOP_COPY, BGOP_MOVE, BGOP_DELETE } BgOpType;

typedef struct {
    BgOpType op_type;
    int      total, done;
    char     desc[128];
    int      status;
} BgOpRecord;

typedef struct BgTask {
    volatile int  active, finished, visible, error;
    BgOpType      op_type;
    int           total_items;
    volatile int  done_items;
    char          current_file[260], title[64];
    struct timespec start_time;

    pthread_mutex_t lock;
    pthread_t       thread;

    char   *paths[4096];
    int     path_count;
    char    dest_dir[BGOP_PATH_MAX];
    int     panel_src_idx, panel_dst_idx;
    void   *fs_provider;
} BgTask;

void  bgop_init(BgTask *t);
void  bgop_free(BgTask *t);
int   bgop_is_active(const BgTask *t);
void  bgop_lock(BgTask *t);
void  bgop_unlock(BgTask *t);
int   bgop_start_copy(BgTask *t);
int   bgop_start_move(BgTask *t);
int   bgop_start_delete(BgTask *t);
void  bgop_history_push(BgOpRecord *hist, int *count, int cap,
                        BgOpType op, int total, int done, const char *desc, int status);
uint64_t bgop_tick_ms(void);

#endif
