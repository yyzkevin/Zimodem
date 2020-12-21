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
        
        packet_len = len + 6;

        packet_buffer[12]=(packet_len >> 8) & 0xFF;
        packet_buffer[13]=packet_len & 0xFF;

        packet_buffer[14]=0xAA;
        packet_buffer[15]=0xAA;
        packet_buffer[16]=0x03;
        packet_buffer[17]=0x00;
        packet_buffer[18]=0x00;
        packet_buffer[19]=0x00;
        packet_buffer[20]=0x81;
        packet_buffer[21]=0x37;
        memcpy(packet_buffer+22,payload,len);
        send_frame(packet_buffer,packet_len+14);

}
void ZPacket::slip_rx(uint8_t ch) {
    uint16_t decoded_size;
    if(ch == 0xC0) { 
            decoded_size=SLIP::decode(slip_rx_buffer+1,3030,slip_rx_buffer_decoded);                        
            ipx_send(slip_rx_buffer,slip_rx_buffer_len);
            slip_rx_buffer_len=0;                
            return;
    }
    if(slip_rx_buffer_len > 1000) return;//temporary hack       
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
void ZPacket::debug_msg(uint8_t *msg,uint16_t len) {        
    packet_buffer[0]=0xFF;
    packet_buffer[1]=0xFF;
    packet_buffer[2]=0xFF;
    packet_buffer[3]=0xFF;
    packet_buffer[4]=0xFF;
    packet_buffer[5]=0xFF;
    packet_buffer[6]=0x5C;
    packet_buffer[7]=0xCF;
    packet_buffer[8]=0x7F;
    packet_buffer[9]=0x8B;
    packet_buffer[10]=0x1C;
    packet_buffer[11]=0x9A;
    packet_buffer[12]=0x12;
    packet_buffer[13]=0x34;
    memcpy(packet_buffer+14,msg,len);    
    send_frame(packet_buffer,len+14);
    
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


