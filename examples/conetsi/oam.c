/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "oam.h"
/*---------------------------------------------------------------------------*/
struct oam_stats oam_buf_state;

struct oam_entry collect_array[MAX_OAM_ENTRIES];
static struct oam_val return_val;
static int count, i;
static struct etimer poll_timer;

PROCESS(oam_collect_process, "OAM Process");
/*---------------------------------------------------------------------------*/
void
register_oam(int oam_id, void (* value_callback) (struct oam_val *))
{
  collect_array[count].id = oam_id;
  collect_array[count].get_val = value_callback;
  collect_array[count].timeout = 65535;
  collect_array[count].priority = 65535;
  collect_array[count].data = NULL;
  
  count++;
}
/*---------------------------------------------------------------------------*/
void
unregister_oam(int oam_id, void (* value_callback) (struct oam_val *))
{
  /* TODO: add logic */
  
  count--;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(oam_collect_process, ev, data)
{
  PROCESS_BEGIN();

  oam_stats.bytes = 0;
  oam_stats.exp_time = 65535;

  /* For now register all processes here */
  /* e.g.: register_oam(bat_volt_id, &get_bat_volt) */

  while(1) {
    etimer_set(&poll_timer, OAM_POLL_INTERVAL);
    PROCESS_YIELD_UNTIL(etimer_expired(&poll_timer));

    oam_stats.exp_time = (oam_stats.exp_time > OAM_POLL_INTERVAL)?
                         (oam_stats.exp_time - OAM_POLL_INTERVAL): 0;

    /* poll processes */
    for(i = 0; i < count; i++) {
      /* ignore if info is updated */
      if(collect_array[i].data != NULL) {
        break;
      }

      collect_array[i].get_val(&return_val);
      collect_array[i].timeout = return_val.timeout;
      collect_array[i].priority = return_val.priority;
      collect_array[i].data = return_val.data;

      oam_stats.bytes += OAM_ENTRY_BASE_SIZE;
      oam_stats.bytes += collect_array[i].bytes;

      /* update the global expiration time */
      if(oam_stats.exp_time < collect_array[i].timeout) {
        oam_stats.exp_time = collect_array[i].timeout;
      }
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
