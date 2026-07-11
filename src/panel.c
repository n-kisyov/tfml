#include "panel.h"
#include "ui.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include <unistd.h>
#include <ctype.h>

void panel_init(Panel *p, const char *start_dir) {
    memset(p,0,sizeof(Panel)); p->tab_count=1; p->active_tab=0;
    strncpy(p->tabs[0].path,start_dir,4095);
    const char *name=strrchr(start_dir,'/');
    strncpy(p->tabs[0].display_name,name?name+1:start_dir,63);
    p->needs_refresh=1;p->dirty=1; }

void panel_free(Panel *p) { free(p->entries); free(p->tagged); p->entries=NULL;p->entry_count=p->tagged_count=0; }

void panel_refresh(Panel *p, const FsProvider *fs) {
    PanelTab *tab=&p->tabs[p->active_tab];
    free(p->entries);p->entries=NULL;p->entry_count=0;free(p->tagged);p->tagged=NULL;p->tagged_count=0;
    fs->list_dir(tab->path,&p->entries,&p->entry_count);
    if(p->entries&&p->entry_count) fs_entries_sort(p->entries,p->entry_count,p->sort_by,p->sort_reverse);
    tab->cursor=tab->scroll_offset=0; p->needs_refresh=0;p->dirty=1; }

void panel_enter_dir(Panel *p, const FsProvider *fs) {
    PanelTab *tab=&p->tabs[p->active_tab]; if(!p->entry_count)return;
    FileEntry *e=&p->entries[tab->cursor];
    if(e->type==ENTRY_DIR){ char *np=path_join(tab->path,e->name);
        strncpy(tab->path,np,4095); strncpy(tab->display_name,e->name,63); free(np);
        char drive=tab->path[0]; if(drive) strncpy(p->drive_paths[0],tab->path,4095);
        tab->cursor=tab->scroll_offset=0; panel_refresh(p,fs); } }

void panel_go_parent(Panel *p, const FsProvider *fs) {
    if(p->in_drive_list){panel_exit_drives(p,fs);return;}
    PanelTab *tab=&p->tabs[p->active_tab];
    if(is_root_path(tab->path)){panel_go_drives(p,fs);return;}
    char *parent=get_parent_path(tab->path);strncpy(tab->path,parent,4095);
    const char *name=strrchr(parent,'/');strncpy(tab->display_name,name&&name[0]?name+1:parent,63);
    free(parent);tab->cursor=tab->scroll_offset=0;panel_refresh(p,fs);}

void panel_go_drives(Panel *p, const FsProvider *fs) {
    if(p->in_drive_list){panel_exit_drives(p,fs);return;}
    PanelTab *tab=&p->tabs[p->active_tab];(void)fs;
    strncpy(p->drive_paths[0],tab->path,4095);
    p->saved_entry_count=p->entry_count;p->saved_cursor=tab->cursor;
    strncpy(p->saved_path,tab->path,4095);
    if(p->saved_entry_count>0&&p->saved_entry_count<=4096)
        memcpy(p->saved_entries,p->entries,p->saved_entry_count*sizeof(FileEntry));
    free(p->entries);p->entries=(FileEntry*)calloc(27,sizeof(FileEntry));p->entry_count=0;p->in_drive_list=1;
    FILE *mt=fopen("/proc/mounts","r"); if(mt) {
        struct mntent *me; int idx=0;
        while((me=getmntent(mt))!=NULL&&idx<25) {
            if(strncmp(me->mnt_dir,"/proc",5)==0||strncmp(me->mnt_dir,"/sys",4)==0||
               strncmp(me->mnt_dir,"/dev",4)==0||strncmp(me->mnt_dir,"/run",4)==0) continue;
            p->entries[p->entry_count].type=ENTRY_DIR;
            if(p->drive_paths[idx][0])
                snprintf(p->entries[p->entry_count].name,256,"%s    (%s)",me->mnt_dir,p->drive_paths[idx]);
            else snprintf(p->entries[p->entry_count].name,256,"%s",me->mnt_dir);
            p->entry_count++;idx++;
        }
        fclose(mt);
    }
    if(!p->entry_count){p->entries[0].type=ENTRY_DIR;strcpy(p->entries[0].name,"/");p->entry_count=1;}
    tab->cursor=tab->scroll_offset=0;strcpy(tab->path,"Mounts");strncpy(tab->display_name,"Mounts",63);p->dirty=1;}

