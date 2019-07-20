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
#define LOG_MODULE "Normal-Server"
#define LOG_LEVEL LOG_LEVEL_NORMAL
/*---------------------------------------------------------------------------*/
process_event_t genesis_event;
/* for notifications from UIP */
struct uip_ds6_notification uip_notification;

PROCESS_NAME(oam_collect_process);
PROCESS(normal_server_process, "Normal server");
AUTOSTART_PROCESSES(&normal_server_process);
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
    process_post(&normal_server_process, event, NULL);

    LOG_INFO("Starting OAM and backoff processes\n");
    process_start(&oam_collect_process, NULL);
  }
  return;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(normal_server_process, ev, data)
{
  PROCESS_BEGIN();

  /* Set up route notification */
  uip_ds6_notification_add(&uip_notification, &uip_callback);
  LOG_INFO("Waiting for default route\n");
  PROCESS_YIELD_UNTIL(ev == UIP_DS6_NOTIFICATION_DEFRT_ADD);

  /* if needed remove further callbacks from UIP */
  uip_ds6_notification_rm(&uip_notification);

  /* Initialize global vars */
 // current_state = STATE_IDLE;
  genesis_event = process_alloc_event();

  /* Register the IP address of the host */
  reg_host();
 
  while(1) {
    PROCESS_YIELD();

    /* OAM process will wake us up when it is relevant to
     * send OAM data
     */
    if(ev == genesis_event) {
      send_nsi(NULL, 0);
    }
  }
  PROCESS_END();
}

