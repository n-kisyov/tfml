#include "theme.h"
#include "json.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

void theme_set_default(Theme *t) {
    memset(t,0,sizeof(Theme));
    strcpy(t->name,"Default Dark");
    t->colors[COLOR_BG]=0x1E1E2E;
    t->colors[COLOR_PANEL_BORDER]=0x45475A;
    t->colors[COLOR_FOCUS_BORDER]=0x89B4FA;
    t->colors[COLOR_TAB_ACTIVE]=0x89B4FA;
    t->colors[COLOR_TAB_INACTIVE]=0x585B70;
    t->colors[COLOR_TAB_ACTIVE_BG]=0x313244;
    t->colors[COLOR_DIR]=0x89B4FA;
    t->colors[COLOR_FILE]=0xCDD6F4;
    t->colors[COLOR_SYMLINK]=0xA6E3A1;
    t->colors[COLOR_SELECTED_BG]=0x313244;
    t->colors[COLOR_SELECTED_FG]=0xCDD6F4;
    t->colors[COLOR_TAGGED]=0xF9E2AF;
    t->colors[COLOR_TAGGED_BG]=0x45475A;
    t->colors[COLOR_STATUS_BAR]=0x45475A;
    t->colors[COLOR_STATUS_BG]=0x45475A;
    t->colors[COLOR_STATUS_FG]=0xCDD6F4;
    t->colors[COLOR_CMDLINE]=0xCDD6F4;
    t->colors[COLOR_DIALOG_BORDER]=0xFAB387;
    t->colors[COLOR_DIALOG_BG]=0x313244;
    t->colors[COLOR_ERROR]=0xF38BA8;
    t->colors[COLOR_PROGRESS]=0xA6E3A1;
}

static const char *color_role_names[]={
    "bg","panel_border","focus_border","tab_active","tab_inactive","tab_active_bg",
    "dir","file","symlink","selected_bg","selected_fg","tagged","tagged_bg",
    "status_bar","status_bg","status_fg","cmdline","dialog_border","dialog_bg",
    "error","progress"
};

static uint32_t parse_hex_color(const char *str) {
    if(!str||strlen(str)<7||str[0]!='#') return 0xFFFFFF;
    return (uint32_t)strtol(str+1,NULL,16)&0xFFFFFF;
}

int theme_load(Theme *t, const char *path) {
    int fd=open(path,O_RDONLY);
    if(fd<0) return 0;
    struct stat st; fstat(fd,&st);
    if(st.st_size>65536){close(fd);return 0;}
    char *mb=(char*)malloc(st.st_size+1);
    if(!mb){close(fd);return 0;}
    ssize_t rd=read(fd,mb,st.st_size); close(fd);
    if(rd<=0){free(mb);return 0;}
    mb[rd]=0;
    JsonValue root=json_parse(mb); free(mb);
    if(root.type!=JSON_OBJECT) return 0;
    JsonValue *nv=json_get(&root,"name");
    if(nv&&nv->type==JSON_STRING) strncpy(t->name,nv->str_val,63);
    JsonValue *colors=json_get(&root,"colors");
    if(colors&&colors->type==JSON_OBJECT) {
        for(int i=0;i<COLOR_COUNT;i++) {
            JsonValue *cv=json_get(colors,color_role_names[i]);
            if(cv&&cv->type==JSON_STRING) t->colors[i]=parse_hex_color(cv->str_val);
        }
    }
    json_free(&root);
    return 1;
}

uint32_t theme_get(const Theme *t, ColorRole role) { return t->colors[role]; }
