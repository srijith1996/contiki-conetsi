/*---------------------------------------------------------------------------*/
#ifndef _CONETSI_H_
#define _CONETSI_H_
/*---------------------------------------------------------------------------*/
#include "contiki-net.h"
/*---------------------------------------------------------------------------*/
/* state space defines */
#define STATE_IDLE              0
#define STATE_DEMAND_ADVERTISED 1
#define STATE_CHILD_CHOSEN      2
#define STATE_AWAITING_JOIN_REQ 3
#define STATE_JOINED            4
/*---------------------------------------------------------------------------*/
#define TYPE_DEMAND_ADVERTISEMENT 0
#define TYPE_ACK                  1
#define TYPE_JOIN_REQUEST         2
#define TYPE_NSI                  3
/*---------------------------------------------------------------------------*/
#define THRESHOLD_TIME_USEC      50
#define THRESHOLD_NECESSITY      50
#define THRESHOLD_PATH_LEN        5
#define THRESHOLD_PKT_SIZE       90
#define MARGINAL_PKT_SIZE        80

#define MAX_PARENT_REQ            5
/*---------------------------------------------------------------------------*/
/* packet structs */
struct conetsi_node {
  uip_ipaddr_t parent_node;
  uip_ipaddr_t child_node;
};

struct parent_details {
  uip_ipaddr_t addr;
  uint16_t necessity;
  uint32_t start_time;
  uint16_t backoff;
  uint8_t flagged;
};

struct conetsi_pkt {
  uint16_t type;
  char *data;
};
  
struct nsi_demand {
  uint16_t demand;
  uint16_t time_left;
  uint16_t bytes_left;
};

#define SIZE_DA        8
#define SIZE_ACK       2
#define SIZE_JOIN_REQ 18

struct join_request {
  uip_ipaddr_t *chosen_child;
};

struct nsi_ack {

};
/*---------------------------------------------------------------------------*/
/* functions used by processes */
void send_nsi(char *buf, int buf_len);

/* Register Multicast address for CoNeStI */
void reg_mcast_addr();

/* Control message functions */
/* Handshake step 1: Send multicast demand advertisement */
int send_demand_adv();

/* Handshake step 2: Send unicast ack to parent */
void send_ack(struct uip_ipaddr_t *parent);

/* Handshake step 3: Send multicast join request */
void send_join_req(struct uip_ipaddr_t *child);

void reset_timers();

int get_time_left(state);

/* Get backoff time and necessity factor */
int get_backoff(int necessity, int time_left);
int necessity(struct nsi_demand *demand_pkt);

/* Get and set parent and child */
void set_parent(struct uip_ipaddr_t *p);
struct uip_ipaddr_t *get_parent();
void set_shild(struct uip_ipaddr_t *c);
struct uip_ipaddr_t *get_child();

/*---------------------------------------------------------------------------*/
#endif /* _CONETSI_H_ */
/*---------------------------------------------------------------------------*/
