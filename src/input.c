#include "input.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <signal.h>

static struct termios g_old_termios;
static int g_initialized=0;
static volatile int g_resized=0;
static int g_lastW=80,g_lastH=24;

static void sigwinch_handler(int sig) { (void)sig; g_resized=1; }

int input_init(void) {
    if(tcgetattr(STDIN_FILENO,&g_old_termios)!=0) return -1;
    struct termios raw=g_old_termios;
    cfmakeraw(&raw);
    raw.c_cc[VMIN]=0;
    raw.c_cc[VTIME]=0;
    if(tcsetattr(STDIN_FILENO,TCSANOW,&raw)!=0) return -1;
    fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK);
    struct sigaction sa={.sa_handler=sigwinch_handler};
    sigaction(SIGWINCH,&sa,NULL);
    struct winsize ws;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0) { g_lastW=ws.ws_col; g_lastH=ws.ws_row; }
    g_initialized=1; return 0;
}

void input_shutdown(void) {
    if(g_initialized) tcsetattr(STDIN_FILENO,TCSANOW,&g_old_termios);
    g_initialized=0;
}

static int parse_escape(KeyEvent *ev) {
    char seq[32]; int sl=0;
    struct pollfd pfd={.fd=STDIN_FILENO,.events=POLLIN};
    struct timespec start,now;
    clock_gettime(CLOCK_MONOTONIC,&start);
    while(sl<31) {
        if(poll(&pfd,1,5)<=0) break;
        char c; if(read(STDIN_FILENO,&c,1)!=1) break;
        seq[sl++]=c;
        clock_gettime(CLOCK_MONOTONIC,&now);
        if((now.tv_sec-start.tv_sec)*1000+(now.tv_nsec-start.tv_nsec)/1000000>50) break;
    }
    seq[sl]=0;

    if(sl==1) {
        if(seq[0]=='O'||seq[0]=='['||(seq[0]>='0'&&seq[0]<='9')) { /* need more? timeout. retry later with more reads */ }
        /* Alt+key sequence: ESC followed by a normal key */
        if(seq[0]>='a'&&seq[0]<='z') {
            switch(seq[0]) {
                case 'n': ev->code=KEY_ALT_SHIFT_N; return 1; /* Alt+Shift+n */
                case 'b': ev->code=KEY_ALT_SHIFT_B; return 1;
                case 'm': ev->code=KEY_ALT_SHIFT_M; return 1;
            }
        }
        if(seq[0]=='N') { ev->code=KEY_ALT_SHIFT_N; return 1; }
        if(seq[0]=='B') { ev->code=KEY_ALT_SHIFT_B; return 1; }
        if(seq[0]=='M') { ev->code=KEY_ALT_SHIFT_M; return 1; }
        return 0;
    }

    if(sl>=2&&seq[0]=='[') {
        /* ANSI escape sequences */
        switch(seq[1]) {
            case 'A': ev->code=KEY_UP; return 1;
            case 'B': ev->code=KEY_DOWN; return 1;
            case 'C': ev->code=KEY_RIGHT; return 1;
            case 'D': ev->code=KEY_LEFT; return 1;
            case 'H': ev->code=KEY_HOME; return 1;
            case 'F': ev->code=KEY_END; return 1;
            case 'Z': ev->code=KEY_TAB; return 1; /* Shift+Tab */
            case '1': {
                if(sl>=3) {
                    if(seq[2]=='~') { ev->code=KEY_HOME; return 1; }
                    if(seq[2]==';') {
                        if(sl>=5&&seq[4]=='~') {
                            if(seq[3]=='2') { ev->code=KEY_ALT_SHIFT_B; return 1; }
                            if(seq[3]=='5') { ev->code=KEY_CTRL_TAB; return 1; } /* Ctrl+Tab */
                        }
                    }
                    if(sl>=4&&seq[2]=='5'&&seq[3]=='~') { ev->code=KEY_F5; return 1; }
                    if(sl>=4&&seq[2]=='7'&&seq[3]=='~') { ev->code=KEY_F6; return 1; }
                    if(sl>=4&&seq[2]=='8'&&seq[3]=='~') { ev->code=KEY_F7; return 1; }
                    if(sl>=4&&seq[2]=='9'&&seq[3]=='~') { ev->code=KEY_F8; return 1; }
                }
                return 0;
            }
            case '2': {
                if(sl>=3&&seq[2]=='~') { ev->code=KEY_INSERT; return 1; }
                if(sl>=4&&seq[2]=='0'&&seq[3]=='~') { ev->code=KEY_F9; return 1; }
                if(sl>=4&&seq[2]=='1'&&seq[3]=='~') { ev->code=KEY_F10; return 1; }
                if(sl>=4&&seq[2]=='3'&&seq[3]=='~') { ev->code=KEY_F11; return 1; }
                if(sl>=4&&seq[2]=='4'&&seq[3]=='~') { ev->code=KEY_F12; return 1; }
                return 0;
            }
            case '3': if(sl>=3&&seq[2]=='~'){ev->code=KEY_DELETE;return 1;} return 0;
            case '5': if(sl>=3&&seq[2]=='~'){ev->code=KEY_PAGEUP;return 1;} return 0;
            case '6': if(sl>=3&&seq[2]=='~'){ev->code=KEY_PAGEDOWN;return 1;} return 0;
        }
    }
    if(sl>=2&&seq[0]=='O') {
        /* SCO-style function keys */
        switch(seq[1]) {
            case 'P': ev->code=KEY_F1; return 1;
            case 'Q': ev->code=KEY_F2; return 1;
            case 'R': ev->code=KEY_F3; return 1;
            case 'S': ev->code=KEY_F4; return 1;
        }
    }
    return 0;
}

