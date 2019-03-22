/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include <stdio.h>
#include <random.h>
#include <string.h>

#include "oam.h"
/*---------------------------------------------------------------------------*/
struct dummy {
  uint16_t dummy_val;
  uint16_t max_val;
  uint16_t min_val;
} values;
/*---------------------------------------------------------------------------*/
void
reset()
{
  values.max_val = 0;
  values.min_val = 65535;
}
/*---------------------------------------------------------------------------*/
int
start_dummy()
{
  reset();  
  return 0;
}
/*---------------------------------------------------------------------------*/
int
stop_dummy()
{
  return 0;
}
/*---------------------------------------------------------------------------*/
void
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
void *
get_conf()
{
  return NULL;
}
/*---------------------------------------------------------------------------*/
void
set_conf(void *conf)
{

}
/*---------------------------------------------------------------------------*/
