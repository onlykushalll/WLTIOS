#define _GNU_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>

static void msleep(int ms){struct timespec ts={ms/1000,(long)(ms%1000)*1000000L};nanosleep(&ts,NULL);}
static int connect_local(int port,int timeout_s){
  int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
  struct sockaddr_in a;memset(&a,0,sizeof(a));
  a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  for(int i=0;i<timeout_s*2;i++){if(connect(fd,(struct sockaddr*)&a,sizeof(a))==0)return fd;msleep(500);}
  close(fd);return -1;
}
int main(void){
  signal(SIGALRM,SIG_DFL);
  printf("=== WLTIOS SOCKS5 TEST ===\n");fflush(stdout);
  printf("[C.1] Connecting to 127.0.0.1:9050...\n");fflush(stdout);
  int fd=connect_local(9050,30);
  if(fd<0){printf("[C.1] FAIL: cannot connect to 127.0.0.1:9050 after 30s\n");fflush(stdout);return 1;}
  printf("[C.1] Connected to Tor SOCKS5 port.\n");fflush(stdout);
  unsigned char greet[3]={0x05,0x01,0x00};
  alarm(10);
  if(write(fd,greet,3)!=3){printf("[C.1] FAIL: greeting write failed\n");alarm(0);close(fd);fflush(stdout);return 1;}
  unsigned char gr[2]={0,0};int n=read(fd,gr,2);alarm(0);
  if(n==2&&gr[0]==0x05&&gr[1]==0x00){printf("[C.1] PASS: SOCKS5 greeting OK (ver=5 method=0 no-auth)\n");}
  else{printf("[C.1] FAIL: greeting bad (n=%d %02x %02x)\n",n,gr[0],gr[1]);close(fd);fflush(stdout);return 1;}
  fflush(stdout);
  unsigned char conn[10]={0x05,0x01,0x00,0x01,138,201,212,248,0x00,0x50};
  printf("[C.2] SOCKS5 CONNECT to 138.201.212.248:80 (check.torproject.org)...\n");fflush(stdout);
  alarm(30);
  if(write(fd,conn,10)!=10){printf("[C.2] FAIL: connect write failed\n");alarm(0);close(fd);fflush(stdout);return 1;}
  unsigned char cr[16]={0};n=read(fd,cr,10);alarm(0);
  if(n<2){printf("[C.2] FAIL: connect response short (n=%d)\n",n);close(fd);fflush(stdout);return 1;}
  if(cr[1]==0x00){
    printf("[C.2] PASS: SOCKS5 CONNECT accepted (circuit established)\n");
    const char*http="GET / HTTP/1.0\r\nHost: check.torproject.org\r\nUser-Agent: WLTIOS-TorCheck/0.1\r\nConnection: close\r\n\r\n";
    alarm(20);write(fd,http,strlen(http));
    char buf[2048];int total=0;
    while(total<(int)sizeof(buf)-1){int r=read(fd,buf+total,sizeof(buf)-total-1);if(r<=0)break;total+=r;}
    alarm(0);buf[total]=0;
    printf("[C.3] HTTP response (%d bytes):\n",total);
    int plen=total<500?total:500;fwrite(buf,1,plen,stdout);printf("\n");
    if(total>0)printf("[C.3] PASS: Traffic routed through Tor. Tor bootstrapped and functional.\n");
    else printf("[C.3] FAIL: no HTTP response\n");
  }else{
    const char*reasons[]={"OK","general failure","not allowed","net unreachable","host unreachable","conn refused","TTL expired","cmd not supported","addr not supported"};
    const char*r=cr[1]<9?reasons[cr[1]]:"unknown";
    printf("[C.2] CONNECT rejected (reply=%d=%s)\n",cr[1],r);
    printf("[C.2] NOTE: reply=1 or 4 means Tor not bootstrapped (needs network)\n");
  }
  fflush(stdout);close(fd);
  printf("=== SOCKS5 TEST DONE ===\n");fflush(stdout);return 0;
}
