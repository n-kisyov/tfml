#include "config.h"
#include "json.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

void config_set_defaults(Config *c) {
    memset(c,0,sizeof(Config));
    strcpy(c->theme_path,"themes/default.json");
    c->startup_tab_counts[0]=1; c->startup_tab_counts[1]=1;
    char *home=get_home_dir();
    for(int i=0;i<2;i++) strncpy(c->startup_dirs[i][0],home,CONFIG_MAX_PATH-1);
    c->panel_split_pct=50;
}

const char *config_get_path(void) {
    static char path[4096]={0};
    if(!path[0]) {
        char *d=get_config_dir();
        mkdir(d,0755);
        snprintf(path,4096,"%s/config.json",d);
    }
    return path;
}

int config_load(Config *c, const char *path) {
    config_set_defaults(c);
    int fd=open(path,O_RDONLY); if(fd<0) return 0;
    struct stat st; fstat(fd,&st);
    if(st.st_size>65536){close(fd);return 0;}
    char *mb=(char*)malloc(st.st_size+1);
    if(!mb){close(fd);return 0;}
    ssize_t rd=read(fd,mb,st.st_size); close(fd);
    if(rd<=0){free(mb);return 0;}
    mb[rd]=0;
    JsonValue root=json_parse(mb); free(mb);
    if(root.type!=JSON_OBJECT) return 0;
    const char *tp=json_get_str(&root,"theme",NULL);
    if(tp) strncpy(c->theme_path,tp,CONFIG_MAX_PATH-1);
    c->sort_by=(int)json_get_num(&root,"sort_by",0);
    c->sort_reverse=json_get_bool(&root,"sort_reverse",0);
    c->panel_split_pct=(int)json_get_num(&root,"panel_split_pct",50);
    if(c->panel_split_pct<20)c->panel_split_pct=20;
    if(c->panel_split_pct>80)c->panel_split_pct=80;
    JsonValue *panels=json_get(&root,"panels");
    if(panels&&panels->type==JSON_ARRAY) {
        for(int pi=0;pi<2&&pi<panels->arr.count;pi++) {
            JsonValue *pv=json_arr_get(panels,pi);
            if(!pv||pv->type!=JSON_OBJECT) continue;
            JsonValue *tabs=json_get(pv,"tabs");
            if(tabs&&tabs->type==JSON_ARRAY&&tabs->arr.count>0) {
                JsonValue *tv=json_arr_get(tabs,0);
                if(tv&&tv->type==JSON_STRING&&tv->str_val[0])
                    strncpy(c->startup_dirs[pi][0],tv->str_val,CONFIG_MAX_PATH-1);
            }
            c->startup_tab_counts[pi]=1;
            JsonValue *drives=json_get(pv,"drives");
            if(drives&&drives->type==JSON_ARRAY)
                for(int d=0;d<drives->arr.count&&d<26;d++) {
                    JsonValue *dv=json_arr_get(drives,d);
                    if(dv&&dv->type==JSON_STRING&&dv->str_val[0])
                        strncpy(c->drive_paths[pi][d],dv->str_val,CONFIG_MAX_PATH-1);
                }
        }
    }
    json_free(&root);
    return 1;
}

static int buf_puts(char *buf, int cap, int pos, const char *str) {
    while(*str&&pos<cap-1) buf[pos++]=*str++;
    buf[pos]=0; return pos;
}

static int buf_json_string(char *buf, int cap, int pos, const char *str) {
    if(pos>=cap-2) return pos;
    buf[pos++]='"';
    if(str) for(const char *s=str;*s&&pos<cap-3;s++) {
        if(pos>=cap-2) break;
        if(*s=='\\'){buf[pos++]='\\';buf[pos++]='\\';}
        else if(*s=='"'){buf[pos++]='\\';buf[pos++]='"';}
        else if(*s=='\r'){buf[pos++]='\\';buf[pos++]='r';}
        else if(*s=='\n'){buf[pos++]='\\';buf[pos++]='n';}
        else if(*s=='\t'){buf[pos++]='\\';buf[pos++]='t';}
        else buf[pos++]=*s;
    }
    if(pos<cap-1) buf[pos++]='"';
    buf[pos]=0; return pos;
}

int config_save(const Config *c, const char *path) {
    int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0) return 0;
    char *buf=(char*)calloc(65536,1);
    if(!buf){close(fd);return 0;}
    int cap=65536,pos=0;
    pos=buf_puts(buf,cap,pos,"{\n");
    pos=buf_puts(buf,cap,pos,"  \"theme\": ");
    pos=buf_json_string(buf,cap,pos,c->theme_path);
    pos=buf_puts(buf,cap,pos,",\n");
    pos=buf_puts(buf,cap,pos,"  \"sort_by\": 0,\n");
    pos=buf_puts(buf,cap,pos,"  \"sort_reverse\": false,\n");
    pos=buf_puts(buf,cap,pos,"  \"panel_split_pct\": 50,\n");
    pos=buf_puts(buf,cap,pos,"  \"panels\": [\n");
    for(int pi=0;pi<2;pi++) {
        pos=buf_puts(buf,cap,pos,"    {\n");
        pos=buf_puts(buf,cap,pos,"      \"tabs\": [\n        ");
        pos=buf_json_string(buf,cap,pos,c->startup_dirs[pi][0]);
        pos=buf_puts(buf,cap,pos,"\n      ],\n");
        pos=buf_puts(buf,cap,pos,"      \"drives\": [\n");
        for(int d=0;d<26;d++) {
            pos=buf_puts(buf,cap,pos,"        ");
            pos=buf_json_string(buf,cap,pos,c->drive_paths[pi][d][0]?c->drive_paths[pi][d]:"");
            pos=buf_puts(buf,cap,pos,d<25?",\n":"\n");
        }
        pos=buf_puts(buf,cap,pos,"      ]\n");
        pos=buf_puts(buf,cap,pos,pi<1?"    },\n":"    }\n");
    }
    pos=buf_puts(buf,cap,pos,"  ]\n}\n");
    ssize_t wr=write(fd,buf,pos); free(buf); close(fd);
    return wr==pos;
}