void panel_exit_drives(Panel *p, const FsProvider *fs) {
    (void)fs;PanelTab *tab=&p->tabs[p->active_tab];free(p->entries);p->in_drive_list=0;
    p->entry_count=p->saved_entry_count;strncpy(tab->path,p->saved_path,4095);
    const char *nm=strrchr(p->saved_path,'/');strncpy(tab->display_name,nm?nm+1:p->saved_path,63);
    if(p->saved_entry_count>0){p->entries=(FileEntry*)calloc(p->saved_entry_count,sizeof(FileEntry));
    memcpy(p->entries,p->saved_entries,p->saved_entry_count*sizeof(FileEntry));tab->cursor=p->saved_cursor;}
    else{p->entries=NULL;tab->cursor=0;} tab->scroll_offset=0;p->dirty=1;}

void panel_enter_on_drive(Panel *p, const FsProvider *fs) {
    PanelTab *tab=&p->tabs[p->active_tab]; if(!p->in_drive_list||!p->entry_count)return;
    FileEntry *e=&p->entries[tab->cursor]; char *sp=strchr(e->name,' '); if(sp)*sp=0;
    p->in_drive_list=0;free(p->entries);p->entries=NULL;p->entry_count=0;
    if(fs->exists(e->name)&&fs->is_dir(e->name)) strncpy(tab->path,e->name,4095);
    else strncpy(tab->path,"/",4095);
    const char *nm=strrchr(tab->path,'/');strncpy(tab->display_name,nm?nm+1:tab->path,63);
    tab->cursor=tab->scroll_offset=0;panel_refresh(p,fs);}

void panel_cursor_up(Panel *p) { PanelTab *tab=&p->tabs[p->active_tab];if(!p->entry_count)return;
    if(tab->cursor>0){tab->cursor--;if(tab->cursor<tab->scroll_offset)tab->scroll_offset=tab->cursor;p->dirty=1;}}
void panel_cursor_down(Panel *p) { PanelTab *tab=&p->tabs[p->active_tab];if(!p->entry_count)return;
    if(tab->cursor<p->entry_count-1){tab->cursor++;p->dirty=1;}}
void panel_page_up(Panel *p,int ph){PanelTab *t=&p->tabs[p->active_tab];if(!p->entry_count)return;
    t->cursor-=ph;if(t->cursor<0)t->cursor=0;t->scroll_offset-=ph;if(t->scroll_offset<0)t->scroll_offset=0;p->dirty=1;}
void panel_page_down(Panel *p,int ph){PanelTab *t=&p->tabs[p->active_tab];if(!p->entry_count)return;
    t->cursor+=ph;if(t->cursor>=p->entry_count)t->cursor=p->entry_count-1;t->scroll_offset+=ph;p->dirty=1;}
void panel_cursor_home(Panel *p){p->tabs[p->active_tab].cursor=p->tabs[p->active_tab].scroll_offset=0;p->dirty=1;}
void panel_cursor_end(Panel *p){if(p->entry_count)p->tabs[p->active_tab].cursor=p->entry_count-1;p->dirty=1;}
void panel_toggle_tag(Panel *p){PanelTab *t=&p->tabs[p->active_tab];if(!p->entry_count)return;
    int idx=t->cursor;for(int i=0;i<p->tagged_count;i++)if(p->tagged[i]==idx)
    {for(int j=i;j<p->tagged_count-1;j++)p->tagged[j]=p->tagged[j+1];p->tagged_count--;p->dirty=1;return;}
    p->tagged=(int*)xrealloc(p->tagged,(p->tagged_count+1)*sizeof(int));p->tagged[p->tagged_count++]=idx;p->dirty=1;}
void panel_clear_tags(Panel *p){free(p->tagged);p->tagged=NULL;p->tagged_count=0;p->dirty=1;}

