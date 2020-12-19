#include "lwip/err.h"
#include "lwip/init.h" // LWIP_VERSION_
#include "lwip/netif.h" // struct netif
#include "lwip/opt.h"
#include "osapi.h"


extern "C" struct netif* eagle_lwip_getif (int netif_index);

netif_input_fn current_rx_handler;
err_t zpacket_rx_handler(struct pbuf* p, struct netif* inp) {
    char *pkt;
    pkt = (char *)p->payload;
    if(pkt[0] == 0xFF && pkt[1] == 0xFF && pkt[2]==0xFF && pkt[3]==0xFF && pkt[4]==0xFF && pkt[5]==0xFF ) {
        currMode->ethernetIncoming(p,inp);
    }
    else if(pkt[0] == mac_address[0] && pkt[1] == mac_address[1] && pkt[2] == mac_address[2] && pkt[3] == mac_address[3] && pkt[4] == mac_address[4] && pkt[5] == mac_address[5]) {
        currMode->ethernetIncoming(p,inp);
    }

    return current_rx_handler(p,inp);    
}

void ZPacket::switchTo() {        
    serial.println("Entering Packet Mode\n");    
    serial.flush();
    serial.setFlowControlType(FCT_DISABLED);        
    ESPif = eagle_lwip_getif(0);    
    if(ESPif != null) {
        serial.println("Got ESP IF\n");                
        serial.flush();
    }      
    packet_queue=null;
    packet_queue_depth=0;
    current_rx_handler = ESPif->input;
    ESPif->input = zpacket_rx_handler;
    enable_rx_frame=true;
    WiFi.macAddress(mac_address); 
    
    currMode=&packetMode;
}

void ZPacket::ethernetIncoming(struct pbuf* p, struct netif* inp) {
    struct pbuf *q;
    if(packet_queue_depth > max_packet_queue) return;
    ethernet_packet *pkt;
    for(q = p; q != NULL; q = q->next) {                
        pkt = (ethernet_packet *) malloc(sizeof(ethernet_packet));
        if(!pkt) return;
        pkt->ppEnqueue=0;
        pkt->next = packet_queue;
        pkt->payload = (char *)malloc(q->len);
        if(!pkt->payload) {
            delete pkt;
            return;
        }
        pkt->len = pbuf_copy_partial(q,pkt->payload,1500,0);
        packet_queue=pkt;        
        packet_queue_depth++;
    }       
}

void ZPacket::serialIncoming() {
//    serial.prints("incoming serial");
//    serial.flush();
}

/*
TX test code
    //char tx[42]= {0xff,0xff,0xff,0xff,0xff,0xff,0x5c,0xcf,0x7f,0x8b,0x1c,0x9a,0x08,0x06,0x00,0x01,0x08,0x00,0x06,0x04,0x00,0x01,0x5c,0xcf,0x7f,0x8b,0x1c,0x9a,0xc0,0xa8,0x56,0xdc,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xa8,0x56,0xdc};    
    //test = pbuf_alloc(PBUF_RAW_TX, sizeof(tx), PBUF_RAM);    
    //if(!test) {
    //    serial.println("error allocating pbuff");
    //    serial.flush();
    //    return;
    //}
    //test->len = sizeof(tx);
    //memcpy(test->payload ,tx ,sizeof(tx));  

    if(!ESPif->linkoutput(ESPif,test)) {
        serial.println("error sending");
    }
    else {
        serial.println("success sending");
       // pbuf_free(test);

//    if(p->payload[0] != 0x29) return;
    
       //serial.printf("type:%u..",ethz->type);

*/
void ZPacket::debug_frame_print(ethernet_packet *p) {    
    int i;
    if(p->ppEnqueue) {
        if(p->payload[32] !=0xAA || p->payload[33] !=0xAA)  return;   // Should always be snap?
        serial.prints("pp:");
        for(i=0;i<6;i++) {
            serial.printf("%02x ",p->payload[4+i]);
        }
        for(i=0;i<6;i++) {
            serial.printf("%02x ",p->payload[16+i]);
        }
        for(i=37;i<p->len;i++) {
            serial.printf("%02x ",p->payload[i]);
        }
    }
    else {
        serial.prints("np:");
        for(i=0;i<p->len;i++) {
            serial.printf("%02x ",p->payload[i]);
        }
    }
    serial.prints("\n\r\n\r");
    serial.flush();
}
void ZPacket::loop() {       
    ethernet_packet *p;
    while(packet_queue != NULL) {
        p = packet_queue;
        packet_queue = packet_queue->next;
        debug_frame_print(p);        
        free(p->payload);
        delete p;
        packet_queue_depth--;
    }            
    //currMode = &commandMode;    
}


