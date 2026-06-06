#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "storage/cfs/cfs.h"
#include "lib/crc16.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "sys/log.h"

#define LOG_MODULE "UDP-Server-OTA"
#define LOG_LEVEL LOG_LEVEL_INFO
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define PKT_TYPE_DATA 1
#define PKT_TYPE_ACK  2
#define PKT_TYPE_NACK 3

#define MAX_PAYLOAD_LEN 64

typedef struct __attribute__((packed)) {
  uint8_t type;
  uint16_t seq_no;
  uint16_t total_blocks;
  uint8_t payload_len;
  uint16_t crc16;
  uint8_t payload[MAX_PAYLOAD_LEN];
} ota_packet_t;

static struct simple_udp_connection udp_conn;
static uint16_t expected_block = 0;
static bool is_complete = false;

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);

static void udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr, uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr, uint16_t receiver_port,
         const uint8_t *data, uint16_t datalen)
{
    if(datalen != sizeof(ota_packet_t)) {
        return;
    }
    
    ota_packet_t packet;
    memcpy(&packet, data, sizeof(ota_packet_t));
    
    
    if(packet.type != PKT_TYPE_DATA) {
        return;
    }
    
    uint16_t calc_chk = crc16_data(packet.payload, packet.payload_len, 0);
    if(calc_chk != packet.crc16) {
        LOG_INFO("Checksum error for block %u. Expected: %u, Got: %u\n", packet.seq_no, packet.crc16, calc_chk);
        
      
        ota_packet_t nack_pkt;
        memset(&nack_pkt, 0, sizeof(nack_pkt));
        nack_pkt.type = PKT_TYPE_NACK;
        nack_pkt.seq_no = packet.seq_no;
        simple_udp_sendto(&udp_conn, &nack_pkt, sizeof(nack_pkt), sender_addr);
        return; 
    }
    
    LOG_INFO("Received block %u/%u\n", packet.seq_no + 1, packet.total_blocks);
    
    if(packet.seq_no == expected_block) {
        int fd;
        if(expected_block == 0) {
            cfs_remove("firmware.bin"); 
            fd = cfs_open("firmware.bin", CFS_WRITE);
        } else {
            fd = cfs_open("firmware.bin", CFS_WRITE | CFS_APPEND);
        }
        
        if(fd >= 0) {
            cfs_write(fd, packet.payload, packet.payload_len);
            cfs_close(fd);
            expected_block++;
            
            if(expected_block == packet.total_blocks && !is_complete) {
                is_complete = true;
                LOG_INFO("Yüklenmeye hazır yeni firmware alımı tamamlandı.\n");
            }
        } else {
            LOG_ERR("CFS Hatası! Flash belleğe yazma başarısız.\n");
            return; 
        }
    }
    

    ota_packet_t ack_pkt;
    memset(&ack_pkt, 0, sizeof(ack_pkt));
    ack_pkt.type = PKT_TYPE_ACK;
    ack_pkt.seq_no = packet.seq_no;
    simple_udp_sendto(&udp_conn, &ack_pkt, sizeof(ack_pkt), sender_addr);
}

PROCESS_THREAD(udp_server_process, ev, data)
{
    PROCESS_BEGIN();
    
    NETSTACK_ROUTING.root_start();
    
    simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                        UDP_CLIENT_PORT, udp_rx_callback);
                        
    LOG_INFO("UDP sunucusu başlatıldı, OTA yazılımı bekleniyor...\n");
    PROCESS_END();
}