int panel_tagged_or_current(const Panel *p, FileEntry **out, int *count) {
    if(p->tagged_count>0){*count=p->tagged_count;*out=(FileEntry*)malloc(p->tagged_count*sizeof(FileEntry));
        for(int i=0;i<p->tagged_count;i++)(*out)[i]=p->entries[p->tagged[i]];return 1;}
    if(p->entry_count>0){*count=1;*out=(FileEntry*)malloc(sizeof(FileEntry));
        (*out)[0]=p->entries[p->tabs[p->active_tab].cursor];return 1;}
    *count=0;*out=NULL;return 0;}

int panel_tab_new(Panel *p) { if(p->tab_count>=PANEL_MAX_TABS)return 0;
    PanelTab *nt=&p->tabs[p->tab_count]; memset(nt,0,sizeof(PanelTab));
    char *home=get_home_dir();strncpy(nt->path,home,4095);strncpy(nt->display_name,"Home",63);
    p->active_tab=p->tab_count++;p->needs_refresh=1;p->dirty=1;return 1;}

int panel_tab_close(Panel *p) { if(p->tab_count<=1||p->active_tab==0)return 0;
    int idx=p->active_tab;for(int i=idx;i<p->tab_count-1;i++)p->tabs[i]=p->tabs[i+1];
    p->tab_count--;if(p->active_tab>=p->tab_count)p->active_tab=p->tab_count-1;p->needs_refresh=1;p->dirty=1;return 1;}

int panel_tab_next(Panel *p) { if(p->tab_count<=1)return 0;
    p->active_tab=(p->active_tab+1)%p->tab_count;p->needs_refresh=1;p->dirty=1;return 1;}

int panel_tab_prev(Panel *p) { if(p->tab_count<=1)return 0;
    p->active_tab--;if(p->active_tab<0)p->active_tab=p->tab_count-1;p->needs_refresh=1;p->dirty=1;return 1;}

void panel_tab_rename(Panel *p) { PanelTab *t=&p->tabs[p->active_tab];
    const char *n=strrchr(t->path,'/');strncpy(t->display_name,n&&n[0]?n+1:t->path,63);p->dirty=1;}

const char *panel_current_path(const Panel *p) { return p->tabs[p->active_tab].path; }
int panel_list_height(const Panel *p) { return p->entry_count; }
int panel_is_tagged(const Panel *p,int idx) { for(int i=0;i<p->tagged_count;i++)if(p->tagged[i]==idx)return 1;return 0; }