int input_poll(KeyEvent *ev) {
    if(!g_initialized) return 0;
    memset(ev,0,sizeof(KeyEvent));
    if(g_resized) { g_resized=0;
        struct winsize ws;
        if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0){g_lastW=ws.ws_col;g_lastH=ws.ws_row;}
        ev->code=KEY_RESIZE; return 1; }
    struct winsize ws;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0&&(ws.ws_col!=g_lastW||ws.ws_row!=g_lastH)) {
        g_lastW=ws.ws_col; g_lastH=ws.ws_row; ev->code=KEY_RESIZE; return 1; }
    struct pollfd pfd={.fd=STDIN_FILENO,.events=POLLIN};
    if(poll(&pfd,1,-1)<=0) return 0;
    char c;
    ssize_t n=read(STDIN_FILENO,&c,1);
    if(n!=1) return 0;

    if(g_resized) { g_resized=0; ev->code=KEY_RESIZE; return 1; }

    if(c=='\x1b') { /* Escape or ANSI escape sequence */
        struct pollfd pfd2={.fd=STDIN_FILENO,.events=POLLIN};
        if(poll(&pfd2,1,10)>0) {
            if(parse_escape(ev)) return 1;
        }
        ev->code=KEY_ESC; return 1;
    }
    if(c=='\t') { ev->code=KEY_TAB; return 1; }
    if(c=='\r'||c=='\n') { ev->code=KEY_ENTER; return 1; }
    if(c==0x7f) { ev->code=KEY_BACKSPACE; return 1; }
    if(c==' ') { ev->code=KEY_SPACE; return 1; }

    /* Ctrl+key combinations */
    if((c>=1&&c<=26)) {
        if(c==4) { ev->code=KEY_CTRL_D; return 1; }
        if(c==3) { ev->code=KEY_CTRL_C; return 1; }
        if(c==18) { ev->code=KEY_CTRL_R; return 1; }
        if(c==20) { ev->code=KEY_CTRL_T; return 1; }
        if(c==23) { ev->code=KEY_CTRL_W; return 1; }
        return 0;
    }

    ev->code=KEY_CHAR; ev->ch=c; return 1;
}

int input_poll_timeout(KeyEvent *ev, unsigned int timeout_ms) {
    if(!g_initialized) return 0;
    memset(ev,0,sizeof(KeyEvent));
    if(g_resized) { g_resized=0;
        struct winsize ws; ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws);
        g_lastW=ws.ws_col;g_lastH=ws.ws_row; ev->code=KEY_RESIZE; return 1; }
    struct winsize ws;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==0&&(ws.ws_col!=g_lastW||ws.ws_row!=g_lastH)) {
        g_lastW=ws.ws_col;g_lastH=ws.ws_row;ev->code=KEY_RESIZE;return 1;}
    struct pollfd pfd={.fd=STDIN_FILENO,.events=POLLIN};
    if(poll(&pfd,1,(int)timeout_ms)<=0) return 0;
    return input_poll(ev);
}
