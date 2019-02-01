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
/* packet structs */
struct nsi_demand {
  
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
