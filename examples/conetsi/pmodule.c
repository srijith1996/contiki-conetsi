#include <stdio.h>
/*--------------------------------------------------------------------*/
/*for configuringthe poissons process the parameters can be set manually*/ 
#define ARRVL_RATE 5        //IN PER HOUR
#define SERV_RATE  2
#define TOTAL_TIME 1
#define QUEUE_SIZE 14
#ifndef RATE 
#define RATE = ARRVL_RATE
#endif
#include "contiki.h"
#include <stdio.h>
#include <random.h>
#include <string.h>
#include "net/routing/rpl-lite/rpl.h"

#if CONF_DUMMY
#include "oam.h"
 
/*----------------------------------------------------------------------*/
#include "sys/log.h"
#define LOG_MODULE "Dummy"
#define LOG_LEVEL LOG_LEVEL_DUMMY
#define DUMMY_PRIORITY 10
/*----------------------------------------------------------------------*/
#define MAX(a, b) ((a > b) ? a : b)
/*----------------------------------------------------------------------*/
static struct ctimer small;
static struct ctimer big;
static int timer;
static int state;

/*-------------------------------------------------------------------------*/
static void reset(void)     {
  timer=0;
  state=0;

}
/*-------------------------------------------------------------------------*/
static int start_dummy(void) {
   reset();
   op _init(); 
   return 0;
}
/*-------------------------------------------------------------------------*/
static int stop_dummy(void)  {
   return 0;
}
/*-------------------------------------------------------------------------*/
static void get_value(struct oam_value *oam)  {
   oam->priority = state;
   oam->timeout *=2*CLOCK_SECOND;
   oam->bytes = 2*sizeof(uint16_t);
   LOG_INFO("Generated: P=%d, T=%d, B=%d\n", oam->priority , oam->timeout, oam->bytes);
   oam->data = strncpy(&oam->data,"Datasource",10);
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
  dummy_id = oam_alloc_module();
  op_init();
  register_oam(dummy_id,DUMMY_PRIORITY,
               get_value,
               reset,
               get_conf, 
               set_conf, 
               start_dummy, 
               sop_dummy);
}
/*-------------------------------------------------------------------------*/
/*generate poisson's distribution*/
int poisson_process(int rate) { //int ARRVL_RATE, int SERV_RATE, int TOTAL_TIME) {
  int num_process;

  /* TODO: Add logic */
  return num_process;
}
/*-------------------------------------------------------------------------*/
/*generate exponential distribution*/
int exponential_process(int rate){ /*depends strictly on RATE */
  int t;

  /* TODO: Add logic */
  return t;
}
/*-------------------------------------------------------------------------*/
/* Callback functions for ctimers. */
void op_init() {
  int j=0;
  int queue[QUEUE_SIZE],IAT[len-1],ST[len-1],AT[len-1],ET[len-1];
  int len;

  int i;
  
  ctimer_set(&big,TOTAL_TIME*60*60,op_init,NULL);
  ctimer_set(&small,CLOCK_SECOND,op,NULL);  
  
  len = poisson_process(IAT_RATE) * TOTAL_TIME;

  /* Populate Inter-Arrival_Times (IAT)*/
  for (i=0;i<len;i++) {
    /* TODO: check to deal with underflow */
    int temp = exponential_process(1000/IAT_RATE) / 1000; 
  
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
    if(!(int(temp-(temp%1))))      {
      ST[j]=int (temp-(temp%1));
      j++; 
    }
  }
  /*Get arrival-Times (AT) from IAT starting at t=0
    and initializes Waiting-Timings to 0 */
  for(i=0;i<len;i++) {
    if(i=0) {
      AT[0]=0;
    } else {
      AT[i]=AT[i-1]+IAT[i];
    }
  }

  /*Simulation of M/M/1 Queue (i represents current time)*/
  ET[0]=ST[0];
  for(i=0;i<len-1;i++) {
    ET[i+1] = MAX(ET[i], AT[i+1]) + ST[i+1];
  }
}
/*---------------------------------------------------------------*/
void op () {
  ctimer_reset(&small);
  if(time == AT(j)) {
    state++;
    j++;
  } else if(time=ET[k]) {
    state--;
    k++;
  }
  time++;
}
/*---------------------------------------------------------------*/
