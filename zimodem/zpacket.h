#include "lwip/err.h"
#include "lwip/init.h" // LWIP_VERSION_
#include "lwip/netif.h" // struct netif

/*
Thank You to the following site, for saving me signifigant time in having already figured out the location/offset
when intercepting ppEnqueueRxq

https://github.com/ernacktob/esp8266_wifi_raw

Reason for using ppEnqueueRxq vs  NETif->input is due to some mishandling of certain frames (ipx/appletalk) and I don't
know how to fix the if_llc stuff in the libraries.  


*/

struct RxControl {              //Size = 96 bits / 12 Bytes
        signed rssi:8;
        unsigned rate:4;
        unsigned is_group:1;
        unsigned:1;
        unsigned sig_mode:2;
        unsigned legacy_length:12;
        unsigned damatch0:1;
        unsigned damatch1:1;
        unsigned bssidmatch0:1;
        unsigned bssidmatch1:1;
        unsigned MCS:7;
        unsigned CWB:1;
        unsigned HT_length:16;
        unsigned Smoothing:1;
        unsigned Not_Sounding:1;
        unsigned:1;
        unsigned Aggregation:1;
        unsigned STBC:2;
        unsigned FEC_CODING:1;
        unsigned SGI:1;
        unsigned rxend_state:8;
        unsigned ampdu_cnt:8;
        unsigned channel:4;
        unsigned:12;
};

struct RxPacket {
        struct RxControl rx_ctl;
        uint8 data[];
};

struct ethernet_packet {
    char *payload;
    uint16_t len;
    struct ethernet_packet *next;
};
struct ethernet_packet *packet_queue;

/*
Lazy, putting this code in the header to deal with the fact this is a .ino project.
*/
void ICACHE_RAM_ATTR rx_frame(struct RxPacket *p) {    
    /*
    b0..b1 = protocol (always 0)
    b2..b3 = type (10 for data)
    b4..b7 = subtype
    */
    if(p->data[0] != 0x08) return;  // We only want to  handle data frames        
    if(p->rx_ctl.legacy_length > 1514) return; //any larger means something is wrong
    
    ethernet_packet *pkt;
    pkt = (ethernet_packet *) malloc(sizeof(ethernet_packet));
    pkt->len=p->rx_ctl.legacy_length-12;//this length includes RxControl
    pkt->payload=(char *)malloc(pkt->len);
    memcpy(pkt->payload,p->data,pkt->len);    
    pkt->next = packet_queue;
    packet_queue=pkt;    
}


extern "C" void __real_ppEnqueueRxq(void *);
extern "C" void ICACHE_RAM_ATTR  __wrap_ppEnqueueRxq(void *a) {
//   packet_received_cb((RxPacket *)a);
    rx_frame(((struct RxPacket *)(((void **)a)[4])));
   __real_ppEnqueueRxq(a);  
}

class ZPacket : public ZMode {
    private:
        ZSerial serial;           
        struct netif* ESPif;        
        void push_packet(char *payload,uint16_t len);
        void debug_frame_print(ethernet_packet *p);

    public:
        void switchTo(); 
        void serialIncoming();        
        void loop();        
        
};
