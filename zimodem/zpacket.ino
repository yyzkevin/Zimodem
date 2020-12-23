#include "lwip/err.h"
#include "lwip/init.h" // LWIP_VERSION_
#include "lwip/netif.h" // struct netif
#include "lwip/opt.h"
#include "lwip/pbuf.h"

#include "osapi.h"
#include "slip.h"



/*
Mode to for SLIP encapsulated IPX packets.

This is all unstable, hacked in for testing to see if it is feasible.
*/


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


void ZPacket::ipx_send(uint8_t *payload,uint16_t len) {
        uint16_t packet_len;
        packet_buffer[0]=0xFF;
        packet_buffer[1]=0xFF;
        packet_buffer[2]=0xFF;
        packet_buffer[3]=0xFF;
        packet_buffer[4]=0xFF;
        packet_buffer[5]=0xFF;
        packet_buffer[6]=mac_address[0];
        packet_buffer[7]=mac_address[1];
        packet_buffer[8]=mac_address[2];
        packet_buffer[9]=mac_address[3];
        packet_buffer[10]=mac_address[4];
        packet_buffer[11]=mac_address[5];
        
        packet_len = len + 3;
        packet_buffer[12]=(packet_len >> 8) & 0xFF;
        packet_buffer[13]=packet_len & 0xFF;

        packet_buffer[14]=0xE0;         
        packet_buffer[15]=0xE0;         
        packet_buffer[16]=0x03;         
        memcpy(packet_buffer+17,payload,len);
        send_frame(packet_buffer,len+17);        
}



void ZPacket::slip_rx(uint8_t ch) {
    //does not handle  escapes properly yet
    uint16_t decoded_size;
    if(ch == SLIP::END && slip_rx_buffer_len==0) return;
    if(ch == SLIP::END) {        
        ipx_send(slip_rx_buffer,slip_rx_buffer_len);
        slip_rx_buffer_len=0;
        return;
    }          
    if(slip_rx_buffer_len > 1000) return;
    slip_rx_buffer[slip_rx_buffer_len]=ch;
    slip_rx_buffer_len++;
}

void ZPacket::serialIncoming() {
    int bytesAvailable = HWSerial.available();
    if(bytesAvailable == 0) return;
    while(--bytesAvailable >= 0) {
        slip_rx(HWSerial.read());
    }      
}
void ZPacket::send_frame(uint8_t *payload,uint16_t len) {
    pbuf *tx;
    tx = pbuf_alloc(PBUF_RAW_TX, len, PBUF_RAM);        
    tx->len = len;
    memcpy(tx->payload,payload,len);
    ESPif->linkoutput(ESPif,tx);
}

void ZPacket::slip_tx(ethernet_packet *p) {    
    int i;
    if(p->ppEnqueue) {        
        memcpy(packet_buffer,p->payload+4,6);
        memcpy(packet_buffer+6,p->payload+16,6);
        memcpy(packet_buffer+12,p->payload+37,p->len-37);
        if(p->payload[32] !=0xAA || p->payload[33] !=0xAA)  return;   // Should always be snap?                
    }
    else {
        memcpy(packet_buffer,p->payload,p->len);
    }
    if(packet_buffer[13] != 0x81 || packet_buffer[14] != 0x37) return;      //IPX ONLY
    SLIP::encode(packet_buffer,p->len,slip_tx_buffer);    
    for(i=0;i<p->len;i++) {  
        serial.printc(packet_buffer[i]);              
    }           
    serial.flush();    
}

void ZPacket::loop() {       
    ethernet_packet *p;
    while(packet_queue != NULL) {
        p = packet_queue;
        packet_queue = packet_queue->next;
        slip_tx(p);        
        free(p->payload);
        delete p;
        packet_queue_depth--;
    }        
    //currMode = &commandMode;    
}


