/*---------------------------------------------------------------------------*/
#include <math.h>
#include <stdio.h>
#include "contiki.h"
#include "contiki-net.h"

#include "oam.h"
#include "conetsi.h"

#include "sys/log.h"
#define LOG_MODULE "OAM"
#define LOG_LEVEL LOG_LEVEL_OAM
/*---------------------------------------------------------------------------*/
/* TODO: All timers do not consider the compute time
 * This needs an update in the next iteration
 */
/* temporarily declaring methods for dummy, here */
void reset();
int start_dummy();
int stop_dummy();
void get_value(struct oam_val *oam);
void *get_conf();
void set_conf(void *);
/*---------------------------------------------------------------------------*/
struct oam_stats oam_buf_state;
struct oam_module modules[MAX_OAM_ENTRIES];

struct dummy { uint16_t dummy_val; uint16_t max_val; uint16_t min_val; };

static struct oam_val return_val;
static int count, i;
static struct etimer oam_poll_timer;

extern process_event_t genesis_event;

PROCESS_NAME(conetsi_server_process);
PROCESS(oam_collect_process, "OAM Process");
/*---------------------------------------------------------------------------*/
static int
local_priority(int id) {
  switch(id) {
    case 10:                 return  20;
    case BAT_VOLT_ID:        return 100;
    case FRAME_DROP_RATE_ID: return  10;
    case ETX_ID:             return  20;
    case QUEUE_STATE_ID:     return   5;
    default:                 return  -1;
  }
}
/*---------------------------------------------------------------------------*/
static int
global_priority(int id, int priority)
{
  /* scale the priority based on configured priority */
  /* using the Rayleigh function to scale */
  float sigma, x, ret;
  sigma = LOWEST_PRIORITY - HIGHEST_PRIORITY;
  x = local_priority(id) - (LOWEST_PRIORITY - HIGHEST_PRIORITY)/2;

  ret = x / (sigma * sigma);
  ret = ret * exp(- x*x / (sigma*sigma));
  ret = ret + 1;

  ret = ret * priority;

  /* avoid lesser than 1 priority */
  if(ret <= 1) {
    ret = 1;
  }

  return ret;
}
/*---------------------------------------------------------------------------*/
int
demand()
{
  int demand = DEMAND_FACTOR * oam_buf_state.bytes;
  demand *= (LOWEST_PRIORITY - oam_buf_state.priority);
  demand /= (oam_buf_state.exp_time / 100);

  LOG_DBG("Demand computation: %d, %d, %d, %d\n", DEMAND_FACTOR,
         oam_buf_state.bytes, oam_buf_state.exp_time, oam_buf_state.priority);
  LOG_DBG("Demand: %d\n", demand);

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
void
cleanup(int force)
{
  uint16_t min_exp = 65535;
  uint16_t min_priority = LOWEST_PRIORITY;
  int none_left = 1;
  int i;

  for(i = 0; i < count; i++) {

    /* invalidate expired module data */
    if(force || (oam_buf_state.init_min_time) >= modules[i].timeout) {

      LOG_INFO("Cleaning up module id: %d\n", modules[i].id);
      modules[i].bytes = 0;
      modules[i].timeout = 65535;
      modules[i].priority = LOWEST_PRIORITY;
      modules[i].data = NULL;
      oam_buf_state.bytes -= modules[i].bytes;

    } else {
      modules[i].timeout -= oam_buf_state.init_min_time;
      if(modules[i].timeout < min_exp) {
        oam_buf_state.exp_time = modules[i].timeout;
        oam_buf_state.init_min_time = modules[i].timeout;
      }
      if(modules[i].priority < min_priority) {
        oam_buf_state.priority = modules[i].priority;
      }
      none_left = 0;
    }
  }

  if(none_left) {
    oam_buf_state.bytes = LINKADDR_SIZE;
    oam_buf_state.exp_time = 65535;
    oam_buf_state.priority = LOWEST_PRIORITY;
    oam_buf_state.init_min_time = 65535;
  }
  return;
}     
/*---------------------------------------------------------------------------*/
int
oam_string(char *buf)
{
  int ctr = 0;
  int i;

  sprintf(buf, "%c", (uint8_t) oam_buf_state.bytes);
  ctr += 1;
    
  /* TODO: Always store module data in network byte order */
  for(i = 0; i < count; i++) {
    memcpy((buf + ctr), &(modules[i].id), 1);
    ctr += 1;
    memcpy((buf + ctr), &(modules[i].bytes), 1);
    ctr += 1;
    memcpy((buf + ctr), &(modules[i].data), modules[i].bytes);
    ctr += modules[i].bytes;

    /* notify the modules to reset counters */
    modules[i].reset();
  }

  /* NOTE: Clears every module. If selective sending is enabled in
          the future, we need to cleanup differently */
  cleanup(1);
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
  int i;

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

      modules[i].bytes = modules[count - 1].bytes;
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

  oam_buf_state.bytes = LINKADDR_SIZE;
  oam_buf_state.init_min_time = 65535;
  oam_buf_state.exp_time = 65535;
  oam_buf_state.priority = LOWEST_PRIORITY;

  /* For now register all processes here */
  /* e.g.: register_oam(bat_volt_id, &get_bat_volt) */
  register_oam(10, get_value, reset, get_conf,
               set_conf, start_dummy, stop_dummy);
  register_oam(11, get_value, reset, get_conf,
               set_conf, start_dummy, stop_dummy);

  while(1) {
    etimer_set(&oam_poll_timer, OAM_POLL_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&oam_poll_timer));

    /* update the global expiration time */
    oam_buf_state.exp_time = (oam_buf_state.exp_time > OAM_POLL_INTERVAL)?
                         (oam_buf_state.exp_time - OAM_POLL_INTERVAL): 0;

    if(oam_buf_state.exp_time <= 0) {
      cleanup(0);
    }

    /* poll processes */
    for(i = 0; i < count; i++) {
      /* ignore if info is updated */
      if(modules[i].data != NULL) {
        break;
      }

      /* get the current module's value */
      modules[i].get_val(&return_val);

      /* consider entry only if the priority is high enough */
      if((modules[i].priority =
          global_priority(modules[i].id, return_val.priority))
            < THRESHOLD_PRIORITY) {
        modules[i].timeout = return_val.timeout;
        modules[i].data = return_val.data;
        modules[i].bytes = return_val.bytes;

        oam_buf_state.bytes += OAM_ENTRY_BASE_SIZE;
        oam_buf_state.bytes += modules[i].bytes;

        if(oam_buf_state.exp_time > modules[i].timeout) {
          /* update the global expiration time */
          oam_buf_state.exp_time = modules[i].timeout;
          oam_buf_state.init_min_time = oam_buf_state.exp_time;
        }
        if(oam_buf_state.priority > modules[i].priority) {
          oam_buf_state.priority = modules[i].priority;
        }
      }
    }

    if(demand() > THRESHOLD_DEMAND) {
      process_post(&conetsi_server_process, genesis_event, &oam_buf_state);
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
