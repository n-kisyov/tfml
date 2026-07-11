#include "ui.h"
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdarg.h>

static int   g_termW=80,g_termH=24;
static char  *g_buf=NULL;
static int    g_buf_len=0,g_buf_cap=0;
static uint32_t g_cur_fg=0xFFFFFFFF,g_cur_bg=0xFFFFFFFF;
static int   g_bold=0,g_dim=0,g_reverse=0;

static void buf_append(const char *s, int len) {
    if(len<=0) return;
    if(g_buf_len+len>g_buf_cap) { g_buf_cap=(g_buf_len+len)*2; g_buf=(char*)realloc(g_buf,g_buf_cap); if(!g_buf)abort(); }
    memcpy(g_buf+g_buf_len,s,len); g_buf_len+=len;
}
static void buf_appends(const char *s) { if(s) buf_append(s,(int)strlen(s)); }
static void buf_fmt(const char *fmt, ...) {
    char tmp[512]; va_list ap; va_start(ap,fmt);
    int len=vsnprintf(tmp,512,fmt,ap); va_end(ap);
    if(len>0) buf_append(tmp,len);
}

static void emit_sgr(void) {
    if(g_cur_fg==0xFFFFFFFF&&g_cur_bg==0xFFFFFFFF&&!g_bold&&!g_dim&&!g_reverse) return;
    buf_appends("\x1b[0");
    if(g_cur_fg!=0xFFFFFFFF) buf_fmt(";38;2;%d;%d;%d",(g_cur_fg>>16)&0xFF,(g_cur_fg>>8)&0xFF,g_cur_fg&0xFF);
    if(g_cur_bg!=0xFFFFFFFF) buf_fmt(";48;2;%d;%d;%d",(g_cur_bg>>16)&0xFF,(g_cur_bg>>8)&0xFF,g_cur_bg&0xFF);
    if(g_bold) buf_appends(";1"); if(g_dim) buf_appends(";2"); if(g_reverse) buf_appends(";7");
    buf_appends("m");
}

void ui_init(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    struct winsize ws; if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0){g_termW=ws.ws_col;g_termH=ws.ws_row;}
    write(STDOUT_FILENO,"\x1b[?1049h\x1b[2J\x1b[H",15);
}

void ui_shutdown(void) {
    free(g_buf); g_buf=NULL; g_buf_cap=0;
    write(STDOUT_FILENO,"\x1b[?1049l\x1b[0m",14);
}

void ui_begin_frame(void) {
    g_buf_len=0; g_cur_fg=0xFFFFFFFF; g_cur_bg=0xFFFFFFFF; g_bold=g_dim=g_reverse=0;
    struct winsize ws; if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0){g_termW=ws.ws_col;g_termH=ws.ws_row;}
    buf_appends("\x1b[H");
}

void ui_end_frame(void) { write(STDOUT_FILENO,g_buf,g_buf_len); }

void ui_get_term_size(int *w, int *h) {
    struct winsize ws; if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0){g_termW=ws.ws_col;g_termH=ws.ws_row;}
    if(w)*w=g_termW; if(h)*h=g_termH;
}

void ui_set_fg(uint32_t rgb){g_cur_fg=rgb;} void ui_set_bg(uint32_t rgb){g_cur_bg=rgb;}
void ui_set_bold(void){g_bold=1;} void ui_set_dim(void){g_dim=1;} void ui_reverse(void){g_reverse=1;}
void ui_reset_colors(void){g_cur_fg=g_cur_bg=0xFFFFFFFF;g_bold=g_dim=g_reverse=0;}

void ui_move(int x, int y) { buf_fmt("\x1b[%d;%dH",y+1,x+1); }
void ui_clear_line(int y) { ui_move(0,y); buf_appends("\x1b[2K"); }

void ui_clear_screen(void) {
    emit_sgr();
    for(int y=0;y<g_termH;y++){ui_move(0,y);for(int x=0;x<g_termW;x++)buf_appends(" ");}
}

void ui_draw_text(int x, int y, const char *text) { if(!text)return; emit_sgr();ui_move(x,y);buf_appends(text); }

void ui_draw_text_trunc(int x, int y, int max_w, const char *text) {
    if(!text||max_w<=0) return; emit_sgr(); ui_move(x,y);
    int len=(int)strlen(text); if(len>max_w)len=max_w;
    buf_append(text,len); for(int i=len;i<max_w;i++) buf_appends(" ");
}

void ui_draw_text_centered(int y, int max_w, const char *text) {
    if(!text)return; int len=(int)strlen(text); int x=(max_w-len)/2; if(x<0)x=0; ui_draw_text(x,y,text);
}

void ui_draw_char(int x, int y, char ch) { emit_sgr();ui_move(x,y);buf_append(&ch,1); }

