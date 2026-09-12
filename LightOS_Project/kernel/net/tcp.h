#ifndef TCP_H
#define TCP_H
/*
 * kernel/net/tcp.h — Minimal TCP Stack
 * =====================================
 * Sadece client-side HTTP için yeterli:
 *   connect() → send() → recv() → close()
 * 
 * Tek bağlantı (browser için yeterli).
 */
#include "net.h"
#include "rtl8139.h"
#include "e1000.h"

/* Unified NIC: e1000 önce, RTL8139 fallback */
static bool nic_send(const void* d,uint16_t l){
    if(g_e1000.ready) return g_e1000.send(d,l);
    return nic_send(d,l);
}
static uint8_t* nic_recv(uint16_t& len){
    uint8_t* p=nullptr;
    if(g_e1000.ready) p=g_e1000.recv(len);
    if(!p) p=nic_recv(len);
    return p;
}

#define TCP_STATE_CLOSED     0
#define TCP_STATE_SYN_SENT   1
#define TCP_STATE_ESTABLISHED 2
#define TCP_STATE_FIN_WAIT   3
#define TCP_STATE_TIME_WAIT  4

struct TCPConn {
    int      state;
    IP4      remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq;      /* our sequence */
    uint32_t ack;      /* their sequence */
    uint8_t  rx_buf[8192];
    int      rx_len;
    bool     got_fin;
};

/* ── Ethernet + IP + TCP paket gönder ── */
static void send_tcp(TCPConn& conn, uint8_t flags,
                     const uint8_t* data=nullptr, uint16_t dlen=0)
{
    /* Toplam boyut */
    uint16_t ip_len = (uint16_t)(20 + 20 + dlen);
    uint8_t  pkt[1560]; int off=0;

    /* Ethernet */
    MAC* gw_mac = arp_lookup(conn.remote_ip);
    if(!gw_mac) gw_mac = arp_lookup(g_net.gateway_ip);
    if(!gw_mac){ /* ARP yap — basit fallback */ return; }

    EthFrame* eth=(EthFrame*)pkt;
    eth->dst=*gw_mac;
    eth->src=g_net.my_mac;
    eth->type=htons(ETH_TYPE_IP);
    off=14;

    /* IP */
    IPHeader* ip=(IPHeader*)(pkt+off);
    ip->ver_ihl=0x45;
    ip->dscp=0;
    ip->total_len=htons(ip_len);
    ip->id=htons(g_net.ip_id++);
    ip->flags_frag=htons(0x4000); /* DF */
    ip->ttl=64;
    ip->protocol=IP_PROTO_TCP;
    ip->checksum=0;
    ip->src=g_net.my_ip;
    ip->dst=conn.remote_ip;
    ip->checksum=ip_checksum(ip,20);
    off+=20;

    /* TCP */
    TCPHeader* tcp=(TCPHeader*)(pkt+off);
    tcp->src_port=htons(conn.local_port);
    tcp->dst_port=htons(conn.remote_port);
    tcp->seq=htonl(conn.seq);
    tcp->ack=htonl(conn.ack);
    tcp->data_offset=0x50; /* 20 bytes */
    tcp->flags=flags;
    tcp->window=htons(8192);
    tcp->urgent=0;
    /* TCP checksum — pseudo header */
    struct { IP4 src,dst; uint8_t zero,proto; uint16_t len; } ph;
    ph.src=g_net.my_ip; ph.dst=conn.remote_ip;
    ph.zero=0; ph.proto=IP_PROTO_TCP;
    ph.len=htons((uint16_t)(20+dlen));
    uint32_t csum=0;
    auto add=[&](const void* p,int l){
        const uint16_t* pp=(const uint16_t*)p;
        while(l>1){csum+=*pp++;l-=2;}
        if(l) csum+=*(const uint8_t*)pp;
    };
    add(&ph,sizeof(ph)); add(tcp,20);
    if(data&&dlen) add(data,dlen);
    while(csum>>16) csum=(csum&0xFFFF)+(csum>>16);
    tcp->checksum=(uint16_t)~csum;
    off+=20;

    if(data&&dlen) for(int i=0;i<dlen;i++) pkt[off+i]=data[i];
    off+=dlen;

    nic_send(pkt,off);
    if(data&&dlen) conn.seq+=dlen;
}

