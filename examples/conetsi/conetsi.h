/*---------------------------------------------------------------------------*/
#ifndef _CONETSI_H_
#define _CONETSI_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "uip.h"

#include "oam.h"
/*---------------------------------------------------------------------------*/
#define NTOHS(x)  (x = uip_ntohs(x))
#define HTONS(x)  (x = uip_htons(x))
/*---------------------------------------------------------------------------*/
#define UDP_SERVER_PORT 3005
#define UDP_CLIENT_PORT 3005
/*---------------------------------------------------------------------------*/
/* state space defines */
#define STATE_IDLE              0
#define STATE_BACKOFF           1
#define STATE_DEMAND_ADVERTISED 2
#define STATE_CHILD_CHOSEN      3
#define STATE_AWAITING_JOIN_REQ 4
/*---------------------------------------------------------------------------*/
/* packet type defines */
#define TYPE_DEMAND_ADVERTISEMENT 0
#define TYPE_ACK                  1
#define TYPE_JOIN_REQUEST         2
#define TYPE_NSI                  3
/*---------------------------------------------------------------------------*/
/* thresholds */
#define THRESHOLD_PATH_LEN    5
#define MARGINAL_PKT_SIZE (((THRESHOLD_PATH_LEN - 1) * THRESHOLD_PKT_SIZE) /\
                           THRESHOLD_PATH_LEN)

#define BACKOFF_DIV_FACTOR    THRESHOLD_PATH_LEN * 3
#define BACKOFF_RAND_RANGE    10
#define BACKOFF_POLL_INTERVAL 5

#define MAX_PARENT_REQ        5
/*---------------------------------------------------------------------------*/
#define AWAITING_JOIN_REQ_IDLE_TIMEOUT  (CLOCK_SECOND * 5)

/* Configure how conservative the nodes should
 * be about the delay incurred in transmission
 * and queues, above and beyond the estimated
 * delay.
 */
#define DELAY_GUARD_TIME_RTICKS    RTIMER_SECOND / 10
/*---------------------------------------------------------------------------*/
/* packet structs */
struct conetsi_node {
  uip_ipaddr_t parent_node;
  uip_ipaddr_t child_node;
};

struct parent_details {
  uip_ipaddr_t addr;
  uint32_t start_time;
  uint32_t timeout;
  uint32_t backoff;
  uint16_t demand;
  uint16_t bytes;
  uint8_t flagged;
};

struct conetsi_pkt {
  uint8_t type;
  char data[THRESHOLD_PKT_SIZE];
};
  
struct nsi_demand {
  uint16_t demand;
  uint16_t time_left;
  uint16_t bytes;
};

struct join_request {
  uip_ipaddr_t chosen_child;
  uint16_t time_left;
  uint16_t pad;
};

#define SIZE_DA        7
#define SIZE_ACK       1
#define SIZE_JOIN_REQ 19
/*---------------------------------------------------------------------------*/
/* macros for conversion */
#define msec2ticks(x) ((x * CLOCK_SECOND) / 1000)
#define ticks2msec(x) ((x * 1000) / CLOCK_SECOND)
#define msec2rticks(x) ((x * RTIMER_SECOND) / 1000)
#define rticks2msec(x) ((x * 1000) / RTIMER_SECOND)

/* TODO: Possible overflow here */
#define ticks2rticks(x) ((x * RTIMER_SECOND) / CLOCK_SECOND)
#define rticks2ticks(x) ((x * CLOCK_SECOND) / RTIMER_SECOND)

/* functions used by processes */
int send_nsi(const uint8_t *buf, int buf_len);

/* Register Multicast address for CoNeStI */
void reg_mcast_addr();

/* Control message functions */
/* Handshake step 1: Send multicast demand advertisement */
int send_demand_adv(struct parent_details *parent);

/* Handshake step 2: Send unicast ack to parent */
int send_ack(const uip_ipaddr_t *parent);

/* Handshake step 3: Send multicast join request */
int send_join_req(uint32_t timeout);
int my_join_req(void *pkt);

/* Get backoff time and demand */
uint32_t get_backoff(uint16_t demand, uint32_t timeout_rticks);

/* Get and set parent and child */
void set_parent(const uip_ipaddr_t *p);
uip_ipaddr_t *get_parent();
void set_child(const uip_ipaddr_t *c);
uip_ipaddr_t *get_child();

/* callback for handling UDP */
void udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen);
/*---------------------------------------------------------------------------*/
#endif /* _CONETSI_H_ */
/*---------------------------------------------------------------------------*/
