#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/route.h>

static void bring_up_loopback(void){
  /* bring up lo interface and assign 127.0.0.1 */
  int s=socket(AF_INET,SOCK_DGRAM,0);
  if(s<0)return;
  struct ifreq ifr;
  memset(&ifr,0,sizeof(ifr));
  strncpy(ifr.ifr_name,"lo",IFNAMSIZ);
  /* set flags: IFF_UP | IFF_RUNNING */
  if(ioctl(s,SIOCGIFFLAGS,&ifr)==0){
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    ioctl(s,SIOCSIFFLAGS,&ifr);
  }
  /* set address 127.0.0.1 */
  memset(&ifr,0,sizeof(ifr));
  strncpy(ifr.ifr_name,"lo",IFNAMSIZ);
  struct sockaddr_in* sa=(struct sockaddr_in*)&ifr.ifr_addr;
  sa->sin_family=AF_INET;
  sa->sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  ioctl(s,SIOCSIFADDR,&ifr);
  /* set netmask 255.0.0.0 */
  memset(&ifr,0,sizeof(ifr));
  strncpy(ifr.ifr_name,"lo",IFNAMSIZ);
  sa=(struct sockaddr_in*)&ifr.ifr_netmask;
  sa->sin_family=AF_INET;
  sa->sin_addr.s_addr=htonl(0xff000000);
  ioctl(s,SIOCSIFNETMASK,&ifr);
  close(s);
}

static void bring_up_eth0(void){
  /* Configure eth0 with static IP for QEMU SLIRP network:
   * Guest: 10.0.2.15, Gateway: 10.0.2.2, DNS: 10.0.2.3, Netmask: 255.255.255.0
   */
  int s=socket(AF_INET,SOCK_DGRAM,0);
  if(s<0)return;
  struct ifreq ifr;
  /* set address 10.0.2.15 */
  memset(&ifr,0,sizeof(ifr));
  strncpy(ifr.ifr_name,"eth0",IFNAMSIZ);
  struct sockaddr_in* sa=(struct sockaddr_in*)&ifr.ifr_addr;
  sa->sin_family=AF_INET;
  sa->sin_addr.s_addr=htonl(0x0a00020f); /* 10.0.2.15 */
  if(ioctl(s,SIOCSIFADDR,&ifr)<0){
    close(s);return;
  }
  /* set netmask 255.255.255.0 */
  memset(&ifr,0,sizeof(ifr));
  strncpy(ifr.ifr_name,"eth0",IFNAMSIZ);
  sa=(struct sockaddr_in*)&ifr.ifr_netmask;
  sa->sin_family=AF_INET;
  sa->sin_addr.s_addr=htonl(0xffffff00);
  ioctl(s,SIOCSIFNETMASK,&ifr);
  /* set flags: IFF_UP | IFF_RUNNING */
  memset(&ifr,0,sizeof(ifr));
  strncpy(ifr.ifr_name,"eth0",IFNAMSIZ);
  if(ioctl(s,SIOCGIFFLAGS,&ifr)==0){
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    ioctl(s,SIOCSIFFLAGS,&ifr);
  }
  close(s);
  /* Set default route via 10.0.2.2 */
  int rts=socket(AF_INET,SOCK_DGRAM,0);
  if(rts<0)return;
  struct rtentry rt;
  memset(&rt,0,sizeof(rt));
  struct sockaddr_in* dst=(struct sockaddr_in*)&rt.rt_dst;
  dst->sin_family=AF_INET; dst->sin_addr.s_addr=0;
  struct sockaddr_in* gw=(struct sockaddr_in*)&rt.rt_gateway;
  gw->sin_family=AF_INET; gw->sin_addr.s_addr=htonl(0x0a000202); /* 10.0.2.2 */
  struct sockaddr_in* mask=(struct sockaddr_in*)&rt.rt_genmask;
  mask->sin_family=AF_INET; mask->sin_addr.s_addr=0;
  rt.rt_flags = RTF_UP | RTF_GATEWAY;
  ioctl(rts,SIOCADDRT,&rt);
  close(rts);
}

static void msleep(int ms){struct timespec ts={ms/1000,(long)(ms%1000)*1000000L};nanosleep(&ts,NULL);}
static int port_open(int port){
  int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return 0;
  struct sockaddr_in a;memset(&a,0,sizeof(a));
  a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  int r=connect(fd,(struct sockaddr*)&a,sizeof(a));
  close(fd);
  return r==0;
}

