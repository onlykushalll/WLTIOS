/**
 * @file filemanager.c
 * @brief Wayland file manager — two-panel directory browser
 * @module wlt-filemanager
 *
 * Window: 900x600, dark theme
 * Left panel (250px): directory tree
 * Right panel (650px): file list with name, size
 * Top bar (30px): current path
 *
 * Navigation: Up/Down=select, Enter=open, Backspace=up, q=quit
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <wayland-client.h>
#include "file_dnd.h"
#include <wayland-client-protocol.h>
#include "xdg-shell-client-protocol.h"

#define WIN_W 900
#define WIN_H 600
#define TOP_BAR_H 30
#define LEFT_W 250
#define MAX_ENTRIES 64
#define MAX_NAME 256
#define MAX_PATH 512

static struct wl_display *display=NULL;
static struct wl_registry *registry=NULL;
static struct wl_compositor *compositor=NULL;
static struct wl_shm *shm=NULL;
static struct xdg_wm_base *xdg_wm_base=NULL;
static struct wl_seat *seat=NULL;
static struct wl_keyboard *keyboard=NULL;
static struct wl_surface *surface=NULL;
static struct xdg_surface *xdg_surface=NULL;
static struct xdg_toplevel *toplevel=NULL;
static struct wl_buffer *buffer=NULL;
static void *shm_data=NULL;
static int running=1;

typedef struct { char name[MAX_NAME]; int is_dir; unsigned long size; } entry_t;
static entry_t entries[MAX_ENTRIES];
static int entry_count=0, selected=0;
static char current_path[MAX_PATH]="/";

static const uint8_t font[][7]={
    [' ']={0,0,0,0,0,0,0},['!']={4,4,4,4,0,0,4},
    ['-']={0,0,0,0x1F,0,0,0},['.']={0,0,0,0,0,0x0C,0x0C},
    ['/']={1,2,2,4,8,8,0x10},
    ['0']={0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    ['1']={4,0x0C,4,4,4,4,0x0E},
    ['2']={0x0E,0x11,1,6,8,0x10,0x1F},
    ['3']={0x1F,1,2,6,1,0x11,0x0E},
    ['4']={2,6,0x0A,0x12,0x1F,2,2},
    ['5']={0x1F,0x10,0x1E,1,1,0x11,0x0E},
    ['6']={6,8,0x10,0x1E,0x11,0x11,0x0E},
    ['7']={0x1F,1,2,4,8,8,8},
    ['8']={0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    ['9']={0x0E,0x11,0x11,0x0F,1,2,0x0C},
    [':']={0,4,0,0,0,4,0},
    ['a']={0,0,0x0E,1,0x0F,0x11,0x0F},
    ['b']={0x10,0x10,0x1E,0x11,0x11,0x11,0x1E},
    ['c']={0,0,0x0E,0x10,0x10,0x10,0x0E},
    ['d']={1,1,0x0F,0x11,0x11,0x11,0x0F},
    ['e']={0,0,0x0E,0x11,0x1F,0x10,0x0E},
    ['f']={3,4,0x1F,4,4,4,4},
    ['g']={0,0x0F,0x11,0x11,0x0F,1,0x0E},
    ['h']={0x10,0x10,0x1E,0x11,0x11,0x11,0x11},
    ['i']={4,0,0x0C,4,4,4,0x0E},
    ['j']={1,0,3,1,1,0x11,0x0E},
    ['k']={0x10,0x10,0x12,0x14,0x18,0x12,0x11},
    ['l']={0x0C,4,4,4,4,4,0x0E},
    ['m']={0,0,0x1A,0x15,0x15,0x11,0x11},
    ['n']={0,0,0x1E,0x11,0x11,0x11,0x11},
    ['o']={0,0,0x0E,0x11,0x11,0x11,0x0E},
    ['p']={0,0,0x1E,0x11,0x1E,0x10,0x10},
    ['q']={0,0,0x0F,0x11,0x0F,1,1},
    ['r']={0,0,0x1E,0x11,0x10,0x10,0x10},
    ['s']={0,0,0x0F,0x10,0x0E,1,0x1E},
    ['t']={8,8,0x1C,8,8,9,6},
    ['u']={0,0,0x11,0x11,0x11,0x11,0x0F},
    ['v']={0,0,0x11,0x11,0x11,0x0A,4},
    ['w']={0,0,0x11,0x11,0x15,0x15,0x1A},
    ['x']={0,0,0x11,0x0A,4,0x0A,0x11},
    ['y']={0,0,0x11,0x11,0x0F,1,0x0E},
    ['z']={0,0,0x1F,2,4,8,0x1F},
};

#define CW 8
#define CH 14

static void dc(int px,int py,char c,uint32_t*p,uint32_t col)
{
    if(c<32||c>127)c='?';
    const uint8_t*g=font[(int)c];
    if(!g)g=font[(int)'?'];
    for(int gy=0;gy<7;gy++)for(int gx=0;gx<5;gx++)
        if(g[gy]&(0x10>>gx))for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
            int x=px+gx*2+dx,y=py+gy*2+dy;
            if(x>=0&&x<WIN_W&&y>=0&&y<WIN_H)p[y*WIN_W+x]=col;}
}

static void dt(int px,int py,const char*s,uint32_t*p,uint32_t col)
{
    int x=px;
    for(const char*c=s;*c;c++){
        if(*c=='\n'){x=px;py+=CH;continue;}
        dc(x,py,*c,p,col);x+=CW;}
}

static void fr(uint32_t*p,int x,int y,int w,int h,uint32_t c)
{
    for(int py=y;py<y+h&&py<WIN_H;py++)
        for(int px=x;px<x+w&&px<WIN_W;px++)
            if(px>=0&&py>=0)p[py*WIN_W+px]=c;
}

static void scan_dir(const char*path)
{
    entry_count=0;selected=0;
    DIR*d=opendir(path);
    if(!d)return;
    struct dirent*ent;
    while((ent=readdir(d))&&entry_count<MAX_ENTRIES){
        if(ent->d_name[0]=='.'&&ent->d_name[1]!='.'&&ent->d_name[1])continue;
        strncpy(entries[entry_count].name,ent->d_name,MAX_NAME-1);
        entries[entry_count].name[MAX_NAME-1]=0;
        char full[MAX_PATH];
        snprintf(full,sizeof(full),"%s/%s",path,ent->d_name);
        struct stat st;
        if(stat(full,&st)==0){entries[entry_count].is_dir=S_ISDIR(st.st_mode);entries[entry_count].size=st.st_size;}
        else{entries[entry_count].is_dir=0;entries[entry_count].size=0;}
        entry_count++;}
    closedir(d);
}

static void render(void)
{
    uint32_t*p=(uint32_t*)shm_data;
    fr(p,0,0,WIN_W,WIN_H,0xFF2E2E1A);
    fr(p,0,0,WIN_W,TOP_BAR_H,0xFF3E3E2A);
    dt(8,8,current_path,p,0xFFFFFFFF);
    fr(p,0,TOP_BAR_H,LEFT_W,WIN_H-TOP_BAR_H,0xFF2A2A3E);
    dt(8,TOP_BAR_H+8,"Dirs:",p,0xFF888888);
    fr(p,LEFT_W,TOP_BAR_H,WIN_W-LEFT_W,WIN_H-TOP_BAR_H,0xFF1A1A2E);

    int y=TOP_BAR_H+8;
    for(int i=0;i<entry_count&&y<WIN_H-CH;i++){
        if(i==selected)fr(p,LEFT_W,y-2,WIN_W-LEFT_W,CH+2,0xFF660044);
        const char*pre=entries[i].is_dir?"[D] ":"[F] ";
        uint32_t col=entries[i].is_dir?0xFF00D4FF:0xFF00FF88;
        dt(LEFT_W+8,y,pre,p,col);
        dt(LEFT_W+8+5*CW,y,entries[i].name,p,col);
        if(!entries[i].is_dir&&entries[i].size>0){
            char sz[32];
            if(entries[i].size<1024)snprintf(sz,sizeof(sz),"%lu B",entries[i].size);
            else if(entries[i].size<1048576)snprintf(sz,sizeof(sz),"%lu K",entries[i].size/1024);
            else snprintf(sz,sizeof(sz),"%lu M",entries[i].size/1048576);
            dt(WIN_W-8*CW,y,sz,p,0xFF888888);}
        if(entries[i].is_dir&&i<20)
            dt(8,TOP_BAR_H+24+i*CH,entries[i].name,p,0xFF00D4FF);
        y+=CH+2;}

    fr(p,0,WIN_H-20,WIN_W,20,0xFF3E3E2A);
    dt(8,WIN_H-16,"Up/Dn:select Enter:open BS:up m:mark p:paste q:quit",p,0xFF888888);
    wl_surface_attach(surface,buffer,0,0);
    wl_surface_damage(surface,0,0,WIN_W,WIN_H);
    wl_surface_commit(surface);
}

static void nav_up(void)
{
    char*s=strrchr(current_path,'/');
    if(s&&s!=current_path){*s=0;if(!*current_path)strcpy(current_path,"/");}
    else if(s==current_path&&strlen(current_path)>1){*s=0;if(!*current_path)strcpy(current_path,"/");}
    scan_dir(current_path);render();
}

static void nav_into(void)
{
    if(selected<0||selected>=entry_count||!entries[selected].is_dir)return;
    if(!strcmp(entries[selected].name,"."))return;
    if(!strcmp(entries[selected].name,"..")){nav_up();return;}
    char np[MAX_PATH];
    if(!strcmp(current_path,"/"))snprintf(np,sizeof(np),"/%s",entries[selected].name);
    else snprintf(np,sizeof(np),"%s/%s",current_path,entries[selected].name);
    strncpy(current_path,np,MAX_PATH-1);current_path[MAX_PATH-1]=0;
    scan_dir(current_path);render();
}

static void reg_global(void*d,struct wl_registry*r,uint32_t n,const char*i,uint32_t v)
{(void)d;(void)v;if(!strcmp(i,"wl_compositor"))compositor=wl_registry_bind(r,n,&wl_compositor_interface,4);
else if(!strcmp(i,"wl_shm"))shm=wl_registry_bind(r,n,&wl_shm_interface,1);
else if(!strcmp(i,"xdg_wm_base"))xdg_wm_base=wl_registry_bind(r,n,&xdg_wm_base_interface,3);
else if(!strcmp(i,"wl_seat")){seat=wl_registry_bind(r,n,&wl_seat_interface,4);keyboard=wl_seat_get_keyboard(seat);}}

static void reg_remove(void*d,struct wl_registry*r,uint32_t n){(void)d;(void)r;(void)n;}
static const struct wl_registry_listener rl={.global=reg_global,.global_remove=reg_remove};
static int configured=0;
static void xs_conf(void*d,struct xdg_surface*x,uint32_t s){(void)d;xdg_surface_ack_configure(x,s);configured=1;render();}
static const struct xdg_surface_listener xsl={.configure=xs_conf};
static void xt_conf(void*d,struct xdg_toplevel*t,int32_t w,int32_t h,struct wl_array*s){(void)d;(void)t;(void)w;(void)h;(void)s;}
static void xt_close(void*d,struct xdg_toplevel*t){(void)d;(void)t;running=0;}
static const struct xdg_toplevel_listener xtl={.configure=xt_conf,.close=xt_close};
static void xwm_ping(void*d,struct xdg_wm_base*b,uint32_t s){(void)d;xdg_wm_base_pong(b,s);}
static const struct xdg_wm_base_listener xwml={.ping=xwm_ping};
static void kb_keymap(void*d,struct wl_keyboard*k,uint32_t f,int32_t fd,uint32_t s){(void)d;(void)k;(void)f;(void)s;close(fd);}
static void kb_enter(void*d,struct wl_keyboard*k,uint32_t s,struct wl_surface*su,struct wl_array*ke){(void)d;(void)k;(void)s;(void)su;(void)ke;}
static void kb_leave(void*d,struct wl_keyboard*k,uint32_t s,struct wl_surface*su){(void)d;(void)k;(void)s;(void)su;}
static void kb_key(void*d,struct wl_keyboard*k,uint32_t se,uint32_t t,uint32_t key,uint32_t st)
{(void)d;(void)k;(void)se;(void)t;if(st!=1)return;
switch(key){case 103:if(selected>0){selected--;render();}break;case 108:if(selected<entry_count-1){selected++;render();}break;
case 28:nav_into();break;case 14:nav_up();break;case 16:running=0;break;case 50:file_dnd_mark(entries[selected].name,current_path);render();break;case 33:file_dnd_paste(current_path);scan_dir(current_path);render();break;}}
static void kb_mods(void*d,struct wl_keyboard*k,uint32_t s,uint32_t a,uint32_t b,uint32_t c,uint32_t e){(void)d;(void)k;(void)s;(void)a;(void)b;(void)c;(void)e;}
static const struct wl_keyboard_listener kbl={.keymap=kb_keymap,.enter=kb_enter,.leave=kb_leave,.key=kb_key,.modifiers=kb_mods};

static struct wl_buffer*mkbuf(void)
{
    char t[]="/run/wlt-fm-shm-XXXXXX";int fd=mkostemp(t,O_CLOEXEC);
    if(fd<0)return NULL;unlink(t);
    if(ftruncate(fd,WIN_W*WIN_H*4)<0){close(fd);return NULL;}
    shm_data=mmap(NULL,WIN_W*WIN_H*4,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(shm_data==MAP_FAILED){close(fd);return NULL;}
    struct wl_shm_pool*pool=wl_shm_create_pool(shm,fd,WIN_W*WIN_H*4);
    struct wl_buffer*buf=wl_shm_pool_create_buffer(pool,0,WIN_W,WIN_H,WIN_W*4,WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);return buf;
}

int main(void)
{
    fprintf(stderr,"[wlt-fm] Starting\n");
    display=wl_display_connect(NULL);
    if(!display){fprintf(stderr,"[fm] No Wayland\n");return 1;}
    registry=wl_display_get_registry(display);
    wl_registry_add_listener(registry,&rl,NULL);
    wl_display_roundtrip(display);
    if(!compositor||!shm||!xdg_wm_base){fprintf(stderr,"[fm] Missing globals\n");return 1;}
    xdg_wm_base_add_listener(xdg_wm_base,&xwml,NULL);
    surface=wl_compositor_create_surface(compositor);
    xdg_surface=xdg_wm_base_get_xdg_surface(xdg_wm_base,surface);
    xdg_surface_add_listener(xdg_surface,&xsl,NULL);
    toplevel=xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_add_listener(toplevel,&xtl,NULL);
    xdg_toplevel_set_title(toplevel,"WLTIOS File Manager");
    xdg_toplevel_set_app_id(toplevel,"wlt-filemanager");
    buffer=mkbuf();
    if(!buffer){fprintf(stderr,"[fm] No buffer\n");return 1;}
    scan_dir(current_path);
    if(keyboard)wl_keyboard_add_listener(keyboard,&kbl,NULL);
    fprintf(stderr,"[wlt-fm] Window ready\n");
    while(running)wl_display_dispatch(display);
    fprintf(stderr,"[wlt-fm] Exit\n");
    if(toplevel)xdg_toplevel_destroy(toplevel);
    if(xdg_surface)xdg_surface_destroy(xdg_surface);
    if(surface)wl_surface_destroy(surface);
    if(buffer)wl_buffer_destroy(buffer);
    wl_display_disconnect(display);
    return 0;
}