void ui_draw_h_line(int x, int y, int w, char ch) { emit_sgr();ui_move(x,y);for(int i=0;i<w;i++)buf_append(&ch,1); }
void ui_draw_v_line(int x, int y, int h, char ch) { for(int i=0;i<h;i++)ui_draw_char(x,y+i,ch); }

void ui_draw_rect(int x, int y, int w, int h) {
    if(w<2||h<2)return;
    ui_draw_char(x,y,'\xE2\x94\x8C'[0]); /* ┌ */
    buf_append("\xE2\x94\x8C"+1,2); /* rest of utf-8 */
    /* using simpler approach: direct unicode bytes */
    /* top-left */ ui_move(x,y); buf_appends("\xe2\x94\x8c"); /* ┌ */
    /* top-right */ ui_move(x+w-1,y); buf_appends("\xe2\x94\x90"); /* ┐ */
    /* bottom-left */ ui_move(x,y+h-1); buf_appends("\xe2\x94\x94"); /* └ */
    /* bottom-right */ ui_move(x+w-1,y+h-1); buf_appends("\xe2\x94\x98"); /* ┘ */
    /* top/bottom edges */ emit_sgr(); for(int i=1;i<w-1;i++){ui_move(x+i,y);buf_appends("\xe2\x94\x80");}
    for(int i=1;i<w-1;i++){ui_move(x+i,y+h-1);buf_appends("\xe2\x94\x80");}
    /* left/right edges */ for(int i=1;i<h-1;i++){ui_move(x,y+i);buf_appends("\xe2\x94\x82");}
    for(int i=1;i<h-1;i++){ui_move(x+w-1,y+i);buf_appends("\xe2\x94\x82");}
}

void ui_draw_rect_content(int x, int y, int w, int h) {
    if(w<2||h<2)return;
    ui_move(x,y);buf_appends("\xe2\x94\x8c"); /* ┌ */
    ui_move(x+w-1,y);buf_appends("\xe2\x94\x90"); /* ┐ */
    ui_move(x,y+h-1);buf_appends("\xe2\x94\x94"); /* └ */
    ui_move(x+w-1,y+h-1);buf_appends("\xe2\x94\x98"); /* ┘ */
    emit_sgr();
    for(int i=1;i<w-1;i++){ui_move(x+i,y);buf_appends("\xe2\x94\x80");}
    for(int i=1;i<w-1;i++){ui_move(x+i,y+h-1);buf_appends("\xe2\x94\x80");}
    for(int i=1;i<h-1;i++){ui_move(x,y+i);buf_appends("\xe2\x94\x82");}
    for(int i=1;i<h-1;i++){ui_move(x+w-1,y+i);buf_appends("\xe2\x94\x82");}
    for(int i=1;i<h-1;i++){ui_move(x+1,y+i);for(int j=0;j<w-2;j++)buf_appends(" ");}
}

void ui_fill_rect(int x, int y, int w, int h, char ch) { for(int i=0;i<h;i++)ui_draw_h_line(x,y+i,w,ch); }
void ui_hide_cursor(void){buf_appends("\x1b[?25l");}
void ui_show_cursor(int x, int y){ui_move(x,y);buf_appends("\x1b[?25h");}

void ui_message_box(const Theme *theme, const char *title, const char *msg) {
    int tw,th; ui_get_term_size(&tw,&th);
    int bw=(int)strlen(msg)+8; if(bw<(int)strlen(title)+8)bw=(int)strlen(title)+8;
    if(bw>tw-4)bw=tw-4; if(bw<20)bw=20; int bh=5,bx=(tw-bw)/2,by=(th-bh)/2;
    while(1){ ui_begin_frame();
        ui_set_bg(theme_get(theme,COLOR_BG));ui_clear_screen();ui_reset_colors();
        ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_FILE));
        ui_draw_rect_content(bx,by,bw,bh);
        ui_set_fg(theme_get(theme,COLOR_DIALOG_BORDER));ui_draw_rect(bx,by,bw,bh);
        ui_set_fg(theme_get(theme,COLOR_SELECTED_FG));ui_draw_text(bx+2,by+1,title);
        ui_set_fg(theme_get(theme,COLOR_FILE));ui_draw_text(bx+2,by+2,msg);
        ui_set_fg(theme_get(theme,COLOR_DIALOG_BORDER));
        { const char *ft="Press any key to continue..."; ui_draw_text(bx+(bw-(int)strlen(ft))/2,by+bh-2,ft); }
        ui_reset_colors(); ui_end_frame();
        KeyEvent ev; while(input_poll(&ev)){if(ev.code&&ev.code!=KEY_RESIZE)goto msg_done;} }
    msg_done:;
}

