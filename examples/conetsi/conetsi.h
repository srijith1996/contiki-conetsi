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
/*---------------------------------------------------------------------------*/
/* packet type defines */
#define TYPE_DEMAND_ADVERTISEMENT 0
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
#define MAX_EMPTY_HOPS        2
/*---------------------------------------------------------------------------*/
/* Configure how conservative the nodes should
 * be about the delay incurred in transmission
 * and queues, above and beyond the estimated
 * delay.
 */
#define DELAY_GUARD_TIME_RTICKS    RTIMER_SECOND / 10
/*---------------------------------------------------------------------------*/
/* packet structs */
#define MAX_CHILDREN 3
struct conetsi_node {
  uip_ipaddr_t parent_node;
  uip_ipaddr_t child_node[MAX_CHILDREN];
};

struct parent_details {
  uip_ipaddr_t addr;
  uint32_t start_time;
  uint32_t timeout;
  uint32_t backoff;
  uint16_t prev_demand;    /* to speed up backoff process; avoids recalc
                              this is important, since randomness is
                              added to the backoff */
  uint16_t demand;
  uint16_t bytes;
  uint8_t empty_hops;
  uint8_t flagged;
  uint8_t pad;
};

struct conetsi_pkt {
  uint8_t type;
  char data[THRESHOLD_PKT_SIZE];
};
  
struct nsi_demand {
  uip_ipaddr_t parent_addr;
  uint16_t time_left;
  uint8_t demand;
  uint8_t bytes;
  uint8_t empty_hops;     /* count of empty nodes in path */
  uint8_t pad[3];
};

struct nsi_forward {
  uip_ipaddr_t to;
  char data[THRESHOLD_PKT_SIZE];
};

#define SIZE_DA (sizeof(struct nsi_demand) - 3)
/*---------------------------------------------------------------------------*/
/* macros for conversion */
#define msec2ticks(x) ((uint16_t)(((uint64_t)x * CLOCK_SECOND) / 1000))
#define ticks2msec(x) ((uint16_t)(((uint64_t)x * 1000) / CLOCK_SECOND))
#define msec2rticks(x) (((uint64_t)x * RTIMER_SECOND) / 1000)
#define rticks2msec(x) ((uint16_t)(((uint64_t)x * 1000) / RTIMER_SECOND))
#define ticks2rticks(x) (((uint64_t)x * RTIMER_SECOND) \
                          / CLOCK_SECOND)
#define rticks2ticks(x) ((uint16_t)(((uint64_t)x * CLOCK_SECOND) \
                          / RTIMER_SECOND))

/* functions used by processes */
int send_nsi(const uint8_t *buf, int buf_len);

/* Register Multicast address for CoNeStI */
void reg_mcast_addr();

/* Control message functions */
/* Handshake step 1: Send multicast demand advertisement */
uint32_t send_demand_adv(struct parent_details *parent);

/* Get backoff time and demand */
uint32_t get_backoff(uint16_t demand, uint32_t timeout_rticks);

/* Get and set parent and child */
void set_parent(const uip_ipaddr_t *p);
uip_ipaddr_t *get_parent();
void add_child(const uip_ipaddr_t *c);
void rm_child(const uip_ipaddr_t *c);
int child_count(void);

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
