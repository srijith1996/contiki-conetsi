/*---------------------------------------------------------------------------*/
#ifndef _CONETSI_H_
#define _CONETSI_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "uip.h"

#include "oam.h"
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
#define TYPE_DEMAND_ADVERTISEMENT 0
#define TYPE_ACK                  1
#define TYPE_JOIN_REQUEST         2
#define TYPE_NSI                  3
/*---------------------------------------------------------------------------*/
#define THRESHOLD_TIME_MSEC      20
#define THRESHOLD_DEMAND         50
#define THRESHOLD_PATH_LEN        5
#define THRESHOLD_PKT_SIZE       90
#define MARGINAL_PKT_SIZE        80

/* purely motivated by the fact that communation bit rate is 5000kbps */
#define BACKOFF_FACTOR          (0.1/5000)

#define MAX_PARENT_REQ            5
/*---------------------------------------------------------------------------*/
#define AWAITING_JOIN_REQ_IDLE_TIMEOUT  (CLOCK_SECOND * 5)
/*---------------------------------------------------------------------------*/
/* packet structs */
struct conetsi_node {
  uip_ipaddr_t parent_node;
  uip_ipaddr_t child_node;
};

struct parent_details {
  uip_ipaddr_t addr;
  uint32_t start_time;
  uint16_t backoff;
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
};

#define SIZE_DA        8
#define SIZE_ACK       2
#define SIZE_JOIN_REQ 20
/*---------------------------------------------------------------------------*/
/* functions used by processes */
int send_nsi(char *buf, int buf_len);

/* Register Multicast address for CoNeStI */
void reg_mcast_addr();

/* Control message functions */
/* Handshake step 1: Send multicast demand advertisement */
int send_demand_adv();

/* Handshake step 2: Send unicast ack to parent */
int send_ack(const uip_ipaddr_t *parent);

/* Handshake step 3: Send multicast join request */
int send_join_req(int exp_time);

/* Get backoff time and demand */
float get_backoff(int demand, int time_left);

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
