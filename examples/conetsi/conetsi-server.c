/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"

#include "conetsi.h"
/*---------------------------------------------------------------------------*/
#define UDP_SERVER_PORT 3005
#define UDP_CLIENT_PORT 3005

static struct simple_udp_connection conetsi_conn;
static struct uip_ipaddr_t mcast_addr;
static int current_state;

PROCESS(conetsi_server_process, "CoNetSI server");
AUTOSTART_PROCESSES(&conetsi_server_process);
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver port,
         const uint8_t *data,
         uint16_t datalen)
{

/* inclusion of a state space may make more sense */
/* switch case can be used for all states */

  switch(current_state) {

   case STATE_IDLE:
    if(/* received demand adv */) {
      /* compute necessity */
      /* compute T_i */
      if(/* N > N_threshold && T > T_threshold */) {
        /* compute backoff */
        /* wait for T_backoff */
        /* send ack */
        current_state = STATE_AWAITING_JOIN_REQ;
      }
    }
    break;

   case STATE_DEMAND_ADVERTISED:
    if(/* received ack */) {
      /* prepare and send join step 3 packet */
      current_state = STATE_CHILD_CHOSEN;
    }
    break;

   case STATE_CHILD_CHOSEN:
    if(/* received NSI */) {
      /* call add_my_nsi() */
      /* send_nsi() */
      current_state = STATE_IDLE;
    }
    break;

   case STATE_AWAITING_JOIN_REQ:
    if(/* received join req */) {
      current_state = STATE_JOINED;
      reset_timers()
    }
    break;

   case STATE_JOINED:
    /* do nothing, since we should have moved to
     * STATE_DEMAND_ADVERTISED or STATE_IDLE
     */
    break;
   
   default:
    PRINTF("Error in CoNetSI: reached an unknown state\n");

  }
  
  /* recompute timeouts */
  reset_timers();

  return;
    
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(conetsi_server_process, ev, data)
{
  PROCESS_BEGIN();

  current_state = STATE_INIT;

  /* register multicast address */
  uip_ip6addr(&mcast_addr, 0xff01, 0, 0, 0, 0, 0, 0, 0x0002);
  uip_ds6_maddr_add(&mcast_addr);

  /* register to listen to incoming CoNetSI connections */
  simple_udp_register(&conetsi_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
