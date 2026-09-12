#ifndef NET_H
#define NET_H
/*
 * kernel/net/net.h — LightOS Ağ Yığını
 * =====================================
 * RTL8139 → Ethernet → ARP → IP → TCP → HTTP
 * 32-bit, bare-metal
 *
 * QEMU: -device rtl8139,netdev=n0 -netdev user,id=n0,hostfwd=tcp::8080-:80
 * VBox: Ağ → PCnet-PCI II (Am79C970A)
 */
#include <stdint.h>
#include <stddef.h>

/* ── Byte order ── */
static inline uint16_t htons(uint16_t v){return (uint16_t)((v>>8)|(v<<8));}
static inline uint32_t htonl(uint32_t v){
    return ((v>>24)&0xFF)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|((v<<24)&0xFF000000u);
}
#define ntohs htons
#define ntohl htonl

/* ── MAC / IP ── */
typedef struct { uint8_t b[6]; } __attribute__((packed)) MAC;
typedef uint32_t IP4;

static inline bool mac_eq(MAC a,MAC b){
    for(int i=0;i<6;i++) if(a.b[i]!=b.b[i]) return false;
    return true;
}
static const MAC MAC_BROADCAST={{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}};
static const MAC MAC_ZERO={{0,0,0,0,0,0}};

/* IP: a.b.c.d */
#define IP4_MAKE(a,b,c,d) ((uint32_t)(a)|((uint32_t)(b)<<8)|((uint32_t)(c)<<16)|((uint32_t)(d)<<24))

/* ── Ethernet frame ── */
#define ETH_TYPE_IP   0x0800
#define ETH_TYPE_ARP  0x0806

typedef struct {
    MAC      dst,src;
    uint16_t type;        /* big-endian */
    uint8_t  payload[];
} __attribute__((packed)) EthFrame;

/* ── ARP ── */
#define ARP_REQUEST 1
#define ARP_REPLY   2

typedef struct {
    uint16_t hw_type;     /* 1=Ethernet */
    uint16_t proto_type;  /* 0x0800=IP */
    uint8_t  hw_len;      /* 6 */
    uint8_t  proto_len;   /* 4 */
    uint16_t opcode;
    MAC      sender_mac;
    IP4      sender_ip;
    MAC      target_mac;
    IP4      target_ip;
} __attribute__((packed)) ARPPacket;

/* ── IP header ── */
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

typedef struct {
    uint8_t  ver_ihl;     /* 0x45 = IPv4, IHL=5 */
    uint8_t  dscp;
    uint16_t total_len;   /* big-endian */
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    IP4      src,dst;
    uint8_t  payload[];
} __attribute__((packed)) IPHeader;

/* IP checksum */
static uint16_t ip_checksum(const void* data,int len){
    const uint16_t* p=(const uint16_t*)data;
    uint32_t sum=0;
    while(len>1){sum+=*p++;len-=2;}
    if(len) sum+=*(const uint8_t*)p;
    while(sum>>16) sum=(sum&0xFFFF)+(sum>>16);
    return (uint16_t)~sum;
}

/* ── TCP header ── */
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

typedef struct {
    uint16_t src_port,dst_port;
    uint32_t seq,ack;
    uint8_t  data_offset; /* high nibble = header len/4 */
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
    uint8_t  payload[];
} __attribute__((packed)) TCPHeader;

/* ── UDP header ── */
typedef struct {
    uint16_t src_port,dst_port;
    uint16_t length;
    uint16_t checksum;
    uint8_t  payload[];
} __attribute__((packed)) UDPHeader;

/* ── DNS ── */
typedef struct {
    uint16_t id,flags,qdcount,ancount,nscount,arcount;
} __attribute__((packed)) DNSHeader;

/* ── Ağ durumu ── */
struct NetState {
    MAC  my_mac;
    IP4  my_ip;
    IP4  gateway_ip;
    IP4  dns_ip;
    IP4  subnet;
    bool dhcp_done;
    bool link_up;
    /* ARP tablosu */
    struct { IP4 ip; MAC mac; bool valid; } arp_cache[16];
    int  arp_count;
    /* TX/RX buffer */
    uint8_t  tx_buf[4][1536];
    uint8_t  rx_buf[8192+16];
    uint16_t rx_read;
    /* Sıradaki paket ID */
    uint16_t ip_id;
};

extern NetState g_net;

/* ── ARP cache ── */
static MAC* arp_lookup(IP4 ip){
    for(int i=0;i<g_net.arp_count;i++)
        if(g_net.arp_cache[i].valid&&g_net.arp_cache[i].ip==ip)
            return &g_net.arp_cache[i].mac;
    return nullptr;
}
static void arp_store(IP4 ip, MAC mac){
    for(int i=0;i<g_net.arp_count;i++)
        if(g_net.arp_cache[i].ip==ip){g_net.arp_cache[i].mac=mac;return;}
    if(g_net.arp_count<16){
        g_net.arp_cache[g_net.arp_count]={ip,mac,true};
        g_net.arp_count++;
    }
}

#endif
