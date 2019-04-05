/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include <stdio.h>
#include <random.h>
#include <string.h>

#if CONF_DUMMY
#include "oam.h"

#include "sys/log.h"
#define LOG_MODULE "Dummy"
#define LOG_LEVEL LOG_LEVEL_DUMMY
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

  oam->timeout = (random_rand() % (5 * CLOCK_SECOND)) + 5000;
  oam->priority = random_rand() % 40;
  oam->bytes = 6;

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
