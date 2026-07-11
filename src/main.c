#include "main.h"
#include "ui.h"
#include "input.h"
#include "ops.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

AppState g_app;

static int panel_h_for(int idx);

static void init_app(void) {
    memset(&g_app,0,sizeof(g_app));
    ui_init(); input_init();
    const char *cfg_path=config_get_path();
    config_set_defaults(&g_app.config);
    if(!config_load(&g_app.config,cfg_path)) config_save(&g_app.config,cfg_path);
    char theme_full[4096];
    if(g_app.config.theme_path[0]) {
        if(!theme_load(&g_app.theme,g_app.config.theme_path)) {
            const char *fn=strrchr(g_app.config.theme_path,'/');if(fn)fn++;else fn=g_app.config.theme_path;
            snprintf(theme_full,4096,"themes/%s",fn);
            if(!theme_load(&g_app.theme,theme_full)) theme_set_default(&g_app.theme);
        }
    } else theme_set_default(&g_app.theme);
    g_app.fs=&fs_local;
    for(int i=0;i<2;i++) {
        char *dir=g_app.config.startup_dirs[i][0];
        if(!dir||!dir[0]||!g_app.fs->exists(dir)||!g_app.fs->is_dir(dir)) dir=get_home_dir();
        panel_init(&g_app.panels[i],dir);
        g_app.panels[i].show_hidden=g_app.config.show_hidden;
        g_app.panels[i].sort_by=g_app.config.sort_by;
        g_app.panels[i].sort_reverse=g_app.config.sort_reverse;
        for(int d=0;d<26;d++) if(g_app.config.drive_paths[i][d][0]&&g_app.fs->exists(g_app.config.drive_paths[i][d])&&g_app.fs->is_dir(g_app.config.drive_paths[i][d])) strncpy(g_app.panels[i].drive_paths[d],g_app.config.drive_paths[i][d],4095);
        panel_refresh(&g_app.panels[i],g_app.fs);
    }
    cmdline_init(&g_app.cmdline);g_app.focus=FOCUS_LEFT;
    ui_get_term_size(&g_app.tw,&g_app.th);
    g_app.left_w=(g_app.tw*g_app.config.panel_split_pct)/100;
    if(g_app.left_w<20)g_app.left_w=20;if(g_app.left_w>g_app.tw-20)g_app.left_w=g_app.tw-20;
    g_app.running=1;g_app.needs_redraw=1;bgop_init(&g_app.bgtask);
}

static void shutdown_app(void) {
    g_app.running=0;
    for(int i=0;i<2;i++){ Panel *p=&g_app.panels[i];g_app.config.startup_tab_counts[i]=1;
        const char *sp=p->tabs[0].path;
        if(p->in_drive_list&&strcmp(sp,"Mounts")==0) sp=p->saved_path;
        strncpy(g_app.config.startup_dirs[i][0],sp,CONFIG_MAX_PATH-1);
        for(int d=0;d<26;d++) strncpy(g_app.config.drive_paths[i][d],p->drive_paths[d],CONFIG_MAX_PATH-1); }
    config_save(&g_app.config,config_get_path());
    bgop_free(&g_app.bgtask);
    for(int i=0;i<2;i++) panel_free(&g_app.panels[i]);
    cmdline_free(&g_app.cmdline);
    input_shutdown();ui_shutdown();
}