int ui_confirm_dialog(const Theme *theme, const char *title, const char *msg) {
    int tw,th; ui_get_term_size(&tw,&th);
    int bw=(int)strlen(msg)+8; if(bw<(int)strlen(title)+8)bw=(int)strlen(title)+8;
    if(bw>tw-4)bw=tw-4;if(bw<30)bw=30; int bh=6,bx=(tw-bw)/2,by=(th-bh)/2,selected=1;
    while(1){ ui_begin_frame();
        ui_set_bg(theme_get(theme,COLOR_BG));ui_clear_screen();ui_reset_colors();
        ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_FILE));
        ui_draw_rect_content(bx,by,bw,bh);
        ui_set_fg(theme_get(theme,COLOR_DIALOG_BORDER));ui_draw_rect(bx,by,bw,bh);
        ui_set_fg(theme_get(theme,COLOR_SELECTED_FG));ui_set_bold();ui_draw_text(bx+2,by+1,title);ui_reset_colors();
        ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_FILE));ui_draw_text(bx+2,by+2,msg);
        int yb=by+bh-2;
        if(selected){ ui_set_bg(theme_get(theme,COLOR_SELECTED_BG));ui_set_fg(theme_get(theme,COLOR_SELECTED_FG));ui_set_bold();
            ui_draw_text(bx+4,yb,"  Yes  ");ui_reset_colors();
            ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_FILE));ui_draw_text(bx+bw-12,yb,"  No   "); }
        else{ ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_FILE));ui_draw_text(bx+4,yb,"  Yes  ");
            ui_set_bg(theme_get(theme,COLOR_SELECTED_BG));ui_set_fg(theme_get(theme,COLOR_SELECTED_FG));ui_set_bold();
            ui_draw_text(bx+bw-12,yb,"  No   ");ui_reset_colors(); } ui_reset_colors(); ui_end_frame();
        KeyEvent ev; while(input_poll(&ev)){if(ev.code==KEY_RESIZE)continue;
            if(ev.code==KEY_LEFT||ev.code==KEY_RIGHT||ev.code==KEY_TAB){selected=!selected;break;}
            if(ev.code==KEY_ENTER)return selected; if(ev.code==KEY_ESC)return 0;
            if(ev.code==KEY_CHAR){if(ev.ch=='y'||ev.ch=='Y')return 1;if(ev.ch=='n'||ev.ch=='N')return 0;} break; } }
}

int ui_input_dialog(const Theme *theme, const char *title, char *buf, int buf_len) {
    int tw,th;ui_get_term_size(&tw,&th);int bw=50;if(bw>tw-4)bw=tw-4;int bh=5,bx=(tw-bw)/2,by=(th-bh)/2;
    buf[0]=0;int cursor=0,len=0;
    while(1){ ui_begin_frame();
        ui_set_bg(theme_get(theme,COLOR_BG));ui_clear_screen();ui_reset_colors();
        ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_FILE));
        ui_draw_rect_content(bx,by,bw,bh);ui_set_fg(theme_get(theme,COLOR_DIALOG_BORDER));ui_draw_rect(bx,by,bw,bh);
        ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_SELECTED_FG));ui_set_bold();
        ui_draw_text(bx+2,by+1,title);ui_reset_colors();
        ui_set_bg(theme_get(theme,COLOR_BG));ui_set_fg(theme_get(theme,COLOR_CMDLINE));
        ui_draw_h_line(bx+2,by+2,bw-4,' ');ui_move(bx+3,by+2);emit_sgr();buf_append(buf,len);
        if(cursor<len){ui_move(bx+3+cursor,by+2);ui_set_bg(theme_get(theme,COLOR_SELECTED_BG));
            ui_set_fg(theme_get(theme,COLOR_SELECTED_FG));emit_sgr();buf_append(&buf[cursor],1);} ui_reset_colors();
        ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_DIALOG_BORDER));
        { const char *ft="Enter=confirm  Esc=cancel";ui_draw_text(bx+(bw-(int)strlen(ft))/2,by+bh-2,ft); } ui_reset_colors(); ui_end_frame();
        KeyEvent ev; while(input_poll(&ev)){if(ev.code==KEY_RESIZE)continue;
            if(ev.code==KEY_ESC){buf[0]=0;return 0;}if(ev.code==KEY_ENTER)return 1;
            if(ev.code==KEY_LEFT&&cursor>0){cursor--;break;}if(ev.code==KEY_RIGHT&&cursor<len){cursor++;break;}
            if(ev.code==KEY_HOME){cursor=0;break;}if(ev.code==KEY_END){cursor=len;break;}
            if(ev.code==KEY_BACKSPACE&&cursor>0){memmove(buf+cursor-1,buf+cursor,len-cursor+1);cursor--;len--;break;}
            if(ev.code==KEY_DELETE&&cursor<len){memmove(buf+cursor,buf+cursor+1,len-cursor);len--;break;}
            if(ev.code==KEY_CHAR&&ev.ch>=32&&len<buf_len-1){memmove(buf+cursor+1,buf+cursor,len-cursor+1);
                buf[cursor]=ev.ch;cursor++;len++;break;}
            if(ev.code==KEY_SPACE&&len<buf_len-1){memmove(buf+cursor+1,buf+cursor,len-cursor+1);
                buf[cursor]=' ';cursor++;len++;break;} break; } }
}

