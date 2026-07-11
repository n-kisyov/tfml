#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>
#include <sys/stat.h>

char *strdup_safe(const char *src) {
    if (!src) return NULL;
    char *d = strdup(src);
    if (!d) abort();
    return d;
}

void *xcalloc(size_t n, size_t sz) { void *p=calloc(n,sz); if(!p)abort(); return p; }
void *xrealloc(void *p, size_t sz) { void *np=realloc(p,sz); if(!np)abort(); return np; }

int str_ends_with(const char *str, const char *suffix) {
    size_t ls=strlen(str), lf=strlen(suffix);
    if(lf>ls) return 0;
    return strcasecmp(str+ls-lf,suffix)==0;
}

char *path_join(const char *dir, const char *name) {
    size_t dl=strlen(dir), nl=strlen(name);
    int need_sep=(dl>0&&dir[dl-1]!='/');
    char *r=(char*)malloc(dl+nl+3);
    if(!r) return NULL;
    strcpy(r,dir);
    if(need_sep) r[dl++]='/';
    strcpy(r+dl,name);
    return r;
}

char *get_parent_path(const char *path) {
    if(!path||!*path) return strdup_safe("/");
    size_t len=strlen(path);
    while(len>0&&path[len-1]=='/') { len--; if(len==1&&path[0]=='/') break; }
    if(len==1&&path[0]=='/') return strdup_safe("/");
    for(size_t i=len;i>0;i--) {
        if(path[i-1]=='/') {
            if(i==1) return strdup_safe("/");
            char *r=(char*)malloc(i+1);
            memcpy(r,path,i-1); r[i-1]=0; return r;
        }
    }
    return strdup_safe("/");
}

int is_root_path(const char *path) { return path&&strcmp(path,"/")==0; }

const char *get_file_ext(const char *name) {
    const char *dot=strrchr(name,'.'); return dot?dot+1:"";
}

static int natural_compare_part(const char **pa, const char **pb) {
    const char *a=*pa,*b=*pb;
    while(*a=='0')a++;
    while(*b=='0')b++;
    int diff=0;
    while(1) {
        int da=(a[0]>='0'&&a[0]<='9'), db=(b[0]>='0'&&b[0]<='9');
        if(!da&&!db) break;
        if(!da){*pa=a;*pb=b;return -1;} if(!db){*pa=a;*pb=b;return 1;}
        if(diff==0)diff=(int)a[0]-(int)b[0];
        a++;b++;
    }
    *pa=a;*pb=b;
    return diff?diff:(int)(a-*pa)-(int)(b-*pb);
}

int str_compare_natural(const char *a, const char *b) {
    while(*a&&*b) {
        if((*a>='0'&&*a<='9')&&(*b>='0'&&*b<='9')) { int r=natural_compare_part(&a,&b); if(r)return r; }
        else { int wa=toupper(*a),wb=toupper(*b); if(wa!=wb)return wa-wb; a++;b++; }
    }
    return (int)(*a)-(*b);
}

char *get_home_dir(void) {
    static char buf[4096]={0};
    if(buf[0]) return buf;
    const char *h=getenv("HOME");
    if(h){strncpy(buf,h,4095);return buf;}
    struct passwd *pw=getpwuid(getuid());
    if(pw){strncpy(buf,pw->pw_dir,4095);return buf;}
    strcpy(buf,"/"); return buf;
}

char *get_config_dir(void) {
    static char buf[4096]={0};
    if(buf[0]) return buf;
    const char *xdg=getenv("XDG_CONFIG_HOME");
    if(xdg) snprintf(buf,4096,"%s/tfm",xdg);
    else snprintf(buf,4096,"%s/.config/tfm",get_home_dir());
    mkdir(buf,0755);
    return buf;
}

char *get_themes_dir(void) {
    static char buf[4104]={0};
    if(buf[0]) return buf;
    snprintf(buf,sizeof(buf),"%s/themes",get_config_dir());
    mkdir(buf,0755);
    return buf;
}

char *format_file_size(uint64_t bytes, char *buf, size_t buf_size) {
    const char *units[]={"B","KB","MB","GB","TB"};
    int ui=0; double size=(double)bytes;
    while(size>=1024.0&&ui<4){size/=1024.0;ui++;}
    if(ui==0) snprintf(buf,buf_size,"%llu %s",(unsigned long long)bytes,units[ui]);
    else snprintf(buf,buf_size,"%.1f %s",size,units[ui]);
    return buf;
}

char *format_file_time(const FileTime *ft, char *buf, size_t buf_size) {
    struct tm tm;
    time_t t = ft->tv_sec;
    localtime_r(&t,&tm);
    strftime(buf,buf_size,"%x %H:%M",&tm);
    return buf;
}
