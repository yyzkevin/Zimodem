#include "lwip/err.h"
#include "lwip/init.h" // LWIP_VERSION_
#include "lwip/netif.h" // struct netif

struct ethernet_packet {
    char *payload;
    uint16_t len;
    struct ethernet_packet *next;
};

class ZPacket : public ZMode {
    private:
        ZSerial serial;           
        struct netif* ESPif;
        struct ethernet_packet *packet_queue;
        void push_packet(char *payload,uint16_t len);
        void debug_frame_print(ethernet_packet *p);

    public:
        void switchTo(); 
        void serialIncoming();
        void ethernetIncoming(struct pbuf* p, struct netif* inp);
        void loop();        
        
};
