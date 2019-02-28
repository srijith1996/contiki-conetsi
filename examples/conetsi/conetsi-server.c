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

#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"
/*---------------------------------------------------------------------------*/
static uint8_t current_state;
static uint8_t listen_flag;
static uint8_t yield;
static uint8_t all_flagged;
static uint32_t exp_time_init;
static uint16_t exp_time;
static uint16_t nsi_timeout;

static struct parent_details parent[MAX_PARENT_REQ];
static int count, i;

static char conetsi_data[THRESHOLD_PKT_SIZE];

static struct ctimer idle_timer;
static struct etimer poll_timer;
process_event_t genesis_event;

PROCESS_NAME(oam_collect_process);
PROCESS(backoff_polling_process, "Backoff Polling process");
PROCESS(conetsi_server_process, "CoNetSI server");
AUTOSTART_PROCESSES(&conetsi_server_process,
                    &oam_collect_process,
                    &backoff_polling_process);
/*---------------------------------------------------------------------------*/
static int
ticks(const int time_secs) {
  return time_secs * CLOCK_SECOND;
}
/*---------------------------------------------------------------------------*/
static float
msec(const uint16_t time) {
  return time * 1000.0;
}
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
    parent[count].bytes = demand_pkt->bytes;
    parent[count].flagged = 0;

    /* increment counter before yielding */
    count++;
    PRINTF("Added new parent, count = %d\n", count);
  }
}
/*---------------------------------------------------------------------------*/
void
reset_idle()
{
  printf("Resetting back to IDLE from %d\n", current_state);

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

  memcpy(&conetsi_data, data, datalen);
  struct conetsi_pkt *pkt = (struct conetsi_pkt *) conetsi_data;

  PRINTF("Received a UDP packet from ");
  PRINT6ADDR(sender_addr);
  PRINTF(" --> ");
  PRINT6ADDR(receiver_addr);
  PRINTF("\n");

  PRINTF("State: %d\n", current_state);

  PRINTF("Buffer: ");
  for(i=0; i<datalen; i++) {
    PRINTF("%02x:", *((uint8_t *)pkt + i));
  }
  PRINTF("\n");

  switch(current_state) {

   case STATE_IDLE:
    PRINTF("Checking if potential DA\n");
    PRINTF("Packet type: %d\n", pkt->type);
    if(pkt->type == TYPE_DEMAND_ADVERTISEMENT) {
      /* count and listen_flag are set/reset to 0 everytime the
       * node receives a DA in idle state
       */
      count = 0;
      listen_flag = 0;

      PRINTF("Received DA from ");
      PRINT6ADDR(sender_addr);
      PRINTF("\n");

      add_parent(sender_addr, (void *)pkt->data);
    }
    break;

   case STATE_BACKOFF:
    PRINTF("Pkt type: %d\n", pkt->type);
    if(pkt->type == TYPE_JOIN_REQUEST) {

      PRINTF("Received JR from ");
      PRINT6ADDR(sender_addr);
      PRINTF("\n");

      for(i = 0; i < count; i++) {
        PRINT6ADDR(&parent[i].addr);
        PRINTF("\n");
        PRINT6ADDR(sender_addr);
        PRINTF("\n");
        if(uip_ipaddr_cmp(sender_addr, &parent[i].addr)) {
          parent[i].flagged = 1;
          PRINTF("My parent ");
          PRINT6ADDR(&parent[i].addr);
          PRINTF(" chose ");
          PRINT6ADDR(&(((struct join_request *)&pkt->data)->chosen_child));
          PRINTF(":(  \n");
          break;
        }
      }
    } else if(pkt->type == TYPE_DEMAND_ADVERTISEMENT) {
      /* if listen_flag = 1, the demand adv is ignored without checking
       * the value of count. Only if it is zero will the count be checked
       */
      PRINTF("Received another DA from ");
      PRINT6ADDR(sender_addr);
      PRINTF("\n");

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

      PRINTF("Received ACK from ");
      PRINT6ADDR(sender_addr);
      PRINTF("\n");

    } else if(pkt->type == TYPE_NSI) {
      /* send dummy join request */
      set_child(sender_addr);
      send_join_req(clock_seconds() - exp_time_init);
      goto parse_nsi;
    }
      
    break;

   case STATE_CHILD_CHOSEN:
    /* timeout should result in sending nsi to parent */
    if(pkt->type == TYPE_NSI) {
parse_nsi:
      PRINTF("Preparing NSI...\n");
      ctimer_stop(&idle_timer);
      send_nsi((void *)&conetsi_data, datalen);
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
      if(msec(exp_time) < THRESHOLD_TIME_MSEC) {
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
    ctimer_set(&idle_timer, ticks(1), reset_idle, NULL);

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
      printf("Genesis event triggered\n");
      printf("Current state: %d\n", current_state);
      if(current_state == STATE_IDLE) {
        send_demand_adv(&data);

        /* set my parent as NULL */
        set_parent(NULL);
        nsi_timeout = get_nsi_timeout();
        printf("Timeout: %d\n", nsi_timeout);

        current_state = STATE_DEMAND_ADVERTISED;
        ctimer_set(&idle_timer, ticks(nsi_timeout), reset_idle, NULL);
        exp_time_init = clock_seconds();
      }
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/* This process is called by the node when in idle state,
 * on receiving a demand advertisement. The same thread, executes a
 * continuous while loop which exits only when chosen by one of the
 * added parents, or when rejected by all parents
 */
PROCESS_THREAD(backoff_polling_process, ev, data)
{
  PROCESS_BEGIN();

   /* TODO: ACK should be sent to multiple parent */

  /* Change the context to conetsi process to avoid blocking
   * the UDP callback process */ PRINTF("Entered new thread\n"); while(1) {
    /* recompute yield */
    etimer_set(&poll_timer, CLOCK_SECOND / 10);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&poll_timer));

    yield = 1;
    all_flagged = 0;

    for(i = 0; i < count; i++) {
      PRINTF("Parent ");
      PRINT6ADDR(&parent[i].addr);
      PRINTF(" flagged? %d\n", parent[i].flagged);

      PRINTF("Parent start time: %d\n", parent[i].start_time);
      PRINTF("Parent backoff: %d/%d\n", parent[i].backoff * 1000, 1000);
      PRINTF("Time left: %d\n", (clock_seconds() - parent[i].start_time
              - parent[i].backoff/CLOCK_SECOND));

      yield &= (parent[i].flagged ||
                (clock_seconds() * CLOCK_SECOND <
            parent[i].start_time * CLOCK_SECOND + parent[i].backoff));
      if(i == 0) {
        all_flagged = parent[i].flagged;
      }
      all_flagged &= parent[i].flagged;
      if(!yield) {
        break;
      }
    }

    /* If I wasn't chosen by anyone -- break */
    if(all_flagged) {
      PRINTF("All parents flagged, resetting to IDLE\n");
      current_state = STATE_IDLE;
      break;
    }

    if(!yield && current_state == STATE_BACKOFF) {
      /* path terminates here */
      if(parent[i].bytes >= MARGINAL_PKT_SIZE) {
        /* now i indexes the first entry which expired */
        PRINTF("Packet will bloat if continued. Sending NSI\n");
        set_parent(&(parent[i].addr));
        send_nsi(NULL, 0);
        current_state = STATE_IDLE;
      } else {
        PRINTF("Backoff over. Sending ACK\n");
        send_ack(&(parent[i].addr));
        current_state = STATE_AWAITING_JOIN_REQ;
        ctimer_set(&idle_timer, AWAITING_JOIN_REQ_IDLE_TIMEOUT,
                   reset_idle, NULL);
      }
      count = 0;
    }
  }
  PRINTF("Exiting backoff\n");
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
