#include "cmdline.h"
#include "ui.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

void cmdline_init(CmdLine *c) { memset(c,0,sizeof(CmdLine)); }

void cmdline_free(CmdLine *c) { for(int i=0;i<c->history_count;i++)free(c->history[i]); free(c->last_output); memset(c,0,sizeof(CmdLine)); }

void cmdline_insert(CmdLine *c, char ch) {
    if(c->length>=CMDLINE_MAX_LEN-1)return;
    memmove(c->buffer+c->cursor+1,c->buffer+c->cursor,c->length-c->cursor+1);
    c->buffer[c->cursor]=ch;c->cursor++;c->length++;
}

void cmdline_backspace(CmdLine *c) { if(c->cursor<=0)return;
    memmove(c->buffer+c->cursor-1,c->buffer+c->cursor,c->length-c->cursor+1);c->cursor--;c->length--; }

void cmdline_delete(CmdLine *c) { if(c->cursor>=c->length)return;
    memmove(c->buffer+c->cursor,c->buffer+c->cursor+1,c->length-c->cursor);c->length--; }

void cmdline_cursor_left(CmdLine *c) { if(c->cursor>0)c->cursor--; }
void cmdline_cursor_right(CmdLine *c) { if(c->cursor<c->length)c->cursor++; }
void cmdline_cursor_home(CmdLine *c) { c->cursor=0; }
void cmdline_cursor_end(CmdLine *c) { c->cursor=c->length; }

void cmdline_history_prev(CmdLine *c) { if(c->history_count==0)return;
    if(c->history_pos<c->history_count){c->history_pos++;
    strncpy(c->buffer,c->history[c->history_count-c->history_pos],CMDLINE_MAX_LEN-1);
    c->length=(int)strlen(c->buffer);c->cursor=c->length;} }

void cmdline_history_next(CmdLine *c) { if(c->history_pos<=0)return;c->history_pos--;
    if(c->history_pos==0){c->buffer[0]=0;c->length=c->cursor=0;}
    else{strncpy(c->buffer,c->history[c->history_count-c->history_pos],CMDLINE_MAX_LEN-1);
    c->length=(int)strlen(c->buffer);c->cursor=c->length;} }

static void add_history(CmdLine *c, const char *cmd) {
    if(!cmd||!cmd[0])return;
    if(c->history_count>0&&strcmp(c->history[c->history_count-1],cmd)==0)return;
    if(c->history_count>=CMDLINE_HISTORY_MAX){free(c->history[0]);
    memmove(c->history,c->history+1,(CMDLINE_HISTORY_MAX-1)*sizeof(char*));c->history_count--;}
    c->history[c->history_count++]=strdup_safe(cmd);c->history_pos=0; }

int cmdline_execute(CmdLine *c) {
    if(c->length==0)return 0;
    c->buffer[c->length]=0;add_history(c,c->buffer);
    char cmd[CMDLINE_MAX_LEN];memcpy(cmd,c->buffer,CMDLINE_MAX_LEN-1);cmd[CMDLINE_MAX_LEN-1]=0;
    char tmp_path[]="/tmp/tfm-out-XXXXXX";
    int tmp_fd=mkstemp(tmp_path);if(tmp_fd<0)goto cleanup;
    pid_t pid=fork();
    if(pid==0){ dup2(tmp_fd,STDOUT_FILENO);dup2(tmp_fd,STDERR_FILENO);close(tmp_fd);
        execl("/bin/sh","sh","-c",cmd,NULL);_exit(1); }
    close(tmp_fd);
    if(pid>0){int st;waitpid(pid,&st,0);}
    free(c->last_output);c->last_output=NULL;
    FILE *f=fopen(tmp_path,"r");if(f){fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
    if(sz>0&&sz<65536){c->last_output=(char*)calloc(sz+1,1);fread(c->last_output,1,sz,f);c->show_output=1;c->output_scroll=0;}fclose(f);}
    unlink(tmp_path);
cleanup:
    c->buffer[0]=0;c->length=c->cursor=0;return 1; }

void cmdline_clear(CmdLine *c) { c->buffer[0]=0;c->length=c->cursor=0; }

void cmdline_render(const CmdLine *c, const Theme *theme, int x, int y, int w, int focused) {
    if(c->show_output&&c->last_output){ int tw=w,th=10,tx=x,ty=y-th-1;if(ty<1)ty=1;
        ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_DIALOG_BORDER));
        ui_draw_rect(tx,ty,tw,th+2);ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_draw_rect_content(tx+1,ty+1,tw-2,th);
        ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_FILE));
        int line=0;const char *p=c->last_output;
        while(*p&&line<th){const char *nl=strchr(p,'\n');int pl=nl?(int)(nl-p):(int)strlen(p);
        if(pl>tw-4)pl=tw-4;
        char lbuf[512];memcpy(lbuf,p,pl);lbuf[pl]=0;
        ui_draw_text_trunc(tx+2,ty+1+line,tw-4,lbuf);line++;if(nl)p=nl+1;else break;}
        ui_set_fg(theme_get(theme,COLOR_DIALOG_BORDER));
        {const char *ft="Press any key to dismiss";ui_draw_text(tx+(tw-(int)strlen(ft))/2,ty+th+1,ft);}
        ui_reset_colors();return; }
    ui_set_fg(focused?theme_get(theme,COLOR_FOCUS_BORDER):theme_get(theme,COLOR_PANEL_BORDER));
    ui_draw_char(x,y,focused?'>':'$');
    ui_set_bg(theme_get(theme,COLOR_BG));ui_set_fg(theme_get(theme,COLOR_CMDLINE));
    char disp[CMDLINE_MAX_LEN+4];disp[0]=' ';strncpy(disp+1,c->buffer,CMDLINE_MAX_LEN+2);
    ui_draw_text_trunc(x+1,y,w-3,disp);
    if(focused){ if(c->cursor<c->length){ui_set_bg(theme_get(theme,COLOR_SELECTED_BG));
        ui_set_fg(theme_get(theme,COLOR_SELECTED_FG));ui_draw_char(x+2+c->cursor,y,c->buffer[c->cursor]);}
        else{ui_reset_colors();ui_set_bg(theme_get(theme,COLOR_CMDLINE));ui_draw_char(x+2+c->cursor,y,' ');} }
    ui_reset_colors(); }