void panel_render(const Panel *p, const Theme *theme, int x, int y, int w, int h, int focused) {
    uint32_t bc=focused?theme_get(theme,COLOR_FOCUS_BORDER):theme_get(theme,COLOR_PANEL_BORDER);
    PanelTab *tab=(PanelTab*)&p->tabs[p->active_tab];
    ui_set_bg(theme_get(theme,COLOR_BG)); for(int iy=0;iy<h;iy++)ui_draw_h_line(x,y+iy,w,' ');
    ui_set_bg(theme_get(theme,COLOR_BG));ui_set_fg(bc);ui_draw_rect(x,y,w,h);
    for(int ti=0;ti<p->tab_count&&ti<PANEL_MAX_TABS;ti++){int tx=x+2+ti*20;if(tx>=x+w-4)break;
        int tw2=18;if(tx+tw2>x+w-2)tw2=(x+w-2)-tx;if(tw2<3)break;
        if(ti==p->active_tab){ui_set_bg(theme_get(theme,COLOR_TAB_ACTIVE_BG));ui_set_fg(theme_get(theme,COLOR_TAB_ACTIVE));ui_set_bold();}
        else{ui_set_bg(theme_get(theme,COLOR_BG));ui_set_fg(theme_get(theme,COLOR_TAB_INACTIVE));}
        char lb[32];const char *tn=p->tabs[ti].display_name;if(!tn||!tn[0])tn="?";
        int tlen=(int)strlen(tn);if(tlen>tw2-4){strncpy(lb,tn,tw2-4);lb[tw2-4]=0;strcat(lb,"...");}
        else snprintf(lb,32," %d:%-*s ",ti+1,tw2-6,tn);
        ui_draw_text(tx,y+1,lb);ui_reset_colors();}
    ui_set_bg(theme_get(theme,COLOR_BG));
    if(p->in_drive_list){ui_set_fg(theme_get(theme,COLOR_FOCUS_BORDER));ui_set_bold();
        ui_draw_text_trunc(x+2,y+2,w-4,"> Select Mount (Enter=switch  Esc=back)"); }
    else{ui_set_fg(theme_get(theme,COLOR_FILE));ui_set_dim();ui_draw_text_trunc(x+2,y+2,w-4,tab->path);}
    ui_reset_colors();
    int lsy=y+3,lh=h-5;if(lh<1)lh=1;
    if(tab->cursor<tab->scroll_offset)tab->scroll_offset=tab->cursor;
    if(tab->cursor>=tab->scroll_offset+lh)tab->scroll_offset=tab->cursor-lh+1;
    if(tab->scroll_offset<0)tab->scroll_offset=0;
    for(int i=0;i<lh;i++){int ei=tab->scroll_offset+i,ry=lsy+i,rx=x+1;ui_move(rx,ry);
    if(ei>=p->entry_count){ui_set_bg(theme_get(theme,COLOR_BG));ui_set_fg(theme_get(theme,COLOR_BG));
        ui_draw_h_line(rx,ry,w-2,' ');ui_reset_colors();continue;}
    FileEntry *e=&p->entries[ei];int tagged=panel_is_tagged(p,ei),cursor=(ei==tab->cursor);
    if(cursor){ui_set_bg(theme_get(theme,COLOR_SELECTED_BG));ui_set_fg(theme_get(theme,COLOR_SELECTED_FG));}
    else if(tagged){ui_set_bg(theme_get(theme,COLOR_TAGGED_BG));} else ui_set_bg(theme_get(theme,COLOR_BG));
    if(tagged){ui_set_fg(theme_get(theme,COLOR_TAGGED));ui_draw_char(rx,ry,'*');} else ui_draw_char(rx,ry,' '); rx++;
    if(cursor)ui_set_fg(theme_get(theme,COLOR_SELECTED_FG));
    else if(e->type==ENTRY_DIR)ui_set_fg(theme_get(theme,COLOR_DIR));
    else if(e->type==ENTRY_SYMLINK)ui_set_fg(theme_get(theme,COLOR_SYMLINK));
    else ui_set_fg(theme_get(theme,COLOR_FILE));
    if(e->type==ENTRY_FILE){const char *ext=get_file_ext(e->name);
        if(ext&&(strcasecmp(ext,"sh")==0||strcasecmp(ext,"")==0)){if(!cursor)ui_set_fg(theme_get(theme,COLOR_PROGRESS));}}
    char szb[32];int nw=w-22;if(nw<10)nw=10;int isdir=(e->type==ENTRY_DIR);
    char nd[512];if(isdir)snprintf(nd,512," %s/",e->name);else snprintf(nd,512," %s",e->name);
    ui_draw_text_trunc(rx,ry,nw,nd); format_file_size(e->size,szb,32);if(isdir)strcpy(szb,"<DIR>");
    ui_set_fg(cursor?theme_get(theme,COLOR_SELECTED_FG):theme_get(theme,COLOR_FILE));ui_set_dim();
    ui_draw_text(x+w-22,ry,szb);ui_reset_colors();}
    int sy=y+h-2;ui_set_bg(theme_get(theme,COLOR_BG));ui_set_fg(theme_get(theme,COLOR_FILE));ui_set_dim();
    char info[256];snprintf(info,256," %d/%d files ",p->tagged_count,p->entry_count);ui_draw_text(x+2,sy,info);
    if(p->entry_count>lh){int pct=p->entry_count?(tab->cursor*100)/p->entry_count:0;
        snprintf(info,256," %d%% ",pct);int ix=x+w-(int)strlen(info)-3;if(ix>x+20)ui_draw_text(ix,sy,info);}
    ui_reset_colors();}
