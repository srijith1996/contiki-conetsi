/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "oam.h"
/*---------------------------------------------------------------------------*/
struct oam_stats oam_buf_state;
struct oam_module modules[MAX_OAM_ENTRIES];

static struct oam_val return_val;
static int count, i;
static struct etimer poll_timer;

extern process_event_t genesis_event;

PROCESS(oam_collect_process, "OAM Process");
/*---------------------------------------------------------------------------*/
static int
demand()
{
  int demand = oam_buf_state.bytes / oam_buf_state.exp_time;
  demand = oam_buf_state.priority * demand;

  return demand;
}
/*---------------------------------------------------------------------------*/
void
register_oam(int oam_id, void (* value_callback) (struct oam_val *))
{
  modules[count].id = oam_id;

  modules[count].get_val = value_callback;
 
  modules[count].timeout = 65535;
  modules[count].priority = 65535;
  modules[count].data = NULL;
  
  count++;
}
/*---------------------------------------------------------------------------*/
void
unregister_oam(int oam_id)
{
  for(i = 0; i < count; i++) {
    if(modules[i].id == oam_id) {

      if(i == (count - 1)){
        count--;
        break;
      }

      /* copy last element to this location */
      modules[i].id = modules[count - 1].id;

      modules[i].get_val = modules[count - 1].get_val;

      modules[i].timeout = modules[count - 1].timeout;
      modules[i].priority = modules[count - 1].priority;
      modules[i].data = modules[count - 1].data;

      count--;
      break;
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(oam_collect_process, ev, data)
{
  PROCESS_BEGIN();

  oam_buf_state.bytes = 0;
  oam_buf_state.exp_time = 65535;
  oam_buf_state.priority = 65535;

  /* For now register all processes here */
  /* e.g.: register_oam(bat_volt_id, &get_bat_volt) */

  while(1) {
    etimer_set(&poll_timer, OAM_POLL_INTERVAL);
    PROCESS_YIELD_UNTIL(etimer_expired(&poll_timer));

    /* update the global expiration time */
    oam_buf_state.exp_time = (oam_buf_state.exp_time > OAM_POLL_INTERVAL)?
                         (oam_buf_state.exp_time - OAM_POLL_INTERVAL): 0;

    /* poll processes */
    for(i = 0; i < count; i++) {
      /* ignore if info is updated */
      if(modules[i].data != NULL) {
        break;
      }

      /* get the current function's value */
      modules[i].get_val(&return_val);
      modules[i].timeout = return_val.timeout;
      modules[i].data = return_val.data;

      oam_buf_state.bytes += OAM_ENTRY_BASE_SIZE;
      oam_buf_state.bytes += modules[i].bytes;

        if(oam_buf_state.exp_time < modules[i].timeout) {
          /* update the global expiration time */
          oam_buf_state.exp_time = modules[i].timeout;
        }
        if(oam_buf_state.priority > modules[i].priority) {
          oam_buf_state.priority = modules[i].priority;
        }
      }
    }

    if(demand() > DEMAND_THRESHOLD) {
      process_poll(&conetsi_server_process, genesis_event, &oam_buf_state);
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
