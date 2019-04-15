/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "net/ipv6/sicslowpan.h"

#include "conetsi.h"
#include "oam.h"

#include "sys/log.h"
#define LOG_MODULE "CoNetSI"
#define LOG_LEVEL LOG_LEVEL_CONETSI
/*---------------------------------------------------------------------------*/
char conetsi_buf[THRESHOLD_PKT_SIZE];
static int child_ct;

struct conetsi_node me;
static struct simple_udp_connection nsi_conn;
static uip_ipaddr_t mcast_addr;
static uip_ipaddr_t host_addr;
/*---------------------------------------------------------------------------*/
void
reg_mcast_addr()
{
  uip_ip6addr(&host_addr, 0xfd00, 0, 0, 0, 0, 0, 0, 0x0001);
  uip_ip6addr(&mcast_addr, 0xff01, 0, 0, 0, 0, 0, 0, 0x0002);
  uip_ds6_maddr_add(&mcast_addr);

  simple_udp_register(&nsi_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);
}
/*---------------------------------------------------------------------------*/
uint32_t
send_demand_adv(struct parent_details *parent)
{
  uint64_t delay, timeout;
  struct conetsi_pkt *buf = (void *) &conetsi_buf;
  struct nsi_demand *demand_buf = (void *) &(buf->data);

  /* These functions are defined in the OAM process */
  buf->type = TYPE_DEMAND_ADVERTISEMENT;
  demand_buf->demand = demand();
  timeout = ticks2rticks(get_nsi_timeout());
  demand_buf->bytes = get_bytes();

  if(parent != NULL) {
    uip_ipaddr_copy(&(demand_buf->parent_addr), &(parent->addr));
    demand_buf->demand += parent->demand;
    demand_buf->bytes += parent->bytes;
    if(parent->timeout < timeout) {
      timeout = parent->timeout;
    }
  } else {
    /* copy host address */
    uip_ipaddr_copy(&(demand_buf->parent_addr), &host_addr);
  }

  LOG_DBG("----- Buffer contents -----\n");
  LOG_DBG("Demand: %d\n", demand_buf->demand);
  LOG_DBG("Bytes: %d\n", demand_buf->bytes);

  LOG_INFO("Sending DA to ");
  LOG_INFO_6ADDR(&mcast_addr);
  LOG_INFO_("\n");

  /* subtract predicted delay incurred in TX */
  delay = sicslowpan_avg_pertx_delay()
          * sicslowpan_queue_len()
          * sicslowpan_avg_tx_count();

  LOG_DBG("Obtained timeout: %d\n", timeout);

  delay += DELAY_GUARD_TIME_RTICKS;
  LOG_DBG("Predicted delay in TX: %d\n", delay);

  timeout = timeout - delay;
  demand_buf->time_left = rticks2msec(timeout);
  LOG_DBG("Timeout: %d\n", demand_buf->time_left);
  LOG_DBG("---------------------------\n");
  HTONS(demand_buf->time_left);

  /* WARN: comment this out if not necessary */
  /* This printing loop will introduce delay */
  /*
  int i;
  LOG_DBG("Demand buffer ");
  for(i=0; i<SIZE_DA; i++) {
    LOG_DBG_("%02x:", *((uint8_t *)&conetsi_buf + i));
  }
  LOG_DBG_("\n");
  */

  simple_udp_sendto(&nsi_conn, &conetsi_buf, SIZE_DA + 1, &mcast_addr);

  return (timeout + delay);
}
/*---------------------------------------------------------------------------*/
int
send_nsi(const uint8_t *buf, int buf_len)
{
  /* first byte of buf is the length */
  uint8_t add_len = 0;
  uint8_t tmp;

  /* buf always contains an addr */
  uint8_t buf_data_start = sizeof(uip_ipaddr_t) + 1;

  LOG_INFO("Sending NSI to ");
  LOG_INFO_6ADDR(&(me.parent_node));
  LOG_INFO_("\n");

  /* copy type information */
  tmp = TYPE_NSI;
  memcpy(&conetsi_buf, &tmp, 1);
  add_len += 1;

  /* copy receiver parent address only if I'm not genesis */
  if(!uip_ipaddr_cmp(&(me.parent_node), &host_addr)) {
    struct nsi_forward *nsi_info = (struct nsi_forward *)(conetsi_buf + 1);
    uip_ipaddr_copy(&(nsi_info->to), &(me.parent_node));
    add_len += sizeof(uip_ipaddr_t);
  }

  if(buf != NULL) {
    memcpy(conetsi_buf + add_len,
           buf + buf_data_start,
           buf_len - buf_data_start);

    /* increment hop count in packet */
    tmp = *((uint8_t *)(conetsi_buf + add_len)) + 1;
    LOG_INFO("Number of hops: %d\n", tmp);
  } else {
    /* if the buf is empty, I must initiate the NSI packet */
    tmp = 1;
    buf_len = 1;
    buf_data_start = 0;
  }

  memcpy(conetsi_buf + add_len, &tmp, 1);
  add_len += buf_len - buf_data_start;

  memcpy(conetsi_buf + add_len, &uip_lladdr, LINKADDR_SIZE);
  add_len += LINKADDR_SIZE;

  /* Add my NSI data */
  add_len += oam_string(conetsi_buf + add_len);

  int i;
  LOG_DBG("Length of NSI: %d\n", add_len);
  LOG_DBG("Sending NSI packet: ");
  for(i = 0; i < add_len; i++) {
    LOG_DBG_("%02x:", *((uint8_t *)conetsi_buf + i));
  }
  LOG_DBG_("\n");

  /* Send NSI to parent */
  if(uip_ipaddr_cmp(&(me.parent_node), &host_addr)) {
    simple_udp_sendto(&nsi_conn, &conetsi_buf, add_len, &host_addr);
  } else {
    simple_udp_sendto(&nsi_conn, &conetsi_buf, add_len, &mcast_addr);
  }

  return add_len;
}
/*---------------------------------------------------------------------------*/
void
set_parent(const uip_ipaddr_t *p)
{
  if(p == NULL) {
    uip_ipaddr_copy(&(me.parent_node), &host_addr);
  } else {
    uip_ipaddr_copy(&(me.parent_node), p);
  }
}
/*---------------------------------------------------------------------------*/
uip_ipaddr_t *
get_parent()
{
  return &(me.parent_node);
}
/*---------------------------------------------------------------------------*/
void
add_child(const uip_ipaddr_t *c)
{
  uip_ipaddr_copy(&(me.child_node[child_ct++]), c);
}
/*---------------------------------------------------------------------------*/
void
rm_child(const uip_ipaddr_t *c)
{
  int i;

  LOG_DBG("Remove child requested (child count: %d, Addr: ", child_ct);
  LOG_DBG_6ADDR(c);
  LOG_DBG_(")\n");

  for(i = 0; i < child_ct; i++) {
    LOG_DBG("child[%d] = ", i);
    LOG_DBG_6ADDR(&(me.child_node[i]));
    LOG_DBG("\n");

    if(uip_ipaddr_cmp(&(me.child_node[i]), c)) {
      LOG_INFO("Removing existing child ");
      LOG_INFO_6ADDR(c);
      LOG_INFO_("\n");

      uip_ipaddr_copy(&(me.child_node[i]), &(me.child_node[child_ct]));
      child_ct--;
      break;
    }
  }
}
/*---------------------------------------------------------------------------*/
int
child_count(void)
{
  return child_ct;
}
/*---------------------------------------------------------------------------*/
uint32_t
get_backoff(uint16_t demand, uint32_t timeout_rticks)
{
  /* Strictly lesser than time left */
  LOG_DBG("Computing backoff: %d, %lu", demand, timeout_rticks);

  timeout_rticks = ((MAX_DEMAND - demand) * timeout_rticks) /
                  (BACKOFF_DIV_FACTOR * MAX_DEMAND);

  /* Break ties randomly */
  int rand_ticks = (random_rand() % (2 * BACKOFF_RAND_RANGE));

  if(timeout_rticks <= 0) {
    timeout_rticks = rand_ticks;
  } else {
    rand_ticks -= BACKOFF_RAND_RANGE;  /* range [-RAND_RANGE, RAND_RANGE-1] */
    timeout_rticks += rand_ticks;
  }

  LOG_DBG_(", %lu\n", timeout_rticks);
  LOG_INFO("Obtained backoff: %lu\n", timeout_rticks);

  return timeout_rticks;
}
/*---------------------------------------------------------------------------*/
