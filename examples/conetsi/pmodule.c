#include <stdio.h>
#include "contiki.h"
#include <stdio.h>
#include <random.h>
#include <string.h>
#include "net/routing/rpl-lite/rpl.h"
#include <math.h>
#if CONF_DUMMY
#include "oam.h"
/*----------------------------------------------------------------------*/
#include "sys/log.h"
/*--------------------------------------------------------------------*/
/*for configuringthe poissons process the parameters can be set manually*/ 
#define ARRVL_RATE 5        //IN PER HOUR
#define SERV_RATE  2
#define TOTAL_TIME 1
#define QUEUE_SIZE 14
#define LOG_MODULE "Dummy"
#define LOG_LEVEL LOG_LEVEL_DUMMY
#define DUMMY_PRIORITY 10

#define MAX_PROCS 1000
/*----------------------------------------------------------------------*/
#define MAXIMUM(a, b) ((a > b) ? a : b)
/*----------------------------------------------------------------------*/
static struct ctimer small;
static struct ctimer big;
static int timer;
static int state;
static int time;
static int at_iter, et_iter;

/* simulation vars */
static int len;
static int IAT[MAX_PROCS], ST[MAX_PROCS],
           AT[MAX_PROCS], ET[MAX_PROCS];
/*-------------------------------------------------------------------------*/
void op_init(); 
/*-------------------------------------------------------------------------*/
static void reset(void)     {
  timer=0;
  state=0;

}
/*-------------------------------------------------------------------------*/
static int start_dummy(void) {
   reset();
   op_init(); 
   return 0;
}
/*-------------------------------------------------------------------------*/
static int stop_dummy(void)  {
   return 0;
}
/*-------------------------------------------------------------------------*/
static void get_value(struct oam_val *oam)  {
   oam->priority = state;
   oam->timeout *=2*CLOCK_SECOND;
   oam->bytes = 2*sizeof(uint16_t);
   LOG_INFO("Generated: P=%d, T=%d, B=%d\n", oam->priority , oam->timeout, oam->bytes);
   strncpy((oam->data),"Datasource",10);
}
/*-------------------------------------------------------------------------*/
static void *get_conf(void)  {
    return NULL;
}
/*-------------------------------------------------------------------------*/
static void set_conf(void *conf)
{

}
/*-------------------------------------------------------------------------*/
void priority_sim_init(void) {
  /* register to oam*/
  op_init();
 int dummy_id = oam_alloc_module();
  register_oam(dummy_id,DUMMY_PRIORITY,
               get_value,
               reset,
               get_conf, 
               set_conf, 
               start_dummy, 
               stop_dummy);
  reset();
}
/*-------------------------------------------------------------------------*/
/*generate poisson's distribution*/
int poisson_process(int rate) { 
   int k = 0,l;
   l=pow(M_E, rate);
   long p; 
   do{
      k++;
      p *= random_rand()%65536;
      } while (p<l);  
   
   
   return k;   
}
/*-----------------------------------------------------------------------*/
/*generate exponential distribution*/
int exponential_process(int rate) {       
    int x = random_rand()%65536;
    int r =(int)((1/rate)*(log(x-1)));
  
  return r;
}
/*---------------------------------------------------------------*/
void op () {
  ctimer_reset(&small);
  if(time == AT[at_iter]) {
    state++;
    at_iter++;
  } else if(time == ET[et_iter]) {
    state--;
    et_iter++;
  }
  time++;
}
/*-------------------------------------------------------------------------*/
/* Callback functions for ctimers. */
void op_init() {

  int j = 0, i = 0, temp = 0;

  /* sample total time */
  len = poisson_process(ARRVL_RATE) * TOTAL_TIME;

  time = 0;

  ctimer_set(&big,TOTAL_TIME*60*60,op_init,NULL);
  ctimer_set(&small,CLOCK_SECOND,op,NULL);  
  
  /* Populate Inter-Arrival_Times (IAT)*/
  for (i=0;i<len;i++) {
    /* TODO: check to deal with underflow */
    temp = exponential_process(1/ARRVL_RATE); 
  
    IAT[i] = (i != 0) ? (int) (temp-(temp%1)) : 0;
    /*
    if(i==0) {
      IAT[0]=0; 
    } else {
      IAT[i] = (int) (temp-(temp%1)) ; 
    }
    */
  }    
    /* Populating Service-Times(ST) (where ST(j)!=0)*/
  while((j!= len))  {
    /* TODO: check to deal with underflow */
    temp = exponential_process(1/SERV_RATE);
    if(!((int)(temp-(temp%1)))) {
      ST[j] = (int) (temp-(temp%1));
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
    ET[i+1] = MAXIMUM(ET[i], AT[i+1]) + ST[i+1];
  }
}
/*---------------------------------------------------------------*/
#endif /* CONF_DUMMY */
