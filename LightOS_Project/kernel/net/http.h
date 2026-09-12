#ifndef HTTP_H
#define HTTP_H
/*
 * kernel/net/http.h — HTTP/1.0 Client
 * =====================================
 * GET isteği gönder, yanıtı al, header'ı ayır.
 * DNS: Basit UDP DNS lookup.
 */
#include "tcp.h"

/* ── DNS lookup ── */
static IP4 dns_resolve(const char* hostname){
    /* Önce bilinen adresleri kontrol et */
    if(hostname[0]>='0'&&hostname[0]<='9'){
        /* IP string → int */
        uint8_t b[4]={0}; int bi=0;
        for(int i=0;hostname[i]&&bi<4;i++){
            if(hostname[i]=='.'){bi++;continue;}
            b[bi]=(uint8_t)(b[bi]*10+(hostname[i]-'0'));
        }
        return IP4_MAKE(b[0],b[1],b[2],b[3]);
    }

    /* DNS sorgusu */
    uint8_t pkt[512]; int off=0;
    EthFrame* eth=(EthFrame*)pkt;
    eth->dst=MAC_BROADCAST; eth->src=g_net.my_mac;
    eth->type=htons(ETH_TYPE_IP);
    off=14;

    IPHeader* ip=(IPHeader*)(pkt+off);
    ip->ver_ihl=0x45;ip->dscp=0;
    ip->total_len=0; /* sonra doldur */
    ip->id=htons(g_net.ip_id++);ip->flags_frag=htons(0x4000);
    ip->ttl=64;ip->protocol=IP_PROTO_UDP;
    ip->checksum=0;ip->src=g_net.my_ip;ip->dst=g_net.dns_ip;
    off+=20;

    UDPHeader* udp=(UDPHeader*)(pkt+off);
    udp->src_port=htons(53001);udp->dst_port=htons(53);
    udp->checksum=0;
    off+=8;

    DNSHeader* dns=(DNSHeader*)(pkt+off);
    dns->id=htons(0x1234);dns->flags=htons(0x0100);
    dns->qdcount=htons(1);dns->ancount=0;dns->nscount=0;dns->arcount=0;
    off+=12;

    /* QNAME encode */
    const char* h=hostname;
    while(*h){
        const char* dot=h;while(*dot&&*dot!='.') dot++;
        int llen=(int)(dot-h);
        pkt[off++]=(uint8_t)llen;
        for(int i=0;i<llen;i++) pkt[off++]=(uint8_t)h[i];
        h=dot+(*dot?1:0);
        if(!*dot) break;
    }
    pkt[off++]=0; /* root */
    /* Type A, Class IN */
    pkt[off++]=0;pkt[off++]=1;
    pkt[off++]=0;pkt[off++]=1;

    /* UDP length + IP total */
    uint16_t udp_len=(uint16_t)(off-14-20);
    udp->length=htons(udp_len);
    ip->total_len=htons((uint16_t)(off-14));
    ip->checksum=ip_checksum(ip,20);

    /* Gateway MAC bul */
    do_arp(g_net.gateway_ip);
    MAC* gw=arp_lookup(g_net.gateway_ip);
    if(gw) eth->dst=*gw;
    g_rtl.send(pkt,(uint16_t)off);

    /* Yanıt bekle */
    for(int t=0;t<5000000;t++){
        uint16_t plen; uint8_t* p=g_rtl.recv(plen);
        if(!p) continue;
        EthFrame* ef=(EthFrame*)p;
        if(htons(ef->type)!=ETH_TYPE_IP) continue;
        IPHeader* iph=(IPHeader*)(p+14);
        if(iph->protocol!=IP_PROTO_UDP) continue;
        UDPHeader* uh=(UDPHeader*)(p+14+20);
        if(htons(uh->dst_port)!=53001) continue;
        DNSHeader* dh=(DNSHeader*)(p+14+20+8);
        if(dh->id!=htons(0x1234)) continue;
        if(htons(dh->ancount)<1) return 0;
        /* Answer section skip */
        uint8_t* ans=(uint8_t*)(dh+1);
        /* Skip question */
        while(*ans) {
            if((*ans&0xC0)==0xC0){ans+=2;break;}
            ans+=*ans+1;
        }
        ans+=4; /* qtype+qclass */
        /* Answer */
        if((*ans&0xC0)==0xC0) ans+=2; else while(*ans) ans+=*ans+1;
        ans+=10; /* type+class+ttl+rdlength */
        if(plen>(uint16_t)(ans-p+4))
            return *(IP4*)ans;
    }
    return 0;
}

