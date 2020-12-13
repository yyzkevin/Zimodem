#include "lwip/err.h"
#include "lwip/init.h" // LWIP_VERSION_
#include "lwip/netif.h" // struct netif

extern "C" struct netif* eagle_lwip_getif (int netif_index);

netif_input_fn current_rx_handler;
err_t zpacket_rx_handler(struct pbuf* p, struct netif* inp) {
    currMode->ethernetIncoming(p,inp);
    return current_rx_handler(p,inp);    
}



void ZPacket::switchTo() {        
    serial.prints("Entering Packet Mode");    
    serial.flush();
    ESPif = eagle_lwip_getif(0);    
    if(ESPif != null) {
        current_rx_handler = ESPif->input;        
        ESPif->input = zpacket_rx_handler;
        serial.println("Got ESP IF\n");                
        serial.flush();
    }      
    
    currMode=&packetMode;
}

void ZPacket::serialIncoming() {
//    serial.prints("incoming serial");
//    serial.flush();
}
void ZPacket::ethernetIncoming(struct pbuf* p, struct netif* inp) {
    struct pbuf *q;
    for(q = p; q != NULL; q = q->next) {
        push_packet((char *)q->payload,q->len);
    }       
}
void ZPacket::push_packet(char *payload,uint16_t len) {
    ethernet_packet *p;
    p = (ethernet_packet *) malloc(sizeof(ethernet_packet));
    if(!p) return;
    p->next = packet_queue;
    p->len = len;
    p->payload = (char *)malloc(len);        
    if(!p->payload) return;
    memcpy(p->payload,payload,len);    
    packet_queue=p;
}
void ZPacket::debug_frame_print(ethernet_packet *p) {
    int i;
    serial.print("Packet:");
    if(p->len > 16) {
        for(i=0;i<16;i++) {
            serial.printf("%02x ",p->payload[i]);       
        }
    }
    serial.print("\n\r");
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

    }        
    //currMode = &commandMode;    
}


