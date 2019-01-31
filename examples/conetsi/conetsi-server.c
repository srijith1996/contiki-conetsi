#include "contiki.h"
#include "contiki-net.h"

#include "net/ipv6/simple-udp.h"

#include "sys/log.h"

#define UDP_SERVER_PORT 3005
#define UDP_CLIENT_PORT 3005

static struct simple_udp_connection conetsi_conn;
static struct uip_ipaddr_t mcast_addr;

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

  if(/* receiver address is multicast conetsi */) {
    if(/* demand advertised */) {
      /* drop */ 
    } else {
      /* compute demand */
    }
 
    if(/* demand > threshold */) {
      /* compute back-off and wait */
      /* send ack */
    } else {
      /* drop */
    }
  }
  /* check the type of conetsi message */
  if(/*demand advertised */) {
    if(/* received ack */) {
      /* set demand advertisation flag to 0 */
      /* prepare and send join step 3 packet */
    }
  }

  if(/* state == awaiting ack */) {
    if(/* received joining step 3 */) {
      /* recalculate demand */
      /* store parent */
      /* mcast demand */
    } else {
      /* nothing */
    }
  }

  if(/* nsi packet received */) {
    if(/* iamgenesis */) {
      /* send packet to root */
    } else {
      /* send packet to stored parent */
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(conetsi_server_process, ev, data)
{
  PROCESS_BEGIN();

  /* register multicast address */
  uip_ip6addr(&mcast_addr, 0xff01, 0, 0, 0, 0, 0, 0, 0x0002);
  uip_ds6_maddr_add(&mcast_addr);

  /* register to listen to incoming CoNetSI connections */
  simple_udp_register(&conetsi_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  PROCESS_END();
}
