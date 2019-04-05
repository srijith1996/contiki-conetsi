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
static int count, iter, indexer;
static struct etimer oam_poll_timer;

extern process_event_t genesis_event;

PROCESS_NAME(conetsi_server_process);
PROCESS(oam_collect_process, "OAM Process");
/*---------------------------------------------------------------------------*/
/*
 * Linear scaling applied to local priority
 */
#if (CONF_PRIORITY != PRIORITY_RAYLEIGH)
static int
global_priority_lin(int config_priority, int data_priority)
{
  /* scale the priority based on configured priority */
  int ret = HIGHEST_PRIORITY;

  ret += ((data_priority - HIGHEST_PRIORITY)
        * (LOWEST_PRIORITY - config_priority))
      / (LOWEST_PRIORITY - HIGHEST_PRIORITY);

  /* Priority should be hard bound */
  ret = (ret < HIGHEST_PRIORITY) ? HIGHEST_PRIORITY : ret;
  ret = (ret > LOWEST_PRIORITY) ? LOWEST_PRIORITY : ret;

  LOG_DBG("Global Priority lin: %d\n", ret);

  return ret;
}
/*---------------------------------------------------------------------------*/
/*
 * Rayleigh scaling applied to local priority
 */
#else /* (CONF_PRIORITY != PRIORITY_RAYLEIGH) */
static int
global_priority_ray(int config_priority, int data_priority)
{
  /* scale the priority based on configured priority */
  /* using the Rayleigh function to scale */
  int sigma, x, ret;
  sigma = LOWEST_PRIORITY - HIGHEST_PRIORITY;
  x = config_priority - (LOWEST_PRIORITY - HIGHEST_PRIORITY)/2;

  ret = x / (sigma * sigma);
  ret = ret * exp(- x*x / (sigma*sigma));
  ret = ret + 1;

  ret = ret * data_priority;

  /* Priority should be hard bound */
  ret = (ret < HIGHEST_PRIORITY) ? HIGHEST_PRIORITY : ret;
  ret = (ret > LOWEST_PRIORITY) ? LOWEST_PRIORITY : ret;

  LOG_DBG("Global Priority: %d\n", ret);

  return ret;
}
#endif /* (CONF_PRIORITY != PRIORITY_RAYLEIGH) */
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
  demand *= (LOWEST_PRIORITY - oam_buf_state.priority);
  demand /= timer_remaining(oam_buf_state.exp_timer);

  LOG_DBG("Demand computation: df=%d, B=%d, T=%lu, P=%d\n", DEMAND_FACTOR,
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
  if(oam_buf_state.exp_timer == NULL) {
    return -1;
  }

  LOG_DBG("Returning timeout, timer(%p) %d\n",
          oam_buf_state.exp_timer,
          timer_remaining(oam_buf_state.exp_timer));


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
  oam_buf_state.exp_timer = NULL;
  if(count == 0) {
    return;
  }

  if(!modules[0].lock) {
    oam_buf_state.exp_timer = &(modules[0].exp_timer.etimer.timer);
  }

  int i;
  for(i = 1; i < count; i++) {
    /* only check unlocked modules */
    if(modules[i].lock) {
      continue;
    }
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
  oam_buf_state.priority = modules[0].data_priority;
  int i;
  for(i = 1; i < count; i++) {
    if(oam_buf_state.priority > modules[i].data_priority) {
      oam_buf_state.priority = modules[i].data_priority;
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
cleanup(void *mod)
{
  struct oam_module *module = mod;
  LOG_INFO("Cleaning up module id: %d\n", module->id);

  ctimer_stop(&module->exp_timer);
  LOG_DBG("Stopped CTimer for module %d\n", module->id);
 
  oam_buf_state.bytes -= module->bytes;
  oam_buf_state.bytes -= OAM_ENTRY_BASE_SIZE;

  LOG_DBG("Locking module: %d\n", module->id);
  module->lock = 1;
  module->bytes = 0;
  module->data_priority = LOWEST_PRIORITY;

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

  sprintf(buf, "%c", (uint8_t)(oam_buf_state.bytes - LINKADDR_SIZE - 1));
  ctr += 1;
    
  /* Always store module data in network byte order */
  for(i = 0; i < count; i++) {
    LOG_DBG("Checking validity for %d (locked: %d)\n",
            modules[i].id, modules[i].lock);

    /* Lock will prevent cleaned up data from being recorded */
    if(!modules[i].lock) {
#if (LOG_LEVEL_OAM == LOG_LEVEL_DBG)
      int j;
      int pctr = ctr;
#endif /* (LOG_LEVEL_OAM == LOG_LEVEL_DBG) */
      memcpy((buf + ctr), &(modules[i].id), 1);
      ctr += 1;
      memcpy((buf + ctr), &(modules[i].bytes), 1);
      ctr += 1;
      memcpy((buf + ctr), &(modules[i].data), modules[i].bytes);
      ctr += modules[i].bytes;

      LOG_DBG("Copied module data (%d) - ", modules[i].id);
      for(j = pctr; j < ctr; j++) {
        LOG_DBG_("%02x:", *((uint8_t *)buf + pctr + j));
      }
      LOG_DBG_("\n");

      /* notify the modules to reset counters */
      modules[i].reset();
      cleanup(&modules[i]);
    }
  }

  return ctr;
}
/*---------------------------------------------------------------------------*/
void
register_oam(int id, int mod_p,
             void (* value_callback) (struct oam_val *),
             void (* reset_callback) (void),
             void* (* get_conf_callback) (void),
             void (* set_conf_callback) (void *),
             int (* start_callback) (void),
             int (* stop_callback) (void))
{
  modules[count].id = id;
  modules[count].lock = 1;
  modules[count].mod_priority = mod_p;

  modules[count].get_val = value_callback;
  modules[count].reset = reset_callback;
  modules[count].get_config = get_conf_callback;
  modules[count].set_config = set_conf_callback;
  modules[count].start = start_callback;
  modules[count].stop = stop_callback;

  modules[count].bytes = 0;
  modules[count].data_priority = LOWEST_PRIORITY;
  /* modules[count].data = NULL; */

  LOG_INFO("Registered module: %d\n", modules[count].id);
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

      LOG_INFO("Un-registering module: %d\n", modules[i].id);

      /* copy last element to this location */
      modules[i].id = modules[count - 1].id;
      modules[i].lock = modules[count - 1].lock;
      modules[i].mod_priority = modules[count - 1].mod_priority;

      modules[i].get_val = modules[count - 1].get_val;
      modules[i].reset = modules[count - 1].reset;
      modules[i].get_config = modules[count - 1].get_config;
      modules[i].set_config = modules[count - 1].set_config;
      modules[i].start = modules[count - 1].start;
      modules[i].stop = modules[count - 1].stop;

      modules[i].bytes = modules[count - 1].bytes;
      modules[i].data_priority = modules[count - 1].data_priority;
      memcpy(modules[i].data, modules[count - 1].data,
             modules[count - 1].bytes);

      count--;
      break;
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(oam_collect_process, ev, data)
{
  PROCESS_BEGIN();

  /* One byte for the size field */
  oam_buf_state.bytes = LINKADDR_SIZE + 1;
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
    for(iter = 0; iter < count; iter++) {
      /* ignore if info is updated */
      if(!modules[iter].lock) {
        break;
      }
      LOG_INFO("Polling module %d for value\n", modules[iter].id);

      /* get the current module's value */
      modules[iter].get_val(&return_val);

      int j;
      LOG_DBG("Received module data - ");
      for(j = 0; j < sizeof(struct oam_val); j++) {
        LOG_DBG_("%02x:", *((uint8_t *)&return_val + j));
      }
      LOG_DBG_("\n");

      /* consider entry only if the priority is high enough */
      if((modules[iter].data_priority =
          global_priority(modules[iter].mod_priority, return_val.priority))
            < THRESHOLD_PRIORITY) {

        LOG_DBG("Obtained NSI: timeout=%d, priority=%d, size=%d\n",
                return_val.timeout, return_val.priority, return_val.bytes);

        /* Setup callback timer to cleanup this OAM process */
        ctimer_set(&modules[iter].exp_timer, return_val.timeout,
                   cleanup, &modules[iter]);
        LOG_DBG("Started CTimer for module %d\n", modules[iter].id);

        modules[iter].bytes = return_val.bytes;
        memcpy(modules[iter].data, return_val.data, return_val.bytes);

        oam_buf_state.bytes += OAM_ENTRY_BASE_SIZE;
        oam_buf_state.bytes += modules[iter].bytes;

        if(oam_buf_state.exp_timer == NULL) {
          oam_buf_state.exp_timer = &(modules[iter].exp_timer.etimer.timer);
        } else if(timer_remaining(oam_buf_state.exp_timer) >
           timer_remaining(&(modules[iter].exp_timer.etimer.timer))) {
          /* update the global expiration time */
          oam_buf_state.exp_timer = &(modules[iter].exp_timer.etimer.timer);
        }
        if(oam_buf_state.priority > modules[iter].data_priority) {
          oam_buf_state.priority = modules[iter].data_priority;
        }

        /* release lock to this module */
        LOG_DBG("Releasing module lock after data entry\n");
        modules[iter].lock = 0;
      }
    }

    if(demand() > THRESHOLD_DEMAND) {
      LOG_INFO("Waking CoNetSI");
      LOG_DBG_(" (Threshold: %d, MAX: %d)", THRESHOLD_DEMAND, MAX_DEMAND);
      LOG_INFO_("\n");
      process_post(&conetsi_server_process, genesis_event, &oam_buf_state);
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
