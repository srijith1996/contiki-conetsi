/*---------------------------------------------------------------------------*/
#include <stdio.h>

#include "contiki.h"
#include "contiki-net.h"
#include "contiki-lib.h"
#include "net/ipv6/simple-udp.h"
#include "sys/clock.h"
#include "sys/log.h"

#include "conetsi.h"
#include "oam.h"
/*---------------------------------------------------------------------------*/
static uint8_t current_state;
static uint8_t listen_flag;
static uint8_t yield;
static uint8_t all_flagged;
static uint32_t exp_time_init;
static uint16_t exp_time;

static struct parent_details parent[MAX_PARENT_REQ];
static int count, i;

static struct ctimer idle_timer;
process_event_t genesis_event;

PROCESS_NAME(oam_collect_process);
PROCESS(conetsi_server_process, "CoNetSI server");
AUTOSTART_PROCESSES(&conetsi_server_process, &oam_collect_process);
/*---------------------------------------------------------------------------*/
static void
add_parent(const uip_ipaddr_t *sender, struct nsi_demand *demand_pkt)
{
  if(demand_pkt->demand > THRESHOLD_DEMAND &&
     demand_pkt->time_left > THRESHOLD_TIME_MSEC) {

    current_state = STATE_BACKOFF;

    /* store essential information */
    uip_ipaddr_copy(&parent[count].addr, sender);
    parent[count].start_time = clock_seconds();
    parent[count].backoff = get_backoff(demand(), demand_pkt->time_left);
    parent[count].flagged = 0;

    /* increment counter before yielding */
    count++;
  }
}
/*---------------------------------------------------------------------------*/
void
reset_idle()
{

  switch(current_state) {

   case STATE_DEMAND_ADVERTISED:
   case STATE_CHILD_CHOSEN:
    send_nsi(NULL, 0);
    break;

   case STATE_IDLE:
   case STATE_BACKOFF:
   case STATE_AWAITING_JOIN_REQ:
   default:
    break;
  }
  current_state = STATE_IDLE;
}
/*---------------------------------------------------------------------------*/
/* This function is called by the node when in idle state,
 * on receiving a demand advertisement. The same thread, executes a
 * continuous while loop which exits only when chosen by one of the
 * added parents, or when rejected by all parents
 */
static void
goto_backoff(const uip_ipaddr_t *sender, struct nsi_demand *demand_pkt)
{
  yield = 0;
  while(!yield) {
    /* recompute yield */
    yield = 1;
    all_flagged = 1;

    for(i = 0; i < count; i++) {
      yield &= (parent[i].flagged ||
                (clock_seconds() < parent[i].start_time + parent[i].backoff));
      all_flagged &= parent[i].flagged;
      if(!yield) {
        break;
      }
    }

    /* If I wasn't chosen by anyone -- break */
    if(all_flagged) {
      current_state = STATE_IDLE;
      break;
    }

    if(!yield && current_state == STATE_BACKOFF) {
      /* path terminates here */
      if(demand_pkt->bytes >= MARGINAL_PKT_SIZE) {
        /* now i indexes the first entry which expired */
        set_parent(&(parent[i].addr));
        send_nsi(NULL, 0);
        current_state = STATE_IDLE;
      } else {
        send_ack(sender);
        current_state = STATE_AWAITING_JOIN_REQ;
        ctimer_set(&idle_timer, AWAITING_JOIN_REQ_IDLE_TIMEOUT,
                   reset_idle, NULL);
      }
      count = 0;
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

      add_parent(sender_addr, (void *)pkt->data);
      goto_backoff(sender_addr, (void *)pkt->data);
    }
    break;

   case STATE_BACKOFF:
    if(pkt->type == TYPE_JOIN_REQUEST) {
      for(i = 0; i < count; i++) {
        if(uip_ipaddr_cmp(&sender_addr, &parent[i].addr)) {
          parent[i].flagged = 1;
          break;
        }
      }
    } else if(pkt->type == TYPE_DEMAND_ADVERTISEMENT) {
      /* if listen_flag = 1, the demand adv is ignored without checking
       * the value of count. Only if it is zero will the count be checked
       */
      if(listen_flag || count >= MAX_PARENT_REQ) {
        printf("Ignoring further Demand advertisements");
        listen_flag = 1;
      } else {
        add_parent(sender_addr, (void *)pkt->data);
      }
    }
    break;

   case STATE_DEMAND_ADVERTISED:
    if(pkt->type == TYPE_ACK) {
      /* prepare and send join step 3 packet */
      set_child(sender_addr);
      send_join_req(clock_seconds() - exp_time_init);
      current_state = STATE_CHILD_CHOSEN;
    }
    break;

   case STATE_CHILD_CHOSEN:
    /* timeout should result in sending nsi to parent */
    if(pkt->type == TYPE_NSI) {
      ctimer_stop(&idle_timer);
      send_nsi(data, datalen);
      current_state = STATE_IDLE;
    }
    break;

   case STATE_AWAITING_JOIN_REQ:
    if(pkt->type == TYPE_JOIN_REQUEST) {
      ctimer_stop(&idle_timer);
      set_parent(sender_addr);

      /* path length will not run out here due to additional
       * condition taken care of in STATE_IDLE case
       */
      exp_time = ((struct join_request *)(pkt->data))->time_left;
      if(exp_time < THRESHOLD_TIME_MSEC) {
        send_nsi(NULL, 0);
        current_state = STATE_IDLE;
      } else {
        send_demand_adv();
        current_state = STATE_DEMAND_ADVERTISED;

        /* start count-down and record current ticks */
        ctimer_set(&idle_timer, exp_time, reset_idle, NULL);
        exp_time_init = clock_seconds();
      }
    }
    break;

   default:
    printf("Error in CoNeStI: reached an unknown state\n");
    ctimer_set(&idle_timer, (2 * CLOCK_SECOND),
               reset_idle, NULL);

  }
  return;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(conetsi_server_process, ev, data)
{
  PROCESS_BEGIN();

  current_state = STATE_IDLE;
  genesis_event = process_alloc_event();

  /* register multicast address for destination advertisement */
  reg_mcast_addr();
  
  while(1) {
    PROCESS_YIELD();

    /* OAM process will wake us up when it is relevant to
     * send OAM data
     */
    if(ev == genesis_event) {
      if(current_state == STATE_IDLE) {
        send_demand_adv(&data);

        /* set my parent as NULL */
        set_parent(NULL);
        current_state = STATE_DEMAND_ADVERTISED;
        ctimer_set(&idle_timer, get_nsi_timeout(),
                   reset_idle, NULL);
        exp_time_init = clock_seconds();
      }
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
