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
#define ARRVL_RATE 5        //IN PER HOUR
#define SERV_RATE  2
#define TOTAL_TIME 1
#define QUEUE_SIZE 14
#define MM1Q_PRIORITY 10

#define MAX_PROCS 500     /* Don't make this too high (memory) */
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
poisson_process(int rate)
{ 
  int k=0;
  double step = 50;
  double p = 1.0, l;

  do {
    k = k+1;
    l = (random_rand() % RANDOM_RAND_MAX)
            / (RANDOM_RAND_MAX * 1.0);
    p *= l;
    LOG_DBG("[Poisson] Random p: %d\n", p);

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

  printf("Generated Poisson: %d\n", (k-1));
  return (k-1);
}
/*---------------------------------------------------------------*/
/*generate exponential distribution*/
static double
exponential_process(int rate)
{
  float x = (random_rand()%RANDOM_RAND_MAX)/(RANDOM_RAND_MAX*1.0);
  double r = (int)(-log(x)/(1.0 * rate));
  
  printf("Generated Exponential: %d\n", r);
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
  } else if(time == ET[et_iter]) {
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
  printf("Sampling poisson\n");
  len = poisson_process(ARRVL_RATE) * TOTAL_TIME;
  time = 0;

  /* Setup ctimers */
  ctimer_set(&big, (CLOCK_SECOND * TOTAL_TIME * 3600),
             op_init, NULL);
  ctimer_set(&small, (CLOCK_SECOND * SAMPLE_TIME_SEC),
             op, NULL);  
  
  /* Populate Inter-Arrival_Times (IAT)*/
  for(i=0;i<len;i++) {
    printf("Sampling exponential\n");
    temp = exponential_process(1.0/ARRVL_RATE) * 3600.0; 
    IAT[i] = (i != 0) ? (int)floor(temp) : 0;
  }    

  /* Populating Service-Times(ST) (where ST(j)!=0)*/
  while((j!= len))  {
    temp = exponential_process(1.0/SERV_RATE) * 3600.0;
    if(!(int)floor(temp)) {
      ST[j] = (int)floor(temp);
      j++; 
    }
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
  time=0;
  state=0;

  op_init();
}
/*---------------------------------------------------------------*/
static int
start_dummy(void)
{
  reset();
  return 0;
}
/*---------------------------------------------------------------*/
static int
stop_dummy(void)
{
  return 0;
}
/*---------------------------------------------------------------*/
static void
get_value(struct oam_val *oam)
{
  oam->priority = (15 - state);
  oam->timeout *= 2 * CLOCK_SECOND;
  oam->bytes = 2 * sizeof(uint16_t);

  //LOG_INFO("Generated: P=%d, T=%d, B=%d\n",
  //         oam->priority , oam->timeout, oam->bytes);

  memcpy(oam->data, &state, oam->bytes);
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
               start_dummy, 
               stop_dummy);

  /* Uncomment when running in real deployment */
  /* int seed = RTIMER_NOW();
  random_init(seed); */
  reset();
}
/*---------------------------------------------------------------*/
#endif /* CONF_QSIM_MODULE */
/*---------------------------------------------------------------*/
