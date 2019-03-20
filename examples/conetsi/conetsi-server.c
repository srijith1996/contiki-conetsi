/*---------------------------------------------------------------------------*/
#include <stdio.h>

#include "contiki.h"
#include "contiki-net.h"
#include "contiki-lib.h"
#include "net/ipv6/simple-udp.h"
#include "sys/clock.h"

#include "conetsi.h"
#include "oam.h"

#include "sys/log.h"
#define LOG_MODULE "CoNetSI"
#define LOG_LEVEL LOG_LEVEL_CONETSI
/*---------------------------------------------------------------------------*/
static uint8_t current_state;
static uint8_t listen_flag, yield, all_flagged;
static uint32_t exp_time;
static uint32_t init_exp_time;
static uint8_t my_parent_id;

static struct parent_details parent[MAX_PARENT_REQ];
static int count, i, prev_demand;

static char conetsi_data[THRESHOLD_PKT_SIZE];

static struct ctimer idle_timer;
static struct etimer bo_poll_timer;
process_event_t genesis_event;

PROCESS_NAME(oam_collect_process);
PROCESS(backoff_polling_process, "Backoff Polling process");
PROCESS(conetsi_server_process, "CoNetSI server");
AUTOSTART_PROCESSES(&conetsi_server_process,
                    &oam_collect_process,
                    &backoff_polling_process);