/* HTTP yanıt yapısı */
struct HTTPResponse {
    int      status;           /* 200, 404 vs. */
    char     content_type[64];
    int      content_length;
    char*    body;             /* rx_buf içinde */
    int      body_len;
    bool     ok;
};

/* URL parser */
struct ParsedURL {
    char host[128];
    char path[256];
    uint16_t port;
};

static bool parse_url(const char* url, ParsedURL& out){
    out.port=80;
    out.path[0]='/'; out.path[1]=0;
    /* http:// veya https:// */
    const char* p=url;
    if(p[0]=='h'&&p[1]=='t'&&p[2]=='t'&&p[3]=='p'){
        p+=4;
        if(*p=='s') p++; /* https — port 443, biz desteklemiyoruz */
        if(*p==':'&&*(p+1)=='/'&&*(p+2)=='/') p+=3;
    }
    /* host */
    int hi=0;
    while(*p&&*p!='/'&&*p!=':'&&hi<127) out.host[hi++]=*p++;
    out.host[hi]=0;
    /* port */
    if(*p==':'){
        p++; out.port=0;
        while(*p>='0'&&*p<='9') out.port=(uint16_t)(out.port*10+(*p++)-'0');
    }
    /* path */
    if(*p=='/'){
        int pi=0;
        while(*p&&pi<255) out.path[pi++]=*p++;
        out.path[pi]=0;
    }
    return hi>0;
}

/* ── HTTP GET ── */
static HTTPResponse http_get(const char* url, TCPConn& conn){
    HTTPResponse resp={0,{0},0,nullptr,0,false};

    ParsedURL pu;
    if(!parse_url(url,pu)) return resp;

    /* DNS */
    IP4 server_ip=dns_resolve(pu.host);
    if(!server_ip) return resp;

    /* TCP connect */
    if(!tcp_connect(conn,server_ip,pu.port)) return resp;

    /* HTTP GET isteği */
    char req[512];
    int rlen=0;
    auto ra=[&](const char* s){while(*s&&rlen<510)req[rlen++]=*s++;};
    ra("GET "); ra(pu.path); ra(" HTTP/1.0\r\n");
    ra("Host: "); ra(pu.host); ra("\r\n");
    ra("User-Agent: LightSurf/1.0\r\n");
    ra("Accept: text/html,text/plain\r\n");
    ra("Connection: close\r\n");
    ra("\r\n");

    tcp_send(conn,req,rlen);

    /* Yanıt al */
    int bytes=tcp_recv(conn);
    if(bytes<=0){tcp_close(conn);return resp;}

    /* Header parse */
    char* buf=(char*)conn.rx_buf;
    /* Status line */
    if(buf[0]=='H'&&buf[1]=='T'&&buf[2]=='T'&&buf[3]=='P'){
        int si=0;
        while(buf[si]&&buf[si]!=' ') si++;
        resp.status=0;
        si++;
        while(buf[si]>='0'&&buf[si]<='9')
            resp.status=resp.status*10+(buf[si++]-'0');
    }
    /* Header biter: \r\n\r\n */
    char* body=buf;
    for(int i=0;i<bytes-3;i++){
        if(buf[i]=='\r'&&buf[i+1]=='\n'&&buf[i+2]=='\r'&&buf[i+3]=='\n'){
            body=buf+i+4; break;
        }
    }
    resp.body=body;
    resp.body_len=(int)(bytes-(body-buf));
    if(resp.body_len<0) resp.body_len=0;
    resp.ok=(resp.status>=200&&resp.status<300);
    return resp;
}

#endif
