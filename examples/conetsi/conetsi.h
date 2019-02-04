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

#define MAX_PARENT_REQ            5
/*---------------------------------------------------------------------------*/
/* packet structs */
struct conetsi_node {
  uip_ipaddr_t parent_node;
  uip_ipaddr_t child_node;
};

struct conetsi_pkt {
  uint16_t type;
  char *data;
};
  
struct nsi_demand {
  uint16_t demand;
  uint16_t time_left;
  uint16_t path_len;
};

struct join_request {

};

struct nsi_ack {

};
/*---------------------------------------------------------------------------*/
/* functions used by processes */
void send_nsi();

void add_my_nsi(char *nsi_buf, int nsi_buf_len);

int get_time_left(state);

int adv_demand();

void send_ack(struct simple_udp_connection *c, int backoff_secs);

void send_join_req(struct simple_udp_connection *c);

void reset_timers();
/*---------------------------------------------------------------------------*/
#endif /* _CONETSI_H_ */
/*---------------------------------------------------------------------------*/
