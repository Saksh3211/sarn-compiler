#include "slua_http.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
static int _parse_url(const char* url, wchar_t* whost, wchar_t* wpath, int* port, int* use_ssl) {
    *use_ssl = 0; *port = 80;
    const char* p = url;
    if (strncmp(p,"https://",8)==0) { p+=8; *use_ssl=1; *port=443; }
    else if (strncmp(p,"http://",7)==0) { p+=7; }
    else return 0;
    const char* slash = strchr(p, '/');
    const char* colon = strchr(p, ':');
    size_t hlen = slash ? (size_t)(slash-p) : strlen(p);
    if (colon && colon < (slash?slash:p+strlen(p))) { hlen=(size_t)(colon-p); *port=atoi(colon+1); }
    char host[512]; memset(host,0,sizeof(host));
    strncpy(host, p, hlen < 511 ? hlen : 511);
    MultiByteToWideChar(CP_UTF8,0,host,-1,whost,512);
    const char* path = slash ? slash : "/";
    MultiByteToWideChar(CP_UTF8,0,path,-1,wpath,2048);
    return 1;
}
static char* _winhttp_request(const char* url, const char* method, const char* body, const char* ctype, int* out_status) {
    wchar_t whost[512], wpath[2048];
    int port, use_ssl;
    if (!_parse_url(url,whost,wpath,&port,&use_ssl)) return strdup("");
    HINTERNET hSession = WinHttpOpen(L"SLua/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,NULL,NULL,0);
    if (!hSession) return strdup("");
    HINTERNET hConnect = WinHttpConnect(hSession,whost,(INTERNET_PORT)port,0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return strdup(""); }
    DWORD flags = use_ssl ? WINHTTP_FLAG_SECURE : 0;
    wchar_t wmeth[16]; MultiByteToWideChar(CP_UTF8,0,method,-1,wmeth,16);
    HINTERNET hReq = WinHttpOpenRequest(hConnect,wmeth,wpath,NULL,NULL,NULL,flags);
    if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return strdup(""); }
    if (use_ssl) {
        DWORD sec = SECURITY_FLAG_IGNORE_CERT_DATE_INVALID|SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        WinHttpSetOption(hReq,WINHTTP_OPTION_SECURITY_FLAGS,&sec,sizeof(sec));
    }
    DWORD blen = body ? (DWORD)strlen(body) : 0;
    wchar_t wct[256] = {0};
    if (ctype) MultiByteToWideChar(CP_UTF8,0,ctype,-1,wct,256);
    if (ctype && blen>0) {
        wchar_t hdr[512]; swprintf_s(hdr,512,L"Content-Type: %s\r\n",wct);
        WinHttpAddRequestHeaders(hReq,hdr,-1L,WINHTTP_ADDREQ_FLAG_ADD);
    }
    WinHttpSendRequest(hReq,NULL,0,(LPVOID)body,blen,blen,0);
    WinHttpReceiveResponse(hReq,NULL);
    if (out_status) {
        DWORD sc=0,scl=sizeof(sc);
        WinHttpQueryHeaders(hReq,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,NULL,&sc,&scl,NULL);
        *out_status=(int)sc;
    }
    char* result = (char*)malloc(1); result[0]='\0'; size_t total=0;
    DWORD avail=0;
    while (WinHttpQueryDataAvailable(hReq,&avail) && avail>0) {
        char* tmp=(char*)malloc(avail+1); DWORD read=0;
        WinHttpReadData(hReq,tmp,avail,&read); tmp[read]='\0';
        result=(char*)realloc(result,total+read+1);
        memcpy(result+total,tmp,read); total+=read; result[total]='\0'; free(tmp);
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return result;
}
char*   slua_http_get(const char* url)  { return _winhttp_request(url,"GET",NULL,NULL,NULL); }
char*   slua_http_post(const char* url, const char* body, const char* ctype) { return _winhttp_request(url,"POST",body,ctype,NULL); }
char*   slua_http_post_json(const char* url, const char* body) { return _winhttp_request(url,"POST",body,"application/json",NULL); }
int32_t slua_http_status(const char* url) { int s=0; char* r=_winhttp_request(url,"GET",NULL,NULL,&s); free(r); return s; }
char*   slua_http_get_header(const char* url, const char* hdr) { (void)url;(void)hdr; return strdup(""); }
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
static char* _sock_request(const char* url, const char* method, const char* body, const char* ctype) {
    int is_https=0; const char* p=url;
    if (strncmp(p,"https://",8)==0) { p+=8; is_https=1; }
    else if (strncmp(p,"http://",7)==0) p+=7;
    else return strdup("");
    char host[512]; int port=is_https?443:80;
    const char* slash=strchr(p,'/'); const char* colon=strchr(p,':');
    size_t hlen=slash?(size_t)(slash-p):strlen(p);
    if (colon&&colon<(slash?slash:p+strlen(p))) { hlen=(size_t)(colon-p); port=atoi(colon+1); }
    strncpy(host,p,hlen<511?hlen:511); host[hlen<511?hlen:511]='\0';
    const char* path=slash?slash:"/";
    struct addrinfo hints={0},*res=NULL; hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
    char ps[16]; snprintf(ps,sizeof(ps),"%d",port);
    if (getaddrinfo(host,ps,&hints,&res)!=0||!res) return strdup("");
    int s=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if (s<0) { freeaddrinfo(res); return strdup(""); }
    if (connect(s,res->ai_addr,(socklen_t)res->ai_addrlen)<0) { close(s); freeaddrinfo(res); return strdup(""); }
    freeaddrinfo(res);
    size_t blen=body?strlen(body):0;
    char req[8192];
    int rlen=snprintf(req,sizeof(req),
        "%s %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nContent-Length: %zu\r\n%s%s\r\n\r\n",
        method,path,host,blen,ctype&&blen>0?"Content-Type: ":"",ctype&&blen>0?ctype:"");
    send(s,req,(size_t)rlen,0);
    if (body&&blen>0) send(s,body,blen,0);
    char* result=(char*)malloc(1); result[0]='\0'; size_t total=0;
    char buf[4096]; ssize_t n;
    while ((n=recv(s,buf,sizeof(buf)-1,0))>0) {
        result=(char*)realloc(result,total+(size_t)n+1);
        memcpy(result+total,buf,(size_t)n); total+=(size_t)n; result[total]='\0';
    }
    close(s);
    char* body_start=strstr(result,"\r\n\r\n");
    if (body_start) { char* r=strdup(body_start+4); free(result); return r; }
    return result;
}
char*   slua_http_get(const char* url)  { return _sock_request(url,"GET",NULL,NULL); }
char*   slua_http_post(const char* url, const char* body, const char* ctype) { return _sock_request(url,"POST",body,ctype); }
char*   slua_http_post_json(const char* url, const char* body) { return _sock_request(url,"POST",body,"application/json"); }
int32_t slua_http_status(const char* url) { char* r=slua_http_get(url); int ok=(r&&r[0])?200:0; free(r); return ok; }
char*   slua_http_get_header(const char* url, const char* hdr) { (void)url;(void)hdr; return strdup(""); }
#endif