void ui_draw_history(const Theme *theme, int tw, int th,
                     const void *bgtask_v, const void *hist_v, int hist_count, int show_current) {
    typedef struct { void*a,*b,*c,*d; int op_type,total_items; volatile int done_items,visible,active;
                     char current_file[260],title[64]; int64_t s,e; } Bt;
    typedef struct { int op_type,total,done; char desc[128]; int status; } Hr;
    const Bt *bt=(const Bt*)bgtask_v; const Hr *hist=(const Hr*)hist_v;
    int lines=hist_count+(show_current?3:0)+3;
    int max_lines=th-4; if(lines>max_lines)lines=max_lines;
    int bw=tw-8; if(bw<50)bw=tw-2; if(bw<30)bw=30; int bh=lines+2,bx=(tw-bw)/2,by=(th-bh)/2; if(by<0)by=0;
    ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_FILE));
    ui_fill_rect(bx,by,bw,bh,' ');ui_reset_colors();
    ui_set_fg(theme_get(theme,COLOR_FOCUS_BORDER));ui_draw_rect(bx,by,bw,bh);
    ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_SELECTED_FG));ui_set_bold();
    ui_draw_text(bx+2,by+1,"  Operation history");
    if(show_current) ui_draw_text(bx+bw-20,by+1," [running]"); ui_reset_colors();
    int row=by+2,inner_w=bw-4;
    const char *opnames[]={"?","Copy","Move","Delete"};
    if(show_current&&bt&&bt->active){ ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));
        ui_set_fg(theme_get(theme,COLOR_SELECTED_FG));ui_set_bold();
        char buf[320]; int pct=bt->total_items?(int)((int64_t)bt->done_items*100/bt->total_items):0;
        snprintf(buf,320," %s  %d/%d  %d%%  %s",
                 opnames[bt->op_type>=1&&bt->op_type<=3?bt->op_type:0],
                 bt->done_items,bt->total_items,pct,bt->current_file[0]?bt->current_file:"...");
        ui_draw_text_trunc(bx+2,row,inner_w,buf);ui_reset_colors();row++;
        ui_set_bg(theme_get(theme,COLOR_TAB_INACTIVE));ui_set_fg(theme_get(theme,COLOR_TAB_INACTIVE));
        ui_draw_h_line(bx+2,row,inner_w,'\xE2'[0]); /* shade */ ui_move(bx+2,row); buf_appends("\xe2\x96\x91");
        if(bt->total_items>0){ int fill=(int)(((int64_t)bt->done_items*(inner_w-2))/bt->total_items);
            if(fill<0)fill=0;if(fill>inner_w-2)fill=inner_w-2;
            ui_set_bg(theme_get(theme,COLOR_PROGRESS));ui_set_fg(theme_get(theme,COLOR_PROGRESS));
            for(int i=0;i<fill;i++){ui_move(bx+3+i,row);buf_appends("\xe2\x96\x88");} } ui_reset_colors();row++; }
    if(hist_count>0){ ui_set_fg(theme_get(theme,COLOR_PANEL_BORDER));
        ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));
        for(int i=0;i<inner_w;i++){ui_move(bx+2+i,row);buf_appends("\xe2\x94\x80");} ui_reset_colors();row++; }
    for(int i=0;i<hist_count&&row<by+bh-1;i++){ const Hr *h=&hist[i];
        char buf[256]; snprintf(buf,256,"   %-6s %d/%d  %s",
                 opnames[h->op_type>=1&&h->op_type<=3?h->op_type:0],h->done,h->total,
                 h->desc[0]?h->desc:"");
        ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_FILE));ui_set_dim();
        ui_draw_text(bx+2,row,buf); const char *st=h->status?"OK":"FAIL";
        uint32_t sc=h->status?theme_get(theme,COLOR_PROGRESS):theme_get(theme,COLOR_ERROR);
        ui_set_fg(sc);ui_set_bold();ui_draw_text(bx+inner_w-6,row,st);row++;ui_reset_colors(); }
    if(hist_count==0&&!show_current){ ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));
        ui_set_fg(theme_get(theme,COLOR_FILE));ui_set_dim();ui_draw_text(bx+4,row,"(no operations yet)");ui_reset_colors(); }
    ui_set_bg(theme_get(theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(theme,COLOR_DIALOG_BORDER));
    { const char *ft=" F3 / Esc = close "; ui_draw_text(bx+(bw-(int)strlen(ft))/2,by+bh-1,ft); } ui_reset_colors();
}
