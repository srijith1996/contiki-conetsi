/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "net/ipv6/simple-udp.h"
#include "sys/clock.h"
#include "sys/log.h"

#include "conetsi.h"
/*---------------------------------------------------------------------------*/
#define UDP_SERVER_PORT 3005
#define UDP_CLIENT_PORT 3005

static struct uip_ipaddr_t mcast_addr;

static int current_state;
static unsigned long start_time;
static int count;
static int listen_flag;

static uint16_t backoff[MAX_PARENT_REQ];
static uint16_t start_times[MAX_PARENT_REQ];
static uint16_t necessity[MAX_PARENT_REQ];

static struct ctimer genesis_timer;

PROCESS(conetsi_server_process, "CoNetSI server");
AUTOSTART_PROCESSES(&conetsi_server_process);
/*---------------------------------------------------------------------------*/
void
goto_backoff(struct nsi_demand *demand_pkt)
{
  necessity[count] = necessity(demand_pkt);
  if(necessity[count] > THRESHOLD_NECESSITY &&
     demand_pkt->time_left > THRESHOLD_TIME_USEC) {

    /* compute backoff */
    backoff[count] = get_backoff(necessity[count], time_left);

    /* wait for T_backoff */
    current_state = STATE_BACKOFF;
    start_time[count] = clock_seconds();
    PROCESS_YIELD_UNTIL(clock_seconds() == start_time[count] + backoff[count]);

    if(current_state == STATE_BACKOFF) {
      /* path terminates here */
      if(demand_pkt->bytes_left >= MARGINAL_PKT_SIZE) {
        send_nsi(NULL, 0);
        ctimer_restart(&genesis_timer);
        current_state = STATE_IDLE;
      } else {
        send_ack(sender_addr);
        current_state = STATE_AWAITING_JOIN_REQ;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

/* inclusion of a state space may make more sense */
/* switch case can be used for all states */

  struct conetsi_pkt *pkt = (struct conetsi_pkt *) data;

  switch(current_state) {

   case STATE_IDLE:
    if(pkt->type == TYPE_DEMAND_ADVERTISEMENT) {
      /* count and listen_flag are set/reset to 0 everytime the
       * node receives a DA in idle state
       */
      count = 0;
      listen_flag = 0;

      goto_backoff(pkt->data);
    }
    break;

   case STATE_BACKOFF:
    if(pkt->type == TYPE_JOIN_REQUEST &&
       uip_ipaddr_cmp(&((struct join_request *)pkt->data)->sender_addr,
                      &my->parent_node)) {
      /* reset back to idle state, since I am not chosen */
      current_state = STATE_IDLE;
      
    } else if(pkt->type == TYPE_DEMAND_ADVERTISEMENT) {
      /* if listen_flag = 1, the demand adv is ignored without checking
       * the value of ++count. Only if it is zero will the count be checked
       * and incremented
       */
      if(listen_flag || ++count >= MAX_PARENT_REQ) {
        printf("Ignoring further Demand advertisements");
        listen_flag = 1;
      } else {
        goto_backoff(pkt->data);
      }
    }
    break;

   case STATE_DEMAND_ADVERTISED:
    if(pkt->type == TYPE_ACK) {
      /* prepare and send join step 3 packet */
      set_child(sender_addr);
      send_join_req(sender_addr);
      current_state = STATE_CHILD_CHOSEN;
    }
    break;

   case STATE_CHILD_CHOSEN:
    /* timeout should result in sending nsi to parent */
    if(pkt->type == TYPE_NSI) {
      send_nsi(pkt->data, datalen-2);
      ctimer_restart(&genesis_timer);
      current_state = STATE_IDLE;
    }
    break;

   case STATE_AWAITING_JOIN_REQ:
    if(pkt->type == TYPE_JOIN_REQUEST) {
      set_parent(sender_addr);

      /* path length will not run out here due to additional
       * condition taken care of in STATE_IDLE case
       */
      if(/* timer is "about to expire" */) {
        send_nsi(NULL, 0);
        ctimer_restart(&genesis_timer);
        current_state = STATE_IDLE;
      } else {
        send_demand_adv();
        current_state = STATE_DEMAND_ADVERTISED;
      }
    }
    break;

   default:
    PRINTF("Error in CoNeStI: reached an unknown state\n");

  }
  return;
}
/*---------------------------------------------------------------------------*/
void
gen_timer_callback()
{
  /* TODO: Add logic */
  if(current_state == STATE_IDLE) {
    send_demand_adv();
    current_state = STATE_DEMAND_ADVERTISED;
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(conetsi_server_process, ev, data)
{
  PROCESS_BEGIN();

  current_state = STATE_IDLE;

  /* register multicast address for destination advertisement */
  uip_ip6addr(&mcast_addr, 0xff01, 0, 0, 0, 0, 0, 0, 0x0002);
  reg_mcast_addr(&mcast_addr);
  
  /* setup timers */
  ctimer_set(&genesis_timer, GENESIS_TIMEOUT,
             gen_timer_callback, NULL);

  while(1) {
    PROCESS_YEILD();

    /* OAM process will wake us up when it is relevant to
     * send OAM data
     */
    if(ev == genesis_event) {
      /* TODO: Add logic */

      if(current_state == STATE_IDLE) {
        
      }
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
