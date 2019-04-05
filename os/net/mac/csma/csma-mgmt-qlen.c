/*---------------------------------------------------------------------------*/
#include <string.h>
#include "contiki.h"
#include "net/mac/csma/csma-mgmt.h"
#include "net/mac/csma/csma-output.h"
#include "net/queuebuf.h"

#if CSMA_MGMT
#include "oam.h"

#include "sys/log.h"
#define LOG_MODULE "CSMA Q-Len"
#define LOG_LEVEL LOG_LEVEL_CSMA_MGMT_QLEN
/*---------------------------------------------------------------------------*/
#define STOPPED 0
#define STARTED 1
/*---------------------------------------------------------------------------*/
#define CSMA_MGMT_QLEN_PRIORITY 5
#define QLEN_DEFAULT_MIN QUEUEBUF_NUM
/*---------------------------------------------------------------------------*/
static oam_module_id_t csma_mgmt_qlen_id;
static struct csma_qlen_data {
  uint8_t last;
  uint8_t avg;
  uint8_t max;
  uint8_t min;
  uint16_t count;
  uint16_t pad;
} qlen_data;

static int ct = 0;
static uint8_t status;
/*---------------------------------------------------------------------------*/
void
csma_mgmt_qlen_record(int len)
{
  if(status != STARTED) {
    return;
  }
  int tmp = ct * qlen_data.avg + len;
  tmp /= ++ct;
  qlen_data.last = len;
  qlen_data.avg = tmp;
  qlen_data.max = (qlen_data.max < len) ? len : qlen_data.max;
  qlen_data.min = (qlen_data.min > len) ? len : qlen_data.min;
  qlen_data.count = ct;
}
/*---------------------------------------------------------------------------*/
static void
value(struct oam_val *val)
{
  val->timeout = 5 * CLOCK_SECOND;
  val->priority = 10;
  val->bytes = sizeof(struct csma_qlen_data) - 2;

  struct csma_qlen_data *out = (struct csma_qlen_data *) val->data;

  LOG_INFO("last=%d, avg=%d, max=%d, min=%d, count=%d\n",
           qlen_data.last, qlen_data.avg, qlen_data.max,
           qlen_data.min, qlen_data.count); 

  /* Send values in network order */
  out->last = qlen_data.last;
  out->avg = qlen_data.avg;
  out->max = qlen_data.max;
  out->min = qlen_data.min;
  out->count = uip_htons(qlen_data.count);

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
  qlen_data.avg = 0;
  qlen_data.max = 0;
  qlen_data.min = QLEN_DEFAULT_MIN;

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
csma_mgmt_qlen_init(void)
{
  /* register to OAM */
  csma_mgmt_qlen_id = oam_alloc_module();
  register_oam(csma_mgmt_qlen_id, CSMA_MGMT_QLEN_PRIORITY,
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
