#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "sys/node-id.h"
#include "lib/crc16.h"

#include "firmware_data.h"

#include "sys/log.h"
#define LOG_MODULE "UDP-Client-OTA"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define SEND_INTERVAL        (CLOCK_SECOND * 2)
#define TIMEOUT_INTERVAL     (CLOCK_SECOND * 3)

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
static struct etimer send_timer;

static uint16_t current_seq_no = 0;
static uint16_t total_blocks = 0;
static bool transfer_complete = false;

static void udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  if(datalen == sizeof(ota_packet_t)) {
    ota_packet_t *pkt = (ota_packet_t *)data;

    if(pkt->seq_no == current_seq_no) {
      if(pkt->type == PKT_TYPE_ACK) {
        LOG_INFO("ACK alindi: Blok %u. Sonraki bloga geciliyor.\n", pkt->seq_no);

        current_seq_no++;

        if(current_seq_no >= total_blocks) {
          transfer_complete = true;
          LOG_INFO("Tum firmware bloklari basariyla gonderildi.\n");
        } else {
          etimer_set(&send_timer, CLOCK_SECOND / 50);
        }
      } else if(pkt->type == PKT_TYPE_NACK) {
        LOG_INFO("NACK alindi: Blok %u. Tekrar gonderilecek.\n", pkt->seq_no);
        etimer_set(&send_timer, CLOCK_SECOND / 50);
      }
    }
  }
}

PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&udp_client_process);

PROCESS_THREAD(udp_client_process, ev, data)
{
  static uip_ipaddr_t dest_ipaddr;
  static ota_packet_t pkt;
  static uint32_t firmware_len;

  PROCESS_BEGIN();

  if(node_id != 2) {
    LOG_INFO("Bu dugum router'dir (ID: %u). OTA baslatilmadi.\n", node_id);
    while(1) {
      PROCESS_WAIT_EVENT();
    }
  }

  firmware_len = sizeof(firmware_payload);
  total_blocks = (firmware_len + MAX_PAYLOAD_LEN - 1) / MAX_PAYLOAD_LEN;
  LOG_INFO("Firmware: %lu byte, %u blok\n", (unsigned long)firmware_len, total_blocks);

  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&send_timer, SEND_INTERVAL);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));

    if(transfer_complete) {
      etimer_set(&send_timer, SEND_INTERVAL * 10);
      continue;
    }

    if(NETSTACK_ROUTING.node_is_reachable() &&
       NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {

      pkt.type         = PKT_TYPE_DATA;
      pkt.seq_no       = current_seq_no;
      pkt.total_blocks = total_blocks;

      uint32_t offset    = (uint32_t)current_seq_no * MAX_PAYLOAD_LEN;
      uint32_t remaining = firmware_len - offset;
      pkt.payload_len    = (remaining > MAX_PAYLOAD_LEN) ? MAX_PAYLOAD_LEN : remaining;

      memset(pkt.payload, 0, MAX_PAYLOAD_LEN);
      memcpy(pkt.payload, &firmware_payload[offset], pkt.payload_len);

      pkt.crc16 = crc16_data(pkt.payload, pkt.payload_len, 0);

      LOG_INFO("Gonderiliyor: Blok %u/%u (%u byte)\n",
               current_seq_no + 1, total_blocks, pkt.payload_len);

      simple_udp_sendto(&udp_conn, &pkt, sizeof(pkt), &dest_ipaddr);

      etimer_set(&send_timer, TIMEOUT_INTERVAL);

    } else {
      LOG_INFO("Node 1 henuz ulasilabilir degil. Bekleniyor...\n");
      etimer_set(&send_timer, SEND_INTERVAL);
    }
  }

  PROCESS_END();
}
