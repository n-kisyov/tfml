#include "fs.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

static int g_sort_by;
static int g_sort_reverse;

static int cmp_entry(const void *va, const void *vb) {
    const FileEntry *a=(const FileEntry*)va,*b=(const FileEntry*)vb;
    int aisdir=(a->type==ENTRY_DIR), bisdir=(b->type==ENTRY_DIR);
    if(aisdir&&!bisdir) return -1;
    if(!aisdir&&bisdir) return 1;
    int cmp=0;
    switch(g_sort_by) {
    case 0: cmp=str_compare_natural(a->name,b->name); break;
    case 1: if(a->size<b->size)cmp=-1;else if(a->size>b->size)cmp=1;
            { if(cmp==0)cmp=str_compare_natural(a->name,b->name); } break;
    case 2: if(a->modified.tv_sec<b->modified.tv_sec)cmp=-1;
            else if(a->modified.tv_sec>b->modified.tv_sec)cmp=1;
            { if(cmp==0)cmp=str_compare_natural(a->name,b->name); } break;
    }
    return g_sort_reverse?-cmp:cmp;
}

int fs_entries_sort(FileEntry *entries, int count, int sort_by, int reverse) {
    if(count<=1) return 0;
    g_sort_by=sort_by;g_sort_reverse=reverse;
    qsort(entries,count,sizeof(FileEntry),cmp_entry);
    return 0;
}
