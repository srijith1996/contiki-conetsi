/*---------------------------------------------------------------*/
#include "contiki.h"
#include <stdio.h>
#include <random.h>
#include <string.h>
#include <math.h>

#if CONF_QSIM_MODULE
#include "oam.h"
#include "net/routing/rpl-lite/rpl.h"

#include "sys/log.h"
#define LOG_MODULE "MM1 Queue"
#define LOG_LEVEL LOG_LEVEL_QSIM_MOD
/*---------------------------------------------------------------*/
#define SAMPLE_TIME_SEC 1
/*---------------------------------------------------------------*/
/* for configuring poissons process
 * parameters can be set manually
 */ 
#define ARRVL_RATE 50.0        //IN PER HOUR
#define SERV_RATE  30.0
#define TOTAL_TIME 1
#define QUEUE_SIZE 14
#define MM1Q_PRIORITY 1 
#define MAX_PROCS 50     /* Don't make this too high (memory) */
/*---------------------------------------------------------------*/
#define QLEN_SIM_MIN_PRIORITY       10
#define QLEN_SIM_PRIORITY_INC_RATE  2
/*---------------------------------------------------------------*/
static struct ctimer small;
static struct ctimer big;
static int state;
static int time;
static int at_iter, et_iter;

/* simulation vars */
static int len;
static int IAT[MAX_PROCS], ST[MAX_PROCS],
           AT[MAX_PROCS], ET[MAX_PROCS];
/*---------------------------------------------------------------*/
/*generate poisson's distribution*/
static int
poisson_process(float rate)
{ 
  int k = 0;
  double step = 50.0;
  double p = 1.0, l;

  do {
    k = k+1;
    l = (random_rand() % RANDOM_RAND_MAX)
            / (RANDOM_RAND_MAX * 1.0);
    p *= l;
    //LOG_DBG("[Poisson] Random p: %d\n", p);

    while(p < 1 && rate > 0) {
      if(rate > step) {
        p *= exp(step);
        rate -= step;
      } else {
        p *= exp(rate);
        rate = 0;
      }
    }
  } while(p > 1);

//  LOG_DBG("Generated Poisson: %d\n", (k-1));
  return (k-1);   
}
/*---------------------------------------------------------------*/
/*generate exponential distribution*/
static float
exponential_process(float rate)
{       
  float x = (random_rand()%RANDOM_RAND_MAX)/(RANDOM_RAND_MAX*1.0);
  float r = (-log(x)/(1.0 * rate));
  
 // LOG_DBG("Generated Exponential: %d\n", r);
  return r;
}
/*---------------------------------------------------------------*/
/* Callback functions for ctimers. */
static void
op()          /* Func for small timer */
{
  ctimer_reset(&small);
  if(time == AT[at_iter]) {
    state++;
    at_iter++;
  }if(time == ET[et_iter]) {
    state--;
    et_iter++;
  }
  time += SAMPLE_TIME_SEC;
}
/*---------------------------------------------------------------*/
static void
op_init()     /* Func for big timer */
{
  int j = 0, i = 0 ;
  double temp;

  /* sample total time */
  //printf("Sampling poisson\n");
  do {
      len = poisson_process(ARRVL_RATE) * 10;
      time = 0;
  }while(len>50);

  /* Setup ctimers */
  ctimer_set(&big, (CLOCK_SECOND * TOTAL_TIME * 3600),
             op_init, NULL);
  ctimer_set(&small, (CLOCK_SECOND * SAMPLE_TIME_SEC),
             op, NULL);  
  
  /* Populate Inter-Arrival_Times (IAT)*/
  for(i=0;i<len;i++) {
    //printf("Sampling exponential\n");
    temp = exponential_process(ARRVL_RATE) * 3600.0; 
    IAT[i] = (i != 0) ? (int)floor(temp) : 0;
  }    

  /* Populating Service-Times(ST) (where ST(j)!=0)*/
  for (j=0;j<len;j++) {
    temp = exponential_process(SERV_RATE) * 3600.0;
    int h = (int)floor(temp);
    if(h == 0) {
      continue;
    }else
      ST[j] = h;
    }

  /*Get arrival-Times (AT) from IAT starting at t=0
    and initializes Waiting-Timings to 0 */
  for(i=0;i<len;i++) {
    if(i == 0) {
      AT[0]=0;
    } else {
      AT[i]=AT[i-1]+IAT[i];
    }
  }

  /*Simulation of M/M/1 Queue (i represents current time)*/
  ET[0]=ST[0];
  for(i=0;i<len-1;i++) {
    ET[i+1] = MAX(ET[i],AT[i+1]) + ST[i+1];
  }
}
/*---------------------------------------------------------------*/
static void
reset(void)
{
  /* Do not reset the sampling process here, since this is called
   * at every module cleanup.  This function should only be used
   * to reset the average counters, etc.
   */

}
/*---------------------------------------------------------------*/
static int
start(void)
{
  reset();
  return 0;
}
/*---------------------------------------------------------------*/
static int
stop(void)
{
  return 0;
}
/*---------------------------------------------------------------*/
static void
get_value(struct oam_val *oam)
{
  if(state < 0) {
    LOG_INFO("Undetermined state reached...\n");
    /* invalidate this data instance */
    oam->priority = THRESHOLD_PRIORITY + 1;
  } else {
    oam->priority = (QLEN_SIM_MIN_PRIORITY -
                     QLEN_SIM_PRIORITY_INC_RATE * state);

    /* lower hard-limit by 1 */
    oam->priority = (oam->priority < 1) ? 1 : oam->priority;
  }
  oam->timeout = 2 * CLOCK_SECOND;
  oam->bytes = 2 * sizeof(uint16_t);

  LOG_INFO("Generated: P=%d, T=%d, B=%d\n",
           oam->priority, oam->timeout, oam->bytes);
  LOG_INFO("Queue length: inst=%d", state);

  memcpy(&(oam->data), &state, oam->bytes);
  /* strncpy(oam->data,"Datasource",10); */
}
/*---------------------------------------------------------------*/
static void*
get_conf(void)
{
  return NULL;
}
/*---------------------------------------------------------------*/
static void
set_conf(void *conf)
{

}
/*---------------------------------------------------------------*/
void
priority_sim_init(void)
{
  /* register to oam*/
  int mm1q_id = oam_alloc_module();
  register_oam(mm1q_id,MM1Q_PRIORITY,
               get_value,
               reset,
               get_conf, 
               set_conf, 
               start, 
               stop);

  /* Uncomment when running in real deployment */
  /* int seed = RTIMER_NOW();
  random_init(seed); */
  op_init();
}
/*---------------------------------------------------------------*/
#endif /* CONF_QSIM_MODULE */
/*---------------------------------------------------------------*/
