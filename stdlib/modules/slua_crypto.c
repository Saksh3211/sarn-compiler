#include "slua_crypto.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define RR(v,n) (((v)>>(n))|((v)<<(32-(n))))
static uint32_t K[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static void sha256_block(uint32_t* s, const uint8_t* b) {
    uint32_t w[64],a,b2,c,d,e,f,g,h,t1,t2,i;
    for(i=0;i<16;i++) w[i]=((uint32_t)b[i*4]<<24)|((uint32_t)b[i*4+1]<<16)|((uint32_t)b[i*4+2]<<8)|(uint32_t)b[i*4+3];
    for(i=16;i<64;i++){uint32_t s0=RR(w[i-15],7)^RR(w[i-15],18)^(w[i-15]>>3);uint32_t s1=RR(w[i-2],17)^RR(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
    a=s[0];b2=s[1];c=s[2];d=s[3];e=s[4];f=s[5];g=s[6];h=s[7];
    for(i=0;i<64;i++){
        uint32_t S1=RR(e,6)^RR(e,11)^RR(e,25);
        uint32_t ch=(e&f)^(~e&g);
        t1=h+S1+ch+K[i]+w[i];
        uint32_t S0=RR(a,2)^RR(a,13)^RR(a,22);
        uint32_t maj=(a&b2)^(a&c)^(b2&c);
        t2=S0+maj;
        h=g;g=f;f=e;e=d+t1;d=c;c=b2;b2=a;a=t1+t2;
    }
    s[0]+=a;s[1]+=b2;s[2]+=c;s[3]+=d;s[4]+=e;s[5]+=f;s[6]+=g;s[7]+=h;
}
char* slua_crypto_sha256(const char* data) {
    size_t len=strlen(data);
    uint32_t st[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    size_t total=(len+9+63)/64*64;
    uint8_t* buf=(uint8_t*)calloc(1,total);
    memcpy(buf,data,len); buf[len]=0x80;
    uint64_t bits=(uint64_t)len*8;
    for(int i=0;i<8;i++) buf[total-1-i]=(uint8_t)(bits>>(i*8));
    for(size_t i=0;i<total;i+=64) sha256_block(st,buf+i);
    free(buf);
    char* out=(char*)malloc(65);
    for(int i=0;i<8;i++) sprintf(out+i*8,"%08x",st[i]);
    out[64]='\0'; return out;
}
static uint32_t md5_s[]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
static uint32_t md5_K[]={0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
#define LR(x,n) (((x)<<(n))|((x)>>(32-(n))))
char* slua_crypto_md5(const char* data) {
    size_t len=strlen(data);
    uint32_t a0=0x67452301,b0=0xefcdab89,c0=0x98badcfe,d0=0x10325476;
    size_t total=(len+9+63)/64*64;
    uint8_t* buf=(uint8_t*)calloc(1,total);
    memcpy(buf,data,len); buf[len]=0x80;
    uint64_t bits=(uint64_t)len*8;
    for(int i=0;i<8;i++) buf[total-8+i]=(uint8_t)(bits>>(i*8));
    for(size_t off=0;off<total;off+=64){
        uint32_t M[16],A=a0,B=b0,C=c0,D=d0;
        for(int i=0;i<16;i++) M[i]=((uint32_t)buf[off+i*4])|((uint32_t)buf[off+i*4+1]<<8)|((uint32_t)buf[off+i*4+2]<<16)|((uint32_t)buf[off+i*4+3]<<24);
        for(int i=0;i<64;i++){
            uint32_t F,g2;
            if(i<16){F=(B&C)|(~B&D);g2=(uint32_t)i;}
            else if(i<32){F=(D&B)|(~D&C);g2=(uint32_t)(5*i+1)%16;}
            else if(i<48){F=B^C^D;g2=(uint32_t)(3*i+5)%16;}
            else{F=C^(B|~D);g2=(uint32_t)(7*i)%16;}
            F+=A+md5_K[i]+M[g2]; A=D; D=C; C=B; B+=LR(F,md5_s[i]);
        }
        a0+=A;b0+=B;c0+=C;d0+=D;
    }
    free(buf);
    char* out=(char*)malloc(33);
    uint32_t vals[4]={a0,b0,c0,d0};
    for(int i=0;i<4;i++) for(int j=0;j<4;j++) sprintf(out+(i*4+j)*2,"%02x",(vals[i]>>(j*8))&0xff);
    out[32]='\0'; return out;
}
static const char B64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
char* slua_crypto_base64_encode(const char* data, int32_t len) {
    int32_t olen=((len+2)/3)*4+1;
    char* out=(char*)malloc((size_t)olen); int32_t j=0;
    for(int32_t i=0;i<len;i+=3){
        uint32_t v=(uint32_t)((uint8_t)data[i])<<16;
        if(i+1<len) v|=(uint32_t)((uint8_t)data[i+1])<<8;
        if(i+2<len) v|=(uint32_t)((uint8_t)data[i+2]);
        out[j++]=B64[(v>>18)&63]; out[j++]=B64[(v>>12)&63];
        out[j++]=(i+1<len)?B64[(v>>6)&63]:'=';
        out[j++]=(i+2<len)?B64[v&63]:'=';
    }
    out[j]='\0'; return out;
}
char* slua_crypto_base64_decode(const char* b64) {
    size_t n=strlen(b64); char* out=(char*)malloc(n+1); int32_t j=0;
    static const int8_t T[256]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1};
    for(size_t i=0;i+3<n;i+=4){
        int32_t a=T[(uint8_t)b64[i]],b=T[(uint8_t)b64[i+1]],c=T[(uint8_t)b64[i+2]],d=T[(uint8_t)b64[i+3]];
        if(a<0||b<0) break;
        out[j++]=(char)((a<<2)|(b>>4));
        if(b64[i+2]!='=') out[j++]=(char)(((b&15)<<4)|(c>>2));
        if(b64[i+3]!='=') out[j++]=(char)(((c&3)<<6)|d);
    }
    out[j]='\0'; return out;
}
char* slua_crypto_hex_encode(const char* data, int32_t len) {
    char* out=(char*)malloc((size_t)len*2+1);
    for(int32_t i=0;i<len;i++) sprintf(out+i*2,"%02x",(uint8_t)data[i]);
    out[len*2]='\0'; return out;
}
char* slua_crypto_hex_decode(const char* hex) {
    size_t n=strlen(hex); char* out=(char*)malloc(n/2+1); size_t j=0;
    for(size_t i=0;i+1<n;i+=2){ uint32_t v; sscanf(hex+i,"%02x",&v); out[j++]=(char)v; }
    out[j]='\0'; return out;
}
int32_t slua_crypto_hex_decode_len(const char* hex) { return (int32_t)(strlen(hex)/2); }
uint32_t slua_crypto_crc32(const char* data, int32_t len) {
    uint32_t crc=0xFFFFFFFF;
    for(int32_t i=0;i<len;i++){
        crc^=(uint8_t)data[i];
        for(int j=0;j<8;j++) crc=(crc&1)?(crc>>1)^0xEDB88320:(crc>>1);
    }
    return crc^0xFFFFFFFF;
}
char* slua_crypto_hmac_sha256(const char* key, const char* data) {
    uint8_t k[64]={0}; size_t kl=strlen(key);
    if(kl>64){ char* hk=slua_crypto_sha256(key); for(int i=0;i<64&&hk[i];i+=2){ uint32_t v; sscanf(hk+i,"%02x",&v); k[i/2]=(uint8_t)v; } free(hk); }
    else memcpy(k,key,kl);
    uint8_t ipad[64],opad[64];
    for(int i=0;i<64;i++){ ipad[i]=k[i]^0x36; opad[i]=k[i]^0x5c; }
    size_t dl=strlen(data); size_t inner_len=64+dl;
    char* inner=(char*)malloc(inner_len+1);
    memcpy(inner,ipad,64); memcpy(inner+64,data,dl); inner[inner_len]='\0';
    char* h1=slua_crypto_sha256(inner); free(inner);
    size_t outer_len=64+32;
    char* outer=(char*)malloc(outer_len+1);
    memcpy(outer,opad,64);
    for(int i=0;i<32;i+=2){ uint32_t v; sscanf(h1+i,"%02x",&v); outer[64+i/2]=(char)v; }
    outer[outer_len]='\0'; free(h1);
    char* res=slua_crypto_sha256(outer); free(outer); return res;
}
char* slua_crypto_xor(const char* data, int32_t len, const char* key, int32_t keylen) {
    char* out=(char*)malloc((size_t)len+1);
    for(int32_t i=0;i<len;i++) out[i]=data[i]^key[i%keylen];
    out[len]='\0'; return out;
}
