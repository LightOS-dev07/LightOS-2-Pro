#ifndef DHCP_H
#define DHCP_H
/*
 * kernel/net/dhcp.h — DHCP Client (minimal)
 * DISCOVER → OFFER → REQUEST → ACK
 */
#include "net.h"
#include "rtl8139.h"
#include "e1000.h"
static inline bool _nic_send(const void* d,uint16_t l){
    if(g_e1000.ready) return g_e1000.send(d,l);
    return _nic_send(d,l);
}
static inline uint8_t* _nic_recv(uint16_t& len){
    uint8_t* p=g_e1000.ready?g_e1000.recv(len):nullptr;
    return p?p:_nic_recv(len);
}

#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_ACK      5

typedef struct {
    uint8_t  op,htype,hlen,hops;
    uint32_t xid;
    uint16_t secs,flags;
    IP4      ciaddr,yiaddr,siaddr,giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;   /* 0x63825363 */
    uint8_t  options[312];
} __attribute__((packed)) DHCPPacket;

static bool dhcp_request(){
    /* UDP üzerinden DHCP DISCOVER */
    uint8_t pkt[342+42]; int off=0;

    EthFrame* eth=(EthFrame*)pkt;
    eth->dst=MAC_BROADCAST; eth->src=g_net.my_mac;
    eth->type=htons(ETH_TYPE_IP);
    off=14;

    IPHeader* ip=(IPHeader*)(pkt+off);
    ip->ver_ihl=0x45; ip->dscp=0;
    ip->total_len=htons(328);
    ip->id=0; ip->flags_frag=0;
    ip->ttl=64; ip->protocol=IP_PROTO_UDP;
    ip->checksum=0;
    ip->src=0; ip->dst=0xFFFFFFFF;
    ip->checksum=ip_checksum(ip,20);
    off+=20;

    UDPHeader* udp=(UDPHeader*)(pkt+off);
    udp->src_port=htons(68);
    udp->dst_port=htons(67);
    udp->length=htons(308);
    udp->checksum=0;
    off+=8;

    DHCPPacket* d=(DHCPPacket*)(pkt+off);
    for(int i=0;i<(int)sizeof(DHCPPacket);i++) ((uint8_t*)d)[i]=0;
    d->op=1; d->htype=1; d->hlen=6;
    d->xid=0xDEAD1234;
    d->flags=htons(0x8000); /* broadcast */
    for(int i=0;i<6;i++) d->chaddr[i]=g_net.my_mac.b[i];
    d->magic=htonl(0x63825363);
    /* Options: DHCP type=DISCOVER */
    int oi=0;
    d->options[oi++]=53;d->options[oi++]=1;d->options[oi++]=DHCP_DISCOVER;
    d->options[oi++]=55;d->options[oi++]=4;
    d->options[oi++]=1;d->options[oi++]=3;d->options[oi++]=6;d->options[oi++]=15;
    d->options[oi++]=255;
    off+=(int)sizeof(DHCPPacket);

    _nic_send(pkt,off);

    /* OFFER bekle */
    IP4 offered=0,server=0,gw=0,dns=0,mask=0;
    for(int t=0;t<500000;t++){
        uint16_t plen; uint8_t* p=_nic_recv(plen);
        if(!p) continue;
        if(plen<14+20+8+(int)sizeof(DHCPPacket)) continue;
        EthFrame* ef=(EthFrame*)p;
        if(htons(ef->type)!=ETH_TYPE_IP) continue;
        IPHeader* iph=(IPHeader*)(p+14);
        if(iph->protocol!=IP_PROTO_UDP) continue;
        UDPHeader* uh=(UDPHeader*)(p+14+20);
        if(htons(uh->dst_port)!=68) continue;
        DHCPPacket* rp=(DHCPPacket*)(p+14+20+8);
        if(rp->op!=2||rp->xid!=0xDEAD1234) continue;
        offered=rp->yiaddr;
        /* Option parse */
        uint8_t* opt=rp->options; int olen=308;
        for(int i=0;i<olen;){
            if(opt[i]==255) break;
            if(opt[i]==0){i++;continue;}
            uint8_t tag=opt[i],len2=opt[i+1];
            if(tag==54) server=*(IP4*)(opt+i+2);
            if(tag==1)  mask=*(IP4*)(opt+i+2);
            if(tag==3)  gw=*(IP4*)(opt+i+2);
            if(tag==6)  dns=*(IP4*)(opt+i+2);
            i+=2+len2;
        }
        break;
    }
    if(!offered) return false;

    /* REQUEST */
    for(int i=0;i<(int)sizeof(DHCPPacket);i++) ((uint8_t*)d)[i]=0;
    d->op=1;d->htype=1;d->hlen=6;d->xid=0xDEAD1234;
    d->flags=htons(0x8000);
    for(int i=0;i<6;i++) d->chaddr[i]=g_net.my_mac.b[i];
    d->magic=htonl(0x63825363);
    int oi2=0;
    d->options[oi2++]=53;d->options[oi2++]=1;d->options[oi2++]=DHCP_REQUEST;
    d->options[oi2++]=50;d->options[oi2++]=4;
    *(IP4*)(d->options+oi2)=offered;oi2+=4;
    d->options[oi2++]=54;d->options[oi2++]=4;
    *(IP4*)(d->options+oi2)=server;oi2+=4;
    d->options[oi2++]=255;
    _nic_send(pkt,off);

    /* ACK bekle */
    for(int t=0;t<300000;t++){
        uint16_t plen; uint8_t* p=_nic_recv(plen);
        if(!p) continue;
        if(plen<14+20+8+(int)sizeof(DHCPPacket)) continue;
        EthFrame* ef=(EthFrame*)p;
        if(htons(ef->type)!=ETH_TYPE_IP) continue;
        IPHeader* iph=(IPHeader*)(p+14);
        if(iph->protocol!=IP_PROTO_UDP) continue;
        UDPHeader* uh=(UDPHeader*)(p+14+20);
        if(htons(uh->dst_port)!=68) continue;
        DHCPPacket* rp=(DHCPPacket*)(p+14+20+8);
        if(rp->op!=2||rp->xid!=0xDEAD1234) continue;
        /* ACK aldık */
        g_net.my_ip=offered;
        g_net.gateway_ip=gw?gw:IP4_MAKE(10,0,2,2);
        g_net.dns_ip=dns?dns:IP4_MAKE(8,8,8,8);
        g_net.subnet=mask?mask:IP4_MAKE(255,255,255,0);
        g_net.dhcp_done=true;
        g_net.link_up=true;
        return true;
    }
    return false;
}
#endif