static void render(void) {
    int tw=g_app.tw,th=g_app.th,lw=g_app.left_w;
    int psy=1,ph=th-2;if(ph<6)ph=6;
    ui_begin_frame();ui_hide_cursor();
    ui_set_bg(theme_get(&g_app.theme,COLOR_BG));ui_clear_screen();ui_reset_colors();
    if(g_app.show_help) {
        static const char *help[]={
            "       Navigation                         File Ops",
            "       ---------                         --------",
            "       Arrows    Move cursor             F5   Copy to opposite panel",
            "       PgUp/Dn   Page up/down            F6   Move to opposite panel",
            "       Home/End  First / last entry      F7   Create directory",
            "       Enter     Enter dir / open file   F8   Delete (with confirm)",
            "       Backspace Parent directory        Space Toggle tag (multi-select)",
            "       Ctrl+D    Mount selector          F3   View / Re-show progress",
            "       Esc       Clear tags              F2   Refresh panel",
            "",
            "       Tabs (1 main + up to 4 extra)     Shell line",
            "       ----------------------------        ----------",
            "       Alt+Sh+N New tab (starts Home)    Arrows  Command history",
            "       Alt+Sh+B Cycle tabs               Enter   Execute command",
            "       Alt+Sh+M Close tab (main=locked)  Esc     Clear command line",
            "       Tab       Rotate focus             Backsp  Delete previous char",
            "",
            "                     F1 Help    F12 Exit", NULL };
        int hc=0,mw=0;while(help[hc]){int l=(int)strlen(help[hc]);if(l>mw)mw=l;hc++;}
        int bw=mw+6;if(bw>tw-2)bw=tw-2;int bh=hc+4,bx=(tw-bw)/2,by=(th-bh)/2;if(by<0)by=0;if(by+bh>=th)by=th-bh;
        ui_set_bg(theme_get(&g_app.theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(&g_app.theme,COLOR_FILE));
        ui_fill_rect(bx,by,bw,bh,' ');ui_reset_colors();
        ui_set_fg(theme_get(&g_app.theme,COLOR_FOCUS_BORDER));ui_draw_rect(bx,by,bw,bh);
        ui_set_bg(theme_get(&g_app.theme,COLOR_DIALOG_BG));ui_set_fg(theme_get(&g_app.theme,COLOR_SELECTED_FG));ui_set_bold();
        {const char *t=" tfm Keyboard Shortcuts ";ui_draw_text(bx+(bw-(int)strlen(t))/2,by+1,t);}ui_reset_colors();
        ui_set_bg(theme_get(&g_app.theme,COLOR_DIALOG_BG));
        for(int i=0;help[i];i++){int row=by+3+i;if(row>=by+bh-1)break;if(help[i][0]){ui_set_fg(theme_get(&g_app.theme,COLOR_FILE));ui_draw_text(bx+3,row,help[i]);}}
        ui_set_fg(theme_get(&g_app.theme,COLOR_DIALOG_BORDER));
        {const char *f=" F1 = Close ";ui_draw_text(bx+(bw-(int)strlen(f))/2,by+bh-2,f);}ui_reset_colors();
        ui_end_frame();return;
    }
    ui_set_bg(theme_get(&g_app.theme,COLOR_STATUS_BG));ui_set_fg(theme_get(&g_app.theme,COLOR_STATUS_FG));
    ui_fill_rect(0,0,tw,1,' ');
    int total_tagged=g_app.panels[0].tagged_count+g_app.panels[1].tagged_count;
    int bgop_r=bgop_is_active(&g_app.bgtask);char keybar[320];
    if(bgop_r){snprintf(keybar,320,total_tagged>0?" F2Rfrsh  F3Prog  F5Copy  F6Move  F8Del  F12Quit  Tag:%d":" F2Rfrsh  F3Prog  F5Copy  F6Move  F8Del  F12Quit",total_tagged);}
    else if(total_tagged>0)snprintf(keybar,320," F2Rfrsh  F3View  F5Copy  F6Move  F7Mkdir  F8Del  F12Quit  Tag:%d",total_tagged);
    else strcpy(keybar," F2Rfrsh  F3View  F5Copy  F6Move  F7Mkdir  F8Del  F12Quit");
    ui_draw_text_trunc(0,0,tw,keybar);
    if(bgop_r){const char *ons[]={"?","Copy","Move","Delete"};
        int op=(g_app.bgtask.op_type>=1&&g_app.bgtask.op_type<=3)?g_app.bgtask.op_type:0;
        const char *vis=g_app.bgtask.visible?"":" [hidden]";char bs[80];
        snprintf(bs,80," >%s %d/%d%s  ",ons[op],g_app.bgtask.done_items,g_app.bgtask.total_items,vis);
        int bl=(int)strlen(bs),bx2=tw-bl;if(bx2<40)bx2=40;
        ui_set_bg(theme_get(&g_app.theme,COLOR_PROGRESS));ui_set_fg(theme_get(&g_app.theme,COLOR_BG));ui_set_bold();
        ui_draw_text(bx2,0,bs);ui_reset_colors();}
    panel_render(&g_app.panels[0],&g_app.theme,0,psy,lw,ph,g_app.focus==FOCUS_LEFT);
    panel_render(&g_app.panels[1],&g_app.theme,lw,psy,tw-lw,ph,g_app.focus==FOCUS_RIGHT);
    if((bgop_is_active(&g_app.bgtask)&&g_app.bgtask.visible)||g_app.history_visible)
        ui_draw_history(&g_app.theme,tw,th,&g_app.bgtask,g_app.history,g_app.history_count,bgop_is_active(&g_app.bgtask)&&g_app.bgtask.visible);
    int cy=th-1;cmdline_render(&g_app.cmdline,&g_app.theme,0,cy,tw,g_app.focus==FOCUS_CMDLINE);
    if(g_app.focus==FOCUS_CMDLINE)ui_show_cursor(2+g_app.cmdline.cursor,cy);
    ui_reset_colors();ui_end_frame();
}

static void handle_panel_input(Panel *panel, int panel_idx, KeyEvent *ev) {
    if(ev->code==KEY_RESIZE){g_app.needs_redraw=1;return;}
    if(bgop_is_active(&g_app.bgtask)&&ev->code==KEY_ESC){g_app.bgtask.visible=0;g_app.history_visible=0;g_app.needs_redraw=1;return;}
    if(bgop_is_active(&g_app.bgtask)&&ev->code==KEY_F3){g_app.bgtask.visible=!g_app.bgtask.visible;g_app.needs_redraw=1;return;}
    if(ev->code==KEY_F3&&!bgop_is_active(&g_app.bgtask)){g_app.history_visible=!g_app.history_visible;g_app.needs_redraw=1;return;}
    if(g_app.cmdline.show_output){g_app.cmdline.show_output=0;free(g_app.cmdline.last_output);g_app.cmdline.last_output=NULL;g_app.needs_redraw=1;return;}
    if(panel->needs_refresh)panel_refresh(panel,g_app.fs);
    switch(ev->code){case KEY_F1:g_app.show_help=!g_app.show_help;g_app.needs_redraw=1;return;
        case KEY_ALT_SHIFT_N:panel_tab_new(panel);panel_refresh(panel,g_app.fs);g_app.needs_redraw=1;return;
        case KEY_ALT_SHIFT_M:panel_tab_close(panel);panel_refresh(panel,g_app.fs);g_app.needs_redraw=1;return;
        case KEY_ALT_SHIFT_B:panel_tab_next(panel);panel_refresh(panel,g_app.fs);g_app.needs_redraw=1;return;
        case KEY_CTRL_R:panel_refresh(panel,g_app.fs);g_app.needs_redraw=1;return;
        case KEY_CTRL_D:if(!g_app.panel_locked[panel_idx])panel_go_drives(panel,g_app.fs);g_app.needs_redraw=1;return;
        default:break;}
    switch(ev->code){case KEY_UP:panel_cursor_up(panel);g_app.needs_redraw=1;break;
        case KEY_DOWN:panel_cursor_down(panel);g_app.needs_redraw=1;break;
        case KEY_PAGEUP:panel_page_up(panel,panel_h_for(panel_idx));g_app.needs_redraw=1;break;
        case KEY_PAGEDOWN:panel_page_down(panel,panel_h_for(panel_idx));g_app.needs_redraw=1;break;
        case KEY_HOME:panel_cursor_home(panel);g_app.needs_redraw=1;break;
        case KEY_END:panel_cursor_end(panel);g_app.needs_redraw=1;break;
        case KEY_ENTER:if(g_app.panel_locked[panel_idx])break;
            if(panel->in_drive_list)panel_enter_on_drive(panel,g_app.fs);
            else if(panel->entry_count>0){PanelTab *tab=&panel->tabs[panel->active_tab];
                FileEntry *e=&panel->entries[tab->cursor];
                if(e->type==ENTRY_DIR)panel_enter_dir(panel,g_app.fs);
                else{char *full=path_join(tab->path,e->name);
                    pid_t p=fork();if(p==0){execlp("xdg-open","xdg-open",full,NULL);_exit(1);}
                    if(p>0){int st;waitpid(p,&st,0);} free(full);} } g_app.needs_redraw=1;break;
        case KEY_BACKSPACE:if(!g_app.panel_locked[panel_idx])panel_go_parent(panel,g_app.fs);g_app.needs_redraw=1;break;
        case KEY_ESC:if(panel->in_drive_list)panel_exit_drives(panel,g_app.fs);else panel_clear_tags(panel);g_app.needs_redraw=1;break;
        case KEY_SPACE:panel_toggle_tag(panel);g_app.needs_redraw=1;break;
        case KEY_F2:panel_refresh(panel,g_app.fs);g_app.needs_redraw=1;break;
        case KEY_F5:{if(bgop_is_active(&g_app.bgtask))break;Panel *other=(panel_idx==0)?&g_app.panels[1]:&g_app.panels[0];
            FileEntry *entries;int count;if(panel_tagged_or_current(panel,&entries,&count)){BgTask *bg=&g_app.bgtask;bgop_lock(bg);
            bg->path_count=0;for(int i=0;i<count&&i<4096;i++)bg->paths[bg->path_count++]=path_join(panel_current_path(panel),entries[i].name);
            strncpy(bg->dest_dir,panel_current_path(other),BGOP_PATH_MAX-1);bg->panel_src_idx=panel_idx;bg->panel_dst_idx=(panel_idx==0)?1:0;
            bg->fs_provider=(void*)g_app.fs;bgop_unlock(bg);free(entries);panel_clear_tags(panel);bgop_start_copy(bg);g_app.panel_locked[panel_idx]=1;}
            g_app.needs_redraw=1;break;}
        case KEY_F6:{if(bgop_is_active(&g_app.bgtask))break;Panel *other=(panel_idx==0)?&g_app.panels[1]:&g_app.panels[0];
            FileEntry *entries;int count;if(panel_tagged_or_current(panel,&entries,&count)){BgTask *bg=&g_app.bgtask;bgop_lock(bg);
            bg->path_count=0;for(int i=0;i<count&&i<4096;i++)bg->paths[bg->path_count++]=path_join(panel_current_path(panel),entries[i].name);
            strncpy(bg->dest_dir,panel_current_path(other),BGOP_PATH_MAX-1);bg->panel_src_idx=panel_idx;bg->panel_dst_idx=(panel_idx==0)?1:0;
            bg->fs_provider=(void*)g_app.fs;bgop_unlock(bg);free(entries);panel_clear_tags(panel);bgop_start_move(bg);g_app.panel_locked[panel_idx]=1;}
            g_app.needs_redraw=1;break;}
        case KEY_F7:{char nn[256]={0};ops_mkdir_dialog(panel_current_path(panel),&g_app.theme,g_app.fs,nn,256);panel_refresh(panel,g_app.fs);g_app.needs_redraw=1;break;}
        case KEY_F8:{if(bgop_is_active(&g_app.bgtask))break;FileEntry *entries;int count;
            if(panel_tagged_or_current(panel,&entries,&count)){char msg[512];snprintf(msg,512,"Delete %d item(s)?",count);
            if(ui_confirm_dialog(&g_app.theme,"Delete",msg)){BgTask *bg=&g_app.bgtask;bgop_lock(bg);bg->path_count=0;
            for(int i=0;i<count&&i<4096;i++)bg->paths[bg->path_count++]=path_join(panel_current_path(panel),entries[i].name);
            bg->dest_dir[0]=0;bg->panel_src_idx=panel_idx;bg->panel_dst_idx=-1;bg->fs_provider=(void*)g_app.fs;bgop_unlock(bg);
            panel_clear_tags(panel);bgop_start_delete(bg);g_app.panel_locked[panel_idx]=1;}free(entries);}g_app.needs_redraw=1;break;}
        case KEY_F12:g_app.running=0;break;
        case KEY_TAB:g_app.focus=(panel_idx==0)?FOCUS_RIGHT:FOCUS_CMDLINE;g_app.needs_redraw=1;break;
        case KEY_CTRL_C:panel_clear_tags(panel);g_app.needs_redraw=1;break; default:break;}
}

static void handle_cmdline_input(KeyEvent *ev) {
    if(ev->code==KEY_RESIZE){g_app.needs_redraw=1;return;}
    if(g_app.cmdline.show_output){g_app.cmdline.show_output=0;free(g_app.cmdline.last_output);g_app.cmdline.last_output=NULL;g_app.needs_redraw=1;return;}
    if(bgop_is_active(&g_app.bgtask)&&ev->code==KEY_ESC){g_app.bgtask.visible=0;g_app.history_visible=0;g_app.needs_redraw=1;return;}
    if(bgop_is_active(&g_app.bgtask)&&ev->code==KEY_F3){g_app.bgtask.visible=!g_app.bgtask.visible;g_app.needs_redraw=1;return;}
    if(ev->code==KEY_F3&&!bgop_is_active(&g_app.bgtask)){g_app.history_visible=!g_app.history_visible;g_app.needs_redraw=1;return;}
    switch(ev->code){case KEY_F1:g_app.show_help=!g_app.show_help;g_app.needs_redraw=1;break;
        case KEY_ENTER:cmdline_execute(&g_app.cmdline);g_app.needs_redraw=1;break;
        case KEY_ESC:cmdline_clear(&g_app.cmdline);g_app.needs_redraw=1;break;
        case KEY_TAB:g_app.focus=FOCUS_LEFT;g_app.needs_redraw=1;break;
        case KEY_F12:g_app.running=0;break;
        case KEY_BACKSPACE:cmdline_backspace(&g_app.cmdline);g_app.needs_redraw=1;break;
        case KEY_DELETE:cmdline_delete(&g_app.cmdline);g_app.needs_redraw=1;break;
        case KEY_LEFT:cmdline_cursor_left(&g_app.cmdline);g_app.needs_redraw=1;break;
        case KEY_RIGHT:cmdline_cursor_right(&g_app.cmdline);g_app.needs_redraw=1;break;
        case KEY_HOME:cmdline_cursor_home(&g_app.cmdline);g_app.needs_redraw=1;break;
        case KEY_END:cmdline_cursor_end(&g_app.cmdline);g_app.needs_redraw=1;break;
        case KEY_UP:cmdline_history_prev(&g_app.cmdline);g_app.needs_redraw=1;break;
        case KEY_DOWN:cmdline_history_next(&g_app.cmdline);g_app.needs_redraw=1;break;
        case KEY_CHAR:if(ev->ch>=32){cmdline_insert(&g_app.cmdline,ev->ch);g_app.needs_redraw=1;}break;
        case KEY_SPACE:cmdline_insert(&g_app.cmdline,' ');g_app.needs_redraw=1;break; default:break;}
}

static int panel_h_for(int idx) { (void)idx;int ph=g_app.th-2;if(ph<6)ph=6;return ph-5; }

int main(void) {
    init_app();
    while(g_app.running) {
        int ow=g_app.tw,oh=g_app.th;ui_get_term_size(&g_app.tw,&g_app.th);
        if(g_app.tw!=ow||g_app.th!=oh){g_app.left_w=(g_app.tw*g_app.config.panel_split_pct)/100;
        if(g_app.left_w<20)g_app.left_w=20;if(g_app.left_w>g_app.tw-20)g_app.left_w=g_app.tw-20;g_app.needs_redraw=1;}
        if(__atomic_load_n(&g_app.bgtask.finished,__ATOMIC_SEQ_CST)) {
            int si=g_app.bgtask.panel_src_idx,di=g_app.bgtask.panel_dst_idx;
            Panel *src=&g_app.panels[si],*dst=(di>=0)?&g_app.panels[di]:NULL;
            int ok=(g_app.bgtask.done_items==g_app.bgtask.total_items)?1:0;
            bgop_history_push(g_app.history,&g_app.history_count,BGOP_HISTORY,g_app.bgtask.op_type,g_app.bgtask.total_items,g_app.bgtask.done_items,g_app.bgtask.title,ok);
            g_app.panel_locked[si]=0;panel_refresh(src,g_app.fs);if(dst)panel_refresh(dst,g_app.fs);
            bgop_free(&g_app.bgtask);bgop_init(&g_app.bgtask);g_app.needs_redraw=1;}
        if(bgop_is_active(&g_app.bgtask))g_app.needs_redraw=1;
        if(g_app.needs_redraw){render();g_app.needs_redraw=0;}
        KeyEvent ev;int gi=bgop_is_active(&g_app.bgtask)?input_poll_timeout(&ev,100):input_poll(&ev);
        if(gi){switch(ev.code){case KEY_F12:g_app.running=0;break;case KEY_RESIZE:g_app.needs_redraw=1;break;
            default:if(g_app.focus==FOCUS_LEFT)handle_panel_input(&g_app.panels[0],0,&ev);
            else if(g_app.focus==FOCUS_RIGHT)handle_panel_input(&g_app.panels[1],1,&ev);
            else handle_cmdline_input(&ev);break;}}
    }
    shutdown_app();return 0;
}
