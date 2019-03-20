/*---------------------------------------------------------------------------*/
#include <math.h>
#include <stdio.h>
#include "contiki.h"
#include "contiki-net.h"

#include "oam.h"

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
config_priority(int id) {
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
/*
 * Linear scaling applied to local priority
 */
#if (CONF_PRIORITY != PRIORITY_RAYLEIGH)
static int
global_priority_lin(int id, int priority)
{
  /* scale the priority based on configured priority */
  int ret = HIGHEST_PRIORITY;

  ret += ((priority - HIGHEST_PRIORITY)
        * (LOWEST_PRIORITY - config_priority(id)))
      / (LOWEST_PRIORITY - HIGHEST_PRIORITY);

  /* Priority should be hard bound */
  ret = (ret < HIGHEST_PRIORITY) ? HIGHEST_PRIORITY : ret;
  ret = (ret > LOWEST_PRIORITY) ? LOWEST_PRIORITY : ret;

  LOG_DBG("Global Priority lin(%d): %d\n", id, ret);

  return ret;
}
/*---------------------------------------------------------------------------*/
/*
 * Rayleigh scaling applied to local priority
 */
#else /* (CONF_PRIORITY != PRIORITY_LINEAR) */
static int
global_priority_ray(int id, int priority)
{
  /* scale the priority based on configured priority */
  /* using the Rayleigh function to scale */
  int sigma, x, ret;
  sigma = LOWEST_PRIORITY - HIGHEST_PRIORITY;
  x = config_priority(id) - (LOWEST_PRIORITY - HIGHEST_PRIORITY)/2;

  ret = x / (sigma * sigma);
  ret = ret * exp(- x*x / (sigma*sigma));
  ret = ret + 1;

  ret = ret * priority;

  /* Priority should be hard bound */
  ret = (ret < HIGHEST_PRIORITY) ? HIGHEST_PRIORITY : ret;
  ret = (ret > LOWEST_PRIORITY) ? LOWEST_PRIORITY : ret;

  LOG_DBG("Global Priority(%d): %d\n", id, ret);

  return ret;
}
#endif /* (CONF_PRIORITY != PRIORITY_LINEAR) */
/*---------------------------------------------------------------------------*/
int
demand()
{
  LOG_DBG("Computing demand, timer(%p)\n", oam_buf_state.exp_timer);

  if(oam_buf_state.exp_timer == NULL) {
    return 0;
  /* handle case where expiration timer just timed out */
  } else if(timer_remaining(oam_buf_state.exp_timer) <= 0) {
    return 0;
  }

  int demand = DEMAND_FACTOR * oam_buf_state.bytes;
  demand *= (LOWEST_PRIORITY - oam_buf_state.priority) * 100;
  demand /= timer_remaining(oam_buf_state.exp_timer);

  LOG_DBG("Demand computation: %d, %d, %lu, %d\n", DEMAND_FACTOR,
         oam_buf_state.bytes,
         timer_remaining(oam_buf_state.exp_timer),
         oam_buf_state.priority);
  LOG_DBG("Demand: %d\n", demand);

  return demand;
}
/*---------------------------------------------------------------------------*/
uint16_t
get_nsi_timeout()
{
  LOG_DBG("Returning timeout, timer(%p)\n", oam_buf_state.exp_timer);

  if(oam_buf_state.exp_timer == NULL) {
    return -1;
  }

  return (uint16_t)timer_remaining(oam_buf_state.exp_timer);
}
/*---------------------------------------------------------------------------*/
uint16_t
get_bytes()
{
  return (uint16_t)oam_buf_state.bytes;
}
/*---------------------------------------------------------------------------*/
static void
global_exp()
{
  if(count == 0) {
    oam_buf_state.exp_timer = NULL;
    return;
  }

  oam_buf_state.exp_timer = &(modules[0].exp_timer.etimer.timer);
  for(i = 1; i < count; i++) {
    LOG_DBG("Global expiration check, timer(%p), module_timer(%p)\n",
             oam_buf_state.exp_timer, &(modules[i].exp_timer.etimer.timer));

    if(timer_remaining(oam_buf_state.exp_timer) >
       timer_remaining(&(modules[i].exp_timer.etimer.timer))) {
      oam_buf_state.exp_timer = &(modules[i].exp_timer.etimer.timer);
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
global_p()
{
  if(count == 0) {
    oam_buf_state.priority = LOWEST_PRIORITY;
    return;
  }
  oam_buf_state.priority = modules[0].priority;
  for(i = 1; i < count; i++) {
    if(oam_buf_state.priority > modules[i].priority) {
      oam_buf_state.priority = modules[i].priority;
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
cleanup(void *mod)
{
  struct oam_module *module = mod;

  LOG_INFO("Cleaning up module id: %d\n", module->id);
  oam_buf_state.bytes -= module->bytes;
  oam_buf_state.bytes -= OAM_ENTRY_BASE_SIZE;

  module->bytes = 0;
  module->priority = LOWEST_PRIORITY;
  module->data = NULL;
  ctimer_stop(&module->exp_timer);

  global_exp();
  global_p();
  //LOG_DBG("After cleanup: (GP: %d, Gexp: %lu)\n",
  //        oam_buf_state.priority, timer_remaining(oam_buf_state.exp_timer));
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
    cleanup(&modules[i]);
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
  oam_buf_state.priority = LOWEST_PRIORITY;
  oam_buf_state.exp_timer = NULL;

  /* For now register all processes here */
  /* e.g.: register_oam(bat_volt_id, &get_bat_volt) */
  register_oam(10, get_value, reset, get_conf,
               set_conf, start_dummy, stop_dummy);
  register_oam(11, get_value, reset, get_conf,
               set_conf, start_dummy, stop_dummy);

  while(1) {
    etimer_set(&oam_poll_timer, OAM_POLL_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&oam_poll_timer));

    LOG_DBG("Woken up for poll, module count - %d\n", count);

    /* poll processes */
    for(i = 0; i < count; i++) {
      /* ignore if info is updated */
      if(modules[i].data != NULL) {
        break;
      }
      LOG_INFO("Polling module %d for value\n", i);

      /* get the current module's value */
      modules[i].get_val(&return_val);

      /* consider entry only if the priority is high enough */
      if((modules[i].priority =
          global_priority(modules[i].id, return_val.priority))
            < THRESHOLD_PRIORITY) {

        /* Setup callback timer to cleanup this OAM process */
        ctimer_set(&modules[i].exp_timer, return_val.timeout,
                   cleanup, &modules[i]);
        modules[i].data = return_val.data;
        modules[i].bytes = return_val.bytes;

        oam_buf_state.bytes += OAM_ENTRY_BASE_SIZE;
        oam_buf_state.bytes += modules[i].bytes;

        if(oam_buf_state.exp_timer == NULL) {
          oam_buf_state.exp_timer = &(modules[i].exp_timer.etimer.timer);
        } else if(timer_remaining(oam_buf_state.exp_timer) >
           timer_remaining(&(modules[i].exp_timer.etimer.timer))) {
          /* update the global expiration time */
          oam_buf_state.exp_timer = &(modules[i].exp_timer.etimer.timer);
        }
        if(oam_buf_state.priority > modules[i].priority) {
          oam_buf_state.priority = modules[i].priority;
        }
      }
    }

    if(demand() > THRESHOLD_DEMAND) {
      LOG_INFO("Waking CoNetSI\n");
      process_post(&conetsi_server_process, genesis_event, &oam_buf_state);
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
