/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include <stdio.h>
#include <random.h>
#include <string.h>
#include "net/routing/rpl-lite/rpl.h"

#if CONF_DUMMY
#include "oam.h"

#include "sys/log.h"
#define LOG_MODULE "Dummy"
#define LOG_LEVEL LOG_LEVEL_DUMMY
/*---------------------------------------------------------------------------*/
#define DUMMY_PRIORITY 10
/*---------------------------------------------------------------------------*/
extern rpl_rank_t get_current_rank(void);
static int p[2][2] = { {4, 9},
                     {0, 4} };
static int t[2][2] = { {4, 9},
                     {0, 2} };
/*---------------------------------------------------------------------------*/
static oam_module_id_t dummy_id;
struct dummy {
  uint16_t dummy_val;
  uint16_t max_val;
  uint16_t min_val;
} values;
/*---------------------------------------------------------------------------*/
static void
reset(void)
{
  values.max_val = 0;
  values.min_val = 65535;
}
/*---------------------------------------------------------------------------*/
static int
start_dummy(void)
{
  reset();  
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
stop_dummy(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
static void
get_value(struct oam_val *oam)
{
  values.dummy_val = random_rand() % 500;

  if(values.dummy_val > values.max_val) {
    values.max_val = values.dummy_val;
  }
  if(values.dummy_val < values.min_val) {
    values.min_val = values.dummy_val;
  }

  int x;
  if(get_current_rank() < 256) {
    x = 0;
  } else {
    x = 1;
  }

  /* priority is randomly assigned between 1, 10, 40 */
  uint16_t prob = random_rand() / (RANDOM_RAND_MAX / 10);
  if(prob < p[x][0]) {                            /* p = 1/5 */
    oam->priority = 1;
  } else if(prob >= p[x][0] && prob < p[x][1]) {  /* p = 1/2 */
    oam->priority = 3;
  } else {                                        /* p = 3/10 */
    oam->priority = 5;
  }

  /* criticality of data depends on timeout */
  prob = random_rand() / (RANDOM_RAND_MAX / 10);
  if(prob < t[x][0]) {
    oam->timeout = 1;
  } else if(prob >= t[x][0] && prob < t[x][1]) {
    oam->timeout = 3;
  } else {
    oam->timeout = 5;
  }
  oam->timeout *= CLOCK_SECOND;

  oam->bytes = sizeof(struct dummy);

  LOG_INFO("Generated: P=%d, T=%d, B=%d\n", oam->priority,
           oam->timeout, oam->bytes);
  
  memcpy(&oam->data, &values, sizeof(struct dummy));
}
/*---------------------------------------------------------------------------*/
static void *
get_conf(void)
{
  return NULL;
}
/*---------------------------------------------------------------------------*/
static void
set_conf(void *conf)
{

}
/*---------------------------------------------------------------------------*/
void
dummy_init(void)
{
  /* register to OAM */
  dummy_id = oam_alloc_module();
  register_oam(dummy_id, DUMMY_PRIORITY,
               get_value,
               reset,
               get_conf,
               set_conf,
               start_dummy,
               stop_dummy);

  reset();
}
/*---------------------------------------------------------------------------*/
#endif /* CONF_DUMMY */
/*---------------------------------------------------------------------------*/