/* ── ARP gönder ve cevap bekle ── */
static bool do_arp(IP4 target_ip){
    /* Zaten biliyor muyuz? */
    if(arp_lookup(target_ip)) return true;

    /* ARP request */
    uint8_t pkt[42];
    EthFrame* eth=(EthFrame*)pkt;
    eth->dst=MAC_BROADCAST;
    eth->src=g_net.my_mac;
    eth->type=htons(ETH_TYPE_ARP);
    ARPPacket* arp=(ARPPacket*)(pkt+14);
    arp->hw_type=htons(1);
    arp->proto_type=htons(ETH_TYPE_IP);
    arp->hw_len=6; arp->proto_len=4;
    arp->opcode=htons(ARP_REQUEST);
    arp->sender_mac=g_net.my_mac;
    arp->sender_ip=g_net.my_ip;
    arp->target_mac=MAC_ZERO;
    arp->target_ip=target_ip;
    nic_send(pkt,42);

    /* Cevap bekle (timeout ~1M iterasyon) */
    for(int t=0;t<200000;t++){
        uint16_t len; uint8_t* p=nic_recv(len);
        if(!p) continue;
        EthFrame* ef=(EthFrame*)p;
        if(htons(ef->type)!=ETH_TYPE_ARP) continue;
        ARPPacket* ar=(ARPPacket*)(p+14);
        if(htons(ar->opcode)!=ARP_REPLY) continue;
        if(ar->sender_ip!=target_ip) continue;
        arp_store(target_ip,ar->sender_mac);
        return true;
    }
    return false;
}

/* ── TCP bağlantı kur ── */
static bool tcp_connect(TCPConn& conn, IP4 ip, uint16_t port){
    conn.state=TCP_STATE_CLOSED;
    conn.remote_ip=ip;
    conn.remote_port=port;
    conn.local_port=(uint16_t)(49152+(g_net.ip_id&0x3FFF));
    conn.seq=0x12345678;
    conn.ack=0;
    conn.rx_len=0;
    conn.got_fin=false;

    /* ARP */
    IP4 via=(ip&g_net.subnet)==(g_net.my_ip&g_net.subnet)?ip:g_net.gateway_ip;
    if(!do_arp(via)) return false;

    /* SYN */
    send_tcp(conn,TCP_SYN);
    conn.seq++;
    conn.state=TCP_STATE_SYN_SENT;

    /* SYN-ACK bekle */
    for(int t=0;t<500000;t++){
        uint16_t plen; uint8_t* p=nic_recv(plen);
        if(!p) continue;
        EthFrame* ef=(EthFrame*)p;
        if(htons(ef->type)!=ETH_TYPE_IP) continue;
        IPHeader* iph=(IPHeader*)(p+14);
        if(iph->protocol!=IP_PROTO_TCP) continue;
        TCPHeader* th=(TCPHeader*)(p+14+20);
        if(htons(th->dst_port)!=conn.local_port) continue;
        if(!(th->flags&(TCP_SYN|TCP_ACK))) continue;
        conn.ack=ntohl(th->seq)+1;
        /* ACK */
        send_tcp(conn,TCP_ACK);
        conn.state=TCP_STATE_ESTABLISHED;
        return true;
    }
    return false;
}

/* ── Veri gönder ── */
static bool tcp_send(TCPConn& conn, const char* data, int len){
    if(conn.state!=TCP_STATE_ESTABLISHED) return false;
    while(len>0){
        int chunk=len>1400?1400:len;
        send_tcp(conn,TCP_PSH|TCP_ACK,(const uint8_t*)data,(uint16_t)chunk);
        data+=chunk; len-=chunk;
    }
    return true;
}

/* ── Veri al ── */
static int tcp_recv(TCPConn& conn, int timeout_iters=2000000){
    conn.rx_len=0;
    for(int t=0;t<timeout_iters&&!conn.got_fin;t++){
        uint16_t plen; uint8_t* p=nic_recv(plen);
        if(!p) continue;
        EthFrame* ef=(EthFrame*)p;
        if(htons(ef->type)!=ETH_TYPE_IP) continue;
        IPHeader* iph=(IPHeader*)(p+14);
        if(iph->protocol!=IP_PROTO_TCP||iph->src!=conn.remote_ip) continue;
        TCPHeader* th=(TCPHeader*)(p+14+20);
        if(htons(th->dst_port)!=conn.local_port) continue;
        /* ACK güncelle */
        int ip_len=htons(iph->total_len);
        int tcp_hdr=(th->data_offset>>4)*4;
        int data_len=ip_len-20-tcp_hdr;
        if(data_len>0){
            uint8_t* payload=(uint8_t*)th+tcp_hdr;
            int copy=data_len;
            if(conn.rx_len+copy>(int)sizeof(conn.rx_buf))
                copy=(int)sizeof(conn.rx_buf)-conn.rx_len;
            for(int i=0;i<copy;i++) conn.rx_buf[conn.rx_len+i]=payload[i];
            conn.rx_len+=copy;
            conn.ack=ntohl(th->seq)+data_len;
            send_tcp(conn,TCP_ACK);
/* t=0; reset disabled to prevent infinite loop */
        }
        if(th->flags&TCP_FIN){conn.got_fin=true;conn.ack++;}
    }
    return conn.rx_len;
}

/* ── Bağlantıyı kapat ── */
static void tcp_close(TCPConn& conn){
    if(conn.state==TCP_STATE_ESTABLISHED){
        send_tcp(conn,TCP_FIN|TCP_ACK);
        conn.seq++;
        conn.state=TCP_STATE_FIN_WAIT;
    }
    conn.state=TCP_STATE_CLOSED;
}

#endif
