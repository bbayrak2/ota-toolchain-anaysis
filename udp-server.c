
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678
typedef struct {
    uint32_t magic;
    uint16_t block_id;
    uint16_t total_blocks;
    uint16_t data_len;
    uint16_t checksum;
    uint8_t payload[BLOCK_SIZE];
} ota_packet_t;

typedef struct {
    uint32_t magic;
    uint16_t block_id;
    uint8_t status;
} ota_ack_t;

static struct simple_udp_connection udp_conn;
static uint16_t expected_block = 0;
static bool is_complete = false;

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);

static uint16_t calculate_checksum(const uint8_t *data, uint16_t len) {
    uint16_t sum = 0;
    for(uint16_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

static void
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
    
    if(packet.magic != OTA_MAGIC_NUMBER) {
        return;
    }
    
    uint16_t calc_chk = calculate_checksum(packet.payload, packet.data_len);
    if(calc_chk != packet.checksum) {
        LOG_INFO("Checksum error for block %u. Expected: %u, Got: %u\n", packet.block_id, packet.checksum, calc_chk);
        return; 
    }
    
    LOG_INFO("Received block %u/%u\n", packet.block_id + 1, packet.total_blocks);
    
    if(packet.block_id == expected_block) {
        int fd;
        if(expected_block == 0) {
            cfs_remove("firmware.bin"); 
            fd = cfs_open("firmware.bin", CFS_WRITE);
        } else {
            fd = cfs_open("firmware.bin", CFS_WRITE | CFS_APPEND);
        }
        
        if(fd >= 0) {
            cfs_write(fd, packet.payload, packet.data_len);
            cfs_close(fd);
            expected_block++;
            
      
            if(expected_block == packet.total_blocks && !is_complete) {
                is_complete = true;
                LOG_INFO("Yüklenmeye hazır yeni firmware alımı tamamlandı.\n");
            }
        } else {
            LOG_ERR("CFS Error! Flash write failed.\n");
            return; 
        }
    }
    

    ota_ack_t ack;
    ack.magic = OTA_MAGIC_NUMBER;
    ack.block_id = packet.block_id;
    ack.status = 1;
    simple_udp_sendto(&udp_conn, &ack, sizeof(ota_ack_t), sender_addr);
}

PROCESS_THREAD(udp_server_process, ev, data)
{
  PROCESS_BEGIN();


  NETSTACK_ROUTING.root_start();

  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);
  LOG_INFO("UDP server started, waiting for OTA firmware...\n");
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
