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

static uint16_t backoff[MAX_PARENT_REQ];
static uint16_t start_times[MAX_PARENT_REQ];
static uint16_t necessity[MAX_PARENT_REQ];

static struct ctimer genesis_timer;

PROCESS(conetsi_server_process, "CoNetSI server");
AUTOSTART_PROCESSES(&conetsi_server_process);
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

  struct conetsi_pkt *pkt = (struct conetsi *) data;
  int n;

  switch(current_state) {

   case STATE_IDLE:
    if(pkt->type == TYPE_DEMAND_ADVERTISEMENT) {
      count = 0;

join_da:
      /* compute necessity */
      n = necessity(pkt->data);
      if(n > THRESHOLD_NECESSITY &&
         ((struct nsi_demand *)pkt->data)->time_left > THRESHOLD_TIME_USEC) {

        /* compute backoff */
        //etimer_set(&backoff_timer, get_backoff(best_n, time_left));
        backoff[count] = get_backoff(n, time_left);

        /* wait for T_backoff */
        current_state = STATE_BACKOFF;
        start_time[count] = clock_seconds();
        PROCESS_YIELD_UNTIL(clock_seconds() ==
            start_time[count] + backoff[count]);

        if(current_state == STATE_BACKOFF) {
          /* path terminates here */
          if(((struct nsi_demand *)pkt->data)->path_len ==
                  (THRESHOLD_PATH_LEN - 1)) {
            send_nsi(sender_addr, NULL);
            ctimer_restart(&genesis_timer);
            current_state = STATE_IDLE;
          } else {
            send_ack(sender_addr);
            current_state = STATE_AWAITING_JOIN_REQ;
          }
        }
      }
    }
    break;

   case STATE_BACKOFF:
    if(pkt->type == TYPE_JOIN_REQUEST &&
       uip_ipaddr_cmp(&((struct join_request *)pkt->data)->sender_addr,
                      &my->parent_node)) {
      /* reset back to idle state, since I am not chosen */
      current_state = STATE_IDLE;
      
    } else if(pkt->type == TYPE_DEMAND_ADVERTISEMENT) {
      count++;
      goto join_da;
    }
    break;

   case STATE_DEMAND_ADVERTISED:
    if(pkt->type == TYPE_ACK) {
      /* prepare and send join step 3 packet */
      send_join_req(sender_addr);
      current_state = STATE_CHILD_CHOSEN;
    }
    break;

   case STATE_CHILD_CHOSEN:
    if(pkt->type == TYPE_NSI) {
      send_nsi(my->parent_addr, ptr);
      ctimer_restart(&genesis_timer);
      current_state = STATE_IDLE;
    }
    break;

   case STATE_AWAITING_JOIN_REQ:
    if(pkt->type == TYPE_JOIN_REQUEST) {
      uip_ipaddr_copy(&my->parent_node, sender_addr);

      /* path length will not run out here due to additional
       * condition taken care of in STATE_IDLE case
       */
      if(/* timer is "about to expire" */) {
        send_nsi(my->parent, NULL);
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

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
