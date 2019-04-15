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

static struct parent_details parent[MAX_PARENT_REQ];
static int count, i;

static char conetsi_data[THRESHOLD_PKT_SIZE];

static struct ctimer idle_timer;
static struct etimer bo_poll_timer;
process_event_t genesis_event;

/* for notifications from UIP */
struct uip_ds6_notification uip_notification;

PROCESS_NAME(oam_collect_process);
PROCESS(backoff_polling_process, "Backoff Polling process");
PROCESS(conetsi_server_process, "CoNetSI server");
AUTOSTART_PROCESSES(&conetsi_server_process);
/*---------------------------------------------------------------------------*/
static int
id_parent(const uip_ipaddr_t *parent_addr)
{
  int i;
  for(i = 0; i < count; i++) {
    if(uip_ipaddr_cmp(&parent[i].addr, parent_addr)) {
      return i;
    }
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
static void
add_parent(const uip_ipaddr_t *sender, struct nsi_demand *demand_pkt)
{

  /* change byte order */
  NTOHS(demand_pkt->time_left);
  
  LOG_DBG("Obtained DA: d=%d, T=%d, B=%d\n",
          demand_pkt->demand, demand_pkt->time_left,
          demand_pkt->bytes);
  if(demand_pkt->demand > THRESHOLD_DEMAND &&
     demand_pkt->time_left > THRESHOLD_TIMEOUT_MSEC) {

    LOG_DBG("Parent can be added\n");

    current_state = STATE_BACKOFF;

    /* store essential information */
    uip_ipaddr_copy(&parent[count].addr, sender);

    /* both backoff and start time in ticks */
    parent[count].flagged = 0;
    parent[count].start_time = RTIMER_NOW();
    parent[count].timeout = msec2rticks(demand_pkt->time_left);
    parent[count].demand = demand_pkt->demand;
    parent[count].bytes = demand_pkt->bytes;

    parent[count].prev_demand = demand();
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
    send_nsi(NULL, 0);
    break;

   case STATE_IDLE:
   case STATE_BACKOFF:
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
    if(pkt->type == TYPE_DEMAND_ADVERTISEMENT) {
      /* if listen_flag = 1, the demand adv is ignored without checking
       * the value of count. Only if it is zero will the count be checked
       */
      LOG_INFO("Received another DA from ");
      LOG_INFO_6ADDR(sender_addr);
      LOG_INFO_("\n");

      /* Add the child as a new parent and continue */
      uint8_t id;
      struct nsi_demand *dmnd = (void *)pkt->data;
      if((id = id_parent(&dmnd->parent_addr)) >= 0) {
        LOG_INFO(" ");
        LOG_INFO_6ADDR(sender_addr);
        LOG_INFO_(" already forwarded ");
        LOG_INFO_6ADDR(&parent[id].addr);
        LOG_INFO_("'s request\n");
        parent[id].flagged = 1;
      }

      if(listen_flag || count >= MAX_PARENT_REQ) {
        LOG_DBG("Ignoring further Demand advertisements");
        listen_flag = 1;
      } else {
        add_parent(sender_addr, (void *)pkt->data);
      }
    } else if(pkt->type == TYPE_NSI) {
      uint8_t id;
      struct nsi_forward *nsi = (void *)pkt->data;
      if((id = id_parent(&(nsi->to))) >= 0) {
        parent[id].flagged = 1;
      }
    }
    break;

   case STATE_DEMAND_ADVERTISED:

    if(pkt->type == TYPE_NSI) {
      struct nsi_forward *nsi = (void *)pkt->data;
      if(uip_ds6_is_my_addr(&(nsi->to))) {
        ctimer_stop(&idle_timer);
        LOG_INFO("Got NSI packet from child ");
        LOG_INFO_6ADDR(sender_addr);
        LOG_INFO_("\n");

        rm_child(sender_addr);
        send_nsi((void *)&conetsi_data, datalen);
        if(child_count() <= 0) {
          LOG_DBG("Resetting to IDLE, child count 0\n");
          current_state = STATE_IDLE;
        }
        /*TODO: figure out what else needs to be done here */
      }
    } else if(pkt->type == TYPE_DEMAND_ADVERTISEMENT) {
      struct nsi_demand *dmnd = (void *)pkt->data;
      if(uip_ds6_is_my_addr(&(dmnd->parent_addr))) {
        LOG_INFO("Got my own DA from child ");
        LOG_INFO_6ADDR(sender_addr);
        LOG_INFO_("\n");
        add_child(sender_addr);
        /*TODO: figure out what else needs to be done here */
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
void
uip_callback(int event, const uip_ipaddr_t *route,
             const uip_ipaddr_t *nexthop, int num_routes)
{

  LOG_INFO("UIP called back: \n");
  LOG_INFO("# of routes: %d\n", num_routes);
  LOG_INFO("Route: ");
  LOG_INFO_6ADDR(route);
  LOG_INFO_("\n");
  LOG_INFO("Next Hop: ");
  LOG_INFO_6ADDR(nexthop);
  LOG_INFO_("\n");

  if(event == UIP_DS6_NOTIFICATION_DEFRT_ADD) {
    LOG_INFO("Posting to CoNetSI server\n");
    process_post(&conetsi_server_process, event, NULL);

    LOG_INFO("Starting OAM and backoff processes\n");
    process_start(&backoff_polling_process, NULL);
    process_start(&oam_collect_process, NULL);
  }
  return;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(conetsi_server_process, ev, data)
{
  PROCESS_BEGIN();

  /* Set up route notification */
  uip_ds6_notification_add(&uip_notification, &uip_callback);
  LOG_INFO("Waiting for default route\n");
  PROCESS_YIELD_UNTIL(ev == UIP_DS6_NOTIFICATION_DEFRT_ADD);

  /* if needed remove further callbacks from UIP */
  uip_ds6_notification_rm(&uip_notification);

  /* Initialize global vars */
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

        if(get_bytes() >= MARGINAL_PKT_SIZE ||
           get_nsi_timeout() <= THRESHOLD_TIMEOUT_TICKS) {
          send_nsi(NULL, 0);
        } else {
          send_demand_adv(NULL);

          init_exp_time = RTIMER_NOW();
          exp_time = get_nsi_timeout();
          ctimer_set(&idle_timer, exp_time, reset_idle, NULL);
          current_state = STATE_DEMAND_ADVERTISED;

          LOG_INFO("Timeout ticks: %ld\n", exp_time);
          exp_time = ticks2rticks(exp_time);
          LOG_DBG("Timeout rtimer ticks: %ld\n", exp_time);
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

  LOG_INFO("Rtimer second: %d\n", RTIMER_SECOND);

  /* Change the context to conetsi process to avoid blocking
   * the UDP callback process */
  while(1) {
    /* recompute yield */
    etimer_set(&bo_poll_timer, BACKOFF_POLL_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&bo_poll_timer));

    yield = 1;
    all_flagged = 0;

    /* LOG_DBG("Woken: count=%d\n", count); */

    for(i = 0; i < count; i++) {
      LOG_DBG("Parent ");
      LOG_DBG_6ADDR(&parent[i].addr);
      LOG_DBG_(" flagged? %d\n", parent[i].flagged);

      LOG_DBG("Parent start time: %ld\n", parent[i].start_time);

      if(demand() != parent[i].prev_demand) {
        parent[i].prev_demand = demand();
        parent[i].backoff = get_backoff(parent[i].prev_demand,
                                        parent[i].timeout);
      }

      LOG_DBG("Parent backoff: %ld\n", parent[i].backoff);
      LOG_DBG("Time now: %ld\n", RTIMER_NOW());
      LOG_DBG("Time left: %ld\n", (parent[i].start_time
                                 + parent[i].backoff - RTIMER_NOW()));

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
      /* account for time wasted in backoff */
      parent[i].timeout -= parent[i].backoff;
      set_parent(&(parent[i].addr));

      /* path terminates here */
      if(parent[i].bytes >= MARGINAL_PKT_SIZE) {
        /* now i indexes the first entry which expired */
        LOG_INFO("Packet will bloat if continued. Sending NSI\n");
        send_nsi(NULL, 0);
        current_state = STATE_IDLE;
      } else {
        LOG_DBG("Backoff over. Sending DA\n");
        parent[i].timeout = send_demand_adv(&parent[i]);
        current_state = STATE_DEMAND_ADVERTISED;

        /* start count-down */
        LOG_INFO("Resetting to IDLE in: %lu\n", parent[i].timeout);
        ctimer_set(&idle_timer, rticks2ticks(parent[i].timeout),
                   reset_idle, NULL);
      }
      count = 0;
    }
  }
  LOG_DBG("Exiting backoff\n");
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