/*---------------------------------------------------------------------------*/
static void
add_parent(const uip_ipaddr_t *sender, struct nsi_demand *demand_pkt)
{

  /* change byte order */
  NTOHS(demand_pkt->demand);
  NTOHS(demand_pkt->time_left);
  NTOHS(demand_pkt->bytes);
  
  if(demand_pkt->demand > THRESHOLD_DEMAND &&
     demand_pkt->time_left > THRESHOLD_TIMEOUT_MSEC) {

    current_state = STATE_BACKOFF;

    /* store essential information */
    uip_ipaddr_copy(&parent[count].addr, sender);

    /* both backoff and start time in ticks */
    parent[count].flagged = 0;
    parent[count].start_time = RTIMER_NOW();
    parent[count].timeout = msec2rticks(demand_pkt->time_left);
    parent[count].demand = demand_pkt->demand;
    parent[count].bytes = demand_pkt->bytes;
    parent[count].backoff = get_backoff(demand(), parent[count].timeout);

    /* increment counter before yielding */
    count++;
    LOG_INFO("Added new parent, count = %d\n", count);
  }
}
/*---------------------------------------------------------------------------*/
void
reset_idle()
{
  LOG_DBG("Resetting back to IDLE from %d\n", current_state);

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
  my_parent_id = MAX_PARENT_REQ;
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

  LOG_INFO("Received a Conetsi packet from ");
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_(" --> ");
  LOG_INFO_6ADDR(receiver_addr);
  LOG_INFO_(" (State: %d)\n", current_state);

  LOG_DBG("Buffer: ");
  for(i=0; i<datalen; i++) {
    LOG_DBG_("%02x:", *((uint8_t *)pkt + i));
  }
  LOG_DBG_("\n");

  LOG_DBG("Packet type: %d\n", pkt->type);

  switch(current_state) {

   case STATE_IDLE:
    LOG_DBG("Checking if potential DA\n");
    if(pkt->type == TYPE_DEMAND_ADVERTISEMENT) {
      /* count and listen_flag are set/reset to 0 everytime the
       * node receives a DA in idle state
       */
      count = 0;
      listen_flag = 0;

      LOG_INFO("Received DA from ");
      LOG_INFO_6ADDR(sender_addr);
      LOG_INFO_("\n");

      add_parent(sender_addr, (void *)pkt->data);

    /* while idling forward any stray NSI */
    } else if(pkt->type == TYPE_NSI) {
      set_parent(NULL);
      send_nsi((void *)&conetsi_data, datalen);
    }
    break;

   case STATE_BACKOFF:
    if(pkt->type == TYPE_JOIN_REQUEST) {

      LOG_INFO("Received JR from ");
      LOG_INFO_6ADDR(sender_addr);
      LOG_INFO_("\n");

      for(i = 0; i < count; i++) {
        if(uip_ipaddr_cmp(sender_addr, &parent[i].addr)) {
          parent[i].flagged = 1;
          LOG_INFO("My parent ");
          LOG_INFO_6ADDR(&parent[i].addr);
          LOG_INFO_(" chose ");
          LOG_INFO_6ADDR(&(((struct join_request *)&pkt->data)->chosen_child));
          LOG_INFO_("  :(  \n");
          break;
        }
      }
    } else if(pkt->type == TYPE_DEMAND_ADVERTISEMENT) {
      /* if listen_flag = 1, the demand adv is ignored without checking
       * the value of count. Only if it is zero will the count be checked
       */
      LOG_INFO("Received another DA from ");
      LOG_INFO_6ADDR(sender_addr);
      LOG_INFO_("\n");

      if(listen_flag || count >= MAX_PARENT_REQ) {
        LOG_DBG("Ignoring further Demand advertisements");
        listen_flag = 1;
      } else {
        add_parent(sender_addr, (void *)pkt->data);
      }
    }
    break;

   case STATE_DEMAND_ADVERTISED:
    if(pkt->type != TYPE_ACK && pkt->type != TYPE_NSI) {
      break;
    }

    LOG_INFO("Received ACK/NSI from ");
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("\n");

    if(pkt->type == TYPE_ACK
        && rticks2ticks(init_exp_time + exp_time - RTIMER_NOW())
            <= THRESHOLD_TIMEOUT_TICKS) {
      ctimer_stop(&idle_timer);
      reset_idle();
      break;
    }

    /* prepare and send join step 3 packet */
    set_child(sender_addr);
    send_join_req(init_exp_time + exp_time - RTIMER_NOW());

    if(pkt->type == TYPE_ACK) {
      current_state = STATE_CHILD_CHOSEN;
    } else if(pkt->type == TYPE_NSI) {
      goto parse_nsi;
    }

    break;

   case STATE_CHILD_CHOSEN:
    /* timeout should result in sending nsi to parent */
    if(pkt->type == TYPE_NSI) {
parse_nsi:
      LOG_DBG("Preparing NSI...\n");
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
      struct join_request *pkt_data = (struct join_request *)(pkt->data);
      NTOHS(pkt_data->time_left);

      init_exp_time = RTIMER_NOW();
      exp_time = msec2rticks(pkt_data->time_left);
      LOG_DBG("Timeout in JREQ: %d\n", exp_time);

      if(rticks2ticks(exp_time) < THRESHOLD_TIMEOUT_TICKS) {
        LOG_DBG("Timeout threshold reached\n");
        send_nsi(NULL, 0);
        current_state = STATE_IDLE;
      } else {
        parent[my_parent_id].timeout = exp_time;
        send_demand_adv(&parent[my_parent_id]);
        current_state = STATE_DEMAND_ADVERTISED;

        /* start count-down */
        ctimer_set(&idle_timer, rticks2ticks(exp_time), reset_idle, NULL);
      }
    }
    break;

   default:
    LOG_WARN("Error in CoNeStI: reached an unknown state\n");
    ctimer_set(&idle_timer, CLOCK_SECOND, reset_idle, NULL);

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
      LOG_INFO("Genesis event triggered (Current state: %d)\n", current_state);
      if(current_state == STATE_IDLE) {
        /* set my parent as NULL */
        set_parent(NULL);

        if(get_bytes() >= MARGINAL_PKT_SIZE) {
          send_nsi(NULL, 0);
        } else {
          send_demand_adv(NULL);

          init_exp_time = RTIMER_NOW();
          exp_time = ticks2rticks(get_nsi_timeout());
          LOG_INFO("Timeout: %d\n", exp_time);

          current_state = STATE_DEMAND_ADVERTISED;
          ctimer_set(&idle_timer, rticks2ticks(exp_time), reset_idle, NULL);
        }

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

  /* invalidate prev_demand */
  prev_demand = -1;

  /* Change the context to conetsi process to avoid blocking
   * the UDP callback process */
  while(1) {
    /* recompute yield */
    etimer_set(&bo_poll_timer, BACKOFF_POLL_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&bo_poll_timer));

    yield = 1;
    all_flagged = 0;

    for(i = 0; i < count; i++) {
      LOG_DBG("Parent ");
      LOG_DBG_6ADDR(&parent[i].addr);
      LOG_DBG_(" flagged? %d\n", parent[i].flagged);

      LOG_DBG("Parent start time: %ld\n", parent[i].start_time);

      if(demand() != prev_demand) {
        prev_demand = demand();
        parent[i].backoff = get_backoff(prev_demand, parent[i].timeout);
        LOG_DBG("Parent backoff: %d\n", parent[i].backoff);
        LOG_DBG("Time now: %ld\n", RTIMER_NOW());
        LOG_DBG("Time left: %ld\n", (parent[i].start_time
                                 + parent[i].backoff - RTIMER_NOW()));
      }

      yield &= (parent[i].flagged || (RTIMER_NOW() <
                    parent[i].start_time + parent[i].backoff));
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
      LOG_DBG("All parents flagged, resetting to IDLE\n");
      current_state = STATE_IDLE;
      break;
    }

    if(!yield && current_state == STATE_BACKOFF) {
      /* path terminates here */
      if(parent[i].bytes >= MARGINAL_PKT_SIZE) {
        /* now i indexes the first entry which expired */
        LOG_INFO("Packet will bloat if continued. Sending NSI\n");
        set_parent(&(parent[i].addr));

        send_nsi(NULL, 0);
        current_state = STATE_IDLE;
      } else {
        LOG_DBG("Backoff over. Sending ACK\n");

        /* TODO: will need change if multiple parents get acked */
        send_ack(&(parent[i].addr));
        my_parent_id = i;

        current_state = STATE_AWAITING_JOIN_REQ;
        ctimer_set(&idle_timer, AWAITING_JOIN_REQ_IDLE_TIMEOUT,
                   reset_idle, NULL);
      }
      count = 0;

      /* invalidate prev_demand */
      prev_demand = -1;
    }
  }
  LOG_DBG("Exiting backoff\n");
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
