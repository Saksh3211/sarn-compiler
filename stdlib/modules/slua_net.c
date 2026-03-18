#include "slua_net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET SlSock;
#define SL_INV INVALID_SOCKET
#define SL_ERR SOCKET_ERROR
#define sl_close closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int SlSock;
#define SL_INV (-1)
#define SL_ERR (-1)
#define sl_close close
#endif
#define NET_MAX 64
static SlSock _socks[NET_MAX];
static int    _nready;
int32_t slua_net_init(void) {
    if (_nready) return 1;
    for (int i=0;i<NET_MAX;i++) _socks[i]=SL_INV;
#ifdef _WIN32
    WSADATA wd; if (WSAStartup(MAKEWORD(2,2),&wd)!=0) return 0;
#endif
    _nready=1; return 1;
}
static int64_t _store(SlSock s) {
    for (int i=0;i<NET_MAX;i++) if(_socks[i]==SL_INV){ _socks[i]=s; return (int64_t)i; }
    sl_close(s); return -1;
}
int64_t slua_net_connect(const char* host, int32_t port) {
    slua_net_init();
    struct addrinfo hints={0},*res=NULL; hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
    char ps[16]; snprintf(ps,sizeof(ps),"%d",port);
    if (getaddrinfo(host,ps,&hints,&res)!=0||!res) return -1;
    SlSock s=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if (s==SL_INV) { freeaddrinfo(res); return -1; }
    if (connect(s,res->ai_addr,(int)res->ai_addrlen)==SL_ERR) { sl_close(s); freeaddrinfo(res); return -1; }
    freeaddrinfo(res); return _store(s);
}
int64_t slua_net_listen(int32_t port) {
    slua_net_init();
    SlSock s=socket(AF_INET,SOCK_STREAM,0); if (s==SL_INV) return -1;
    int opt=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(char*)&opt,sizeof(opt));
    struct sockaddr_in addr={0}; addr.sin_family=AF_INET; addr.sin_port=htons((uint16_t)port); addr.sin_addr.s_addr=INADDR_ANY;
    if (bind(s,(struct sockaddr*)&addr,sizeof(addr))==SL_ERR) { sl_close(s); return -1; }
    if (listen(s,5)==SL_ERR) { sl_close(s); return -1; }
    return _store(s);
}
int64_t slua_net_accept(int64_t sid) {
    if (sid<0||sid>=NET_MAX||_socks[sid]==SL_INV) return -1;
    struct sockaddr_in addr; int len=sizeof(addr);
    SlSock c=accept(_socks[sid],(struct sockaddr*)&addr,&len);
    return (c==SL_INV)?-1:_store(c);
}
int32_t slua_net_send(int64_t id, const char* data) {
    if (id<0||id>=NET_MAX||_socks[id]==SL_INV) return 0;
    int n=(int)strlen(data); return (send(_socks[id],data,n,0)==n)?1:0;
}
int32_t slua_net_send_bytes(int64_t id, const char* data, int32_t len) {
    if (id<0||id>=NET_MAX||_socks[id]==SL_INV) return 0;
    return (send(_socks[id],data,len,0)==len)?1:0;
}
char* slua_net_recv(int64_t id, int32_t maxlen) {
    if (id<0||id>=NET_MAX||_socks[id]==SL_INV) return strdup("");
    if (maxlen<=0||maxlen>65536) maxlen=4096;
    char* b=(char*)malloc((size_t)maxlen+1); if (!b) return strdup("");
    int n=recv(_socks[id],b,maxlen,0); if (n<=0) { free(b); return strdup(""); }
    b[n]='\0'; return b;
}
int32_t slua_net_close(int64_t id) {
    if (id<0||id>=NET_MAX||_socks[id]==SL_INV) return 0;
    sl_close(_socks[id]); _socks[id]=SL_INV; return 1;
}
char* slua_net_local_ip(void) {
    slua_net_init(); char hn[256]; gethostname(hn,sizeof(hn));
    struct hostent* h=gethostbyname(hn);
    return h?strdup(inet_ntoa(*(struct in_addr*)h->h_addr)):strdup("127.0.0.1");
}
