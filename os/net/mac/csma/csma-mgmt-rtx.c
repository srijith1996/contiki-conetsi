/*---------------------------------------------------------------------------*/
#include <string.h>
#include "contiki.h"
#include "net/mac/csma/csma-mgmt.h"
#include "net/mac/csma/csma-output.h"
#include "net/queuebuf.h"

#if CSMA_MGMT
#include "oam.h"

#include "sys/log.h"
#define LOG_MODULE "CSMA RTX"
#define LOG_LEVEL LOG_LEVEL_CSMA_MGMT_RTX
/*---------------------------------------------------------------------------*/
#define STOPPED 0
#define STARTED 1
/*---------------------------------------------------------------------------*/
#define CSMA_MGMT_RTX_PRIORITY 5
#define RTX_DEFAULT_MIN  8
/*---------------------------------------------------------------------------*/
static oam_module_id_t csma_mgmt_rtx_id;
static struct csma_rtx_data {
  uint8_t last;
  uint8_t avg;
  uint8_t max;
  uint8_t min;
  uint16_t count;
  uint16_t pad;
} rtx_data;

static int ct = 0;
static uint8_t status;
/*---------------------------------------------------------------------------*/
void
csma_mgmt_rtx_record(int rtx)
{
  if(status != STARTED) {
    return;
  }
  int tmp = ct * rtx_data.avg + rtx;
  tmp /= ++ct;
  rtx_data.last = rtx;
  rtx_data.avg = tmp;
  rtx_data.max = (rtx_data.max < rtx) ? rtx : rtx_data.max;
  rtx_data.min = (rtx_data.min > rtx) ? rtx : rtx_data.min;
  rtx_data.count = ct;
}
/*---------------------------------------------------------------------------*/
static void
value(struct oam_val *val)
{
  if(rtx_data.count > 10 || rtx_data.last > 4) {

    /* TODO: Timeout and priority are roughly assigned */
    val->timeout = 2 * CLOCK_SECOND;
    val->priority = 10 - rtx_data.last;
    val->bytes = sizeof(struct csma_rtx_data) - 2;

    struct csma_rtx_data *out = (struct csma_rtx_data *) val->data;

    LOG_INFO("last=%d, avg=%d, max=%d, min=%d, count=%d\n",
             rtx_data.last, rtx_data.avg, rtx_data.max,
             rtx_data.min, rtx_data.count); 

    /* Send values in network order */
    out->last = rtx_data.last;
    out->avg = rtx_data.avg;
    out->max = rtx_data.max;
    out->min = rtx_data.min;
    out->count = uip_htons(rtx_data.count);
  }

  int i;
  LOG_DBG("Data sent -  ");
  for(i=0; i<sizeof(struct oam_val); i++) {
    LOG_DBG_("%02x:", *((uint8_t *)val + i));
  }
  LOG_DBG_("\n");

  return;
}
/*---------------------------------------------------------------------------*/
static void
reset(void)
{
  ct = 0;
  rtx_data.avg = 0;
  rtx_data.max = 0;
  rtx_data.min = RTX_DEFAULT_MIN;

  status = STARTED;
}
/*---------------------------------------------------------------------------*/
static int
start(void)
{
  reset(); 
  return status;
}
/*---------------------------------------------------------------------------*/
static int
stop(void)
{
  status = STOPPED;
  return status;
}
/*---------------------------------------------------------------------------*/
static void
set_conf(void *conf)
{
  return;
}
/*---------------------------------------------------------------------------*/
static void *
get_conf(void)
{
  void *conf = NULL;

  return conf;
}
/*---------------------------------------------------------------------------*/
void
csma_mgmt_rtx_init(void)
{
  /* register to OAM */
  csma_mgmt_rtx_id = oam_alloc_module();
  register_oam(csma_mgmt_rtx_id, CSMA_MGMT_RTX_PRIORITY,
               value,
               reset,
               get_conf,
               set_conf,
               start,
               stop);

  reset();
}
/*---------------------------------------------------------------------------*/
#endif /* CSMA_MGMT */
/*---------------------------------------------------------------------------*/
