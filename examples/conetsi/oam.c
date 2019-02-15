/*---------------------------------------------------------------------------*/
#include <math.h>
#include <stdio.h>
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
priority(int id) {
  switch(id) {
    case BAT_VOLT_ID:        return 100;
    case FRAME_DROP_RATE_ID: return  10;
    case ETX_ID:             return  20;
    case QUEUE_STATE_ID:     return   5;
    default:                 return  -1;
  }
}
/*---------------------------------------------------------------------------*/
static float
global_priority(int id, int priority)
{
  /* scale the priority based on configured priority */
  /* using the Rayleigh function to scale */
  float sigma, x, ret;
  sigma = LOWEST_PRIORITY - HIGHEST_PRIORITY;
  x = priority(id) - (LOWEST_PRIORITY - HIGHEST_PRIORITY)/2;

  ret = x / (sigma * sigma);
  ret = ret * exp(- x*x / (sigma*sigma));
  ret = ret + 1;

  ret = ret * priority;
  return ret;
}
/*---------------------------------------------------------------------------*/
uint16_t
demand()
{
  uint16_t demand = oam_buf_state.bytes / oam_buf_state.exp_time;
  demand = oam_buf_state.priority * demand;

  return demand;
}
/*---------------------------------------------------------------------------*/
uint16_t
get_nsi_timeout()
{
  return (uint16_t) oam_buf_state.exp_time;
}
/*---------------------------------------------------------------------------*/
uint16_t
get_bytes()
{
  return (uint16_t) oam_buf_state.bytes;
}
/*---------------------------------------------------------------------------*/
int
oam_string(char *buf)
{
  int ctr = 0;

  sprintf(buf, "%c", (uint8_t) oam_buf_state.bytes);
  ctr += 1;

  for(i = 0; i < count; i++) {
    sprintf((buf + ctr), "%c", (uint8_t) modules[i].id);
    ctr += 1;
    sprintf((buf + ctr), "%c", (uint8_t) modules[i].bytes);
    ctr += 1;
    memcpy((buf + ctr), &modules[i].data, modules[i].bytes);
    ctr += modules[i].bytes;
  }
  return ctr;
}
/*---------------------------------------------------------------------------*/
void
register_oam(int oam_id,
             void (* value_callback) (struct oam_val *),
             void (* reset_callback) (void),
             void* (* get_conf_callback) (void),
             void (* set_conf_callback) (void *),
             int (* start_callback) (void),
             int (* stop_callback) (void))
{
  modules[count].id = oam_id;

  modules[count].get_val = value_callback;
  modules[count].reset = reset_callback;
  modules[count].get_config = get_conf_callback;
  modules[count].set_config = set_conf_callback;
  modules[count].start = start_callback;
  modules[count].stop = stop_callback;

  modules[count].bytes = 0;
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
      modules[i].reset = modules[count - 1].reset;
      modules[i].get_config = modules[count - 1].get_config;
      modules[i].set_config = modules[count - 1].set_config;
      modules[i].start = modules[count - 1].start;
      modules[i].stop = modules[count - 1].stop;

      modules[i].bytes = modules[count - 1].bytes
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
    PROCESS_YIELD_UNTIL(count != 0 && etimer_expired(&poll_timer));

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

      /* consider entry only if the priority is high enough */
      if((modules[i].priority =
          global_priority(modules[i].id, return_val.priority))
            < PRIORITY_THRESHOLD) {
        modules[i].timeout = return_val.timeout;
        modules[i].data = return_val.data;

        oam_buf_state.bytes += OAM_ENTRY_BASE_SIZE;
        oam_buf_state.bytes += modules[i].bytes;

        if(oam_buf_state.exp_time > modules[i].timeout) {
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