/* SOCKS5 test: greeting + CONNECT to check.torproject.org:80 */
static void socks5_test(int consolefd){
  char m[256];
  write(consolefd,"[C.1] Tor SOCKS5 port confirmed, connecting...\r\n",47);
  int fd=socket(AF_INET,SOCK_STREAM,0);
  struct sockaddr_in a;memset(&a,0,sizeof(a));
  a.sin_family=AF_INET;a.sin_port=htons(9050);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(connect(fd,(struct sockaddr*)&a,sizeof(a))<0){
    write(consolefd,"[C.1] FAIL: cannot connect to 127.0.0.1:9050\r\n",44);
    return;
  }
  write(consolefd,"[C.1] Connected to Tor SOCKS5 port.\r\n",36);

  unsigned char greet[3]={0x05,0x01,0x00};
  alarm(10);
  if(write(fd,greet,3)!=3){write(consolefd,"[C.1] FAIL: greeting write\r\n",28);alarm(0);close(fd);return;}
  unsigned char gr[2]={0,0};int n=read(fd,gr,2);alarm(0);
  if(n==2&&gr[0]==0x05&&gr[1]==0x00){
    write(consolefd,"[C.1] PASS: SOCKS5 greeting OK (method=0 no-auth)\r\n",50);
  }else{
    snprintf(m,sizeof(m),"[C.1] FAIL: greeting bad (n=%d %02x %02x)\r\n",n,gr[0],gr[1]);
    write(consolefd,m,strlen(m));close(fd);return;
  }

  /* CONNECT to 138.201.212.248:80 (check.torproject.org) */
  unsigned char conn[10]={0x05,0x01,0x00,0x01,138,201,212,248,0x00,0x50};
  write(consolefd,"[C.2] SOCKS5 CONNECT to 138.201.212.248:80...\r\n",47);
  alarm(30);
  if(write(fd,conn,10)!=10){write(consolefd,"[C.2] FAIL: connect write\r\n",27);alarm(0);close(fd);return;}
  unsigned char cr[16]={0};n=read(fd,cr,10);alarm(0);
  if(n<2){write(consolefd,"[C.2] FAIL: connect response short\r\n",35);close(fd);return;}
  if(cr[1]==0x00){
    write(consolefd,"[C.2] PASS: SOCKS5 CONNECT accepted (circuit established)\r\n",58);
    const char*http="GET / HTTP/1.0\r\nHost: check.torproject.org\r\nUser-Agent: WLTIOS-TorCheck/0.1\r\nConnection: close\r\n\r\n";
    alarm(20);write(fd,http,strlen(http));
    char buf[1024];int total=0;
    while(total<(int)sizeof(buf)-1){int r=read(fd,buf+total,sizeof(buf)-total-1);if(r<=0)break;total+=r;}
    alarm(0);buf[total]=0;
    snprintf(m,sizeof(m),"[C.3] HTTP response (%d bytes):\r\n",total);
    write(consolefd,m,strlen(m));
    int plen=total<400?total:400;
    write(consolefd,buf,plen);
    write(consolefd,"\r\n",2);
    if(total>0)write(consolefd,"[C.3] PASS: Traffic routed through Tor.\r\n",41);
    else write(consolefd,"[C.3] FAIL: no HTTP response\r\n",30);
  }else{
    const char*reasons[]={"OK","general failure","not allowed","net unreachable","host unreachable","conn refused","TTL expired","cmd not supported","addr not supported"};
    const char*r=cr[1]<9?reasons[cr[1]]:"unknown";
    snprintf(m,sizeof(m),"[C.2] CONNECT rejected (reply=%d=%s)\r\n",cr[1],r);
    write(consolefd,m,strlen(m));
    write(consolefd,"[C.2] NOTE: reply=1/4 means Tor not bootstrapped (needs real network)\r\n",68);
  }
  close(fd);
  write(consolefd,"=== SOCKS5 TEST DONE ===\r\n",25);
}

int main(int argc,char**argv){
  /* write startup marker to file for debugging */
  int dfd=open("/tmp/wrap-debug.log",O_WRONLY|O_CREAT|O_TRUNC,0644);
  if(dfd>=0){
    write(dfd,"[wrap] main entered\n",20);
    close(dfd);
  }
  /* fork background SOCKS5 tester */
  pid_t pid=fork();
  if(pid==0){
    setsid();
    bring_up_loopback();
    bring_up_eth0();
    int cfd=open("/dev/console",O_RDWR);
    if(cfd>=0){dup2(cfd,1);dup2(cfd,2);}
    int flog=open("/tmp/socks5-test.log",O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(flog<0)flog=2;
    write(flog,"[socks5-wrap] bg tester started, waiting for Tor...\n",52);
    write(1,"[socks5-wrap] bg tester started\r\n",33);
    /* wait for Tor to start listening */
    int i;
    for(i=0;i<60;i++){
      if(port_open(9050)){
        char m[128];snprintf(m,sizeof(m),"[socks5-wrap] Tor port 9050 UP after %ds, waiting 5s\n",i);
        write(flog,m,strlen(m));
        write(1,m,strlen(m));
        msleep(5000);
        write(flog,"[socks5-wrap] running SOCKS5 test NOW\n",37);
        write(1,"[socks5-wrap] running SOCKS5 test NOW\r\n",39);
        /* write [C.x] results to console (fd 1) so they are captured in the
         * serial boot log; flog is ephemeral (in-memory initrd, lost on exit) */
        socks5_test(1);
        write(flog,"=== SOCKS5 TEST DONE ===\n",25);
        close(flog);
        _exit(0);
      }
      msleep(1000);
    }
    write(flog,"[socks5-wrap] Tor port 9050 never came up after 60s\n",53);
    write(1,"[socks5-wrap] Tor port 9050 never came up\r\n",44);
    close(flog);
    _exit(1);
  }
  /* parent: exec real wlt-shell with argv[0]="wlt-shell" */
  int dfd2=open("/tmp/wrap-debug.log",O_WRONLY|O_APPEND,0644);
  if(dfd2>=0){write(dfd2,"[wrap] forking bg done, execv wlt-shell.real\n",45);close(dfd2);}
  char*new_argv[argc+1];
  new_argv[0]=(char*)"wlt-shell";
  for(int i=1;i<argc;i++) new_argv[i]=argv[i];
  new_argv[argc]=0;
  execv("/bin/wlt-shell.real",new_argv);
  perror("[socks5-wrap] execv wlt-shell.real");
  return 127;
}
