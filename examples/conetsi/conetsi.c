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
int
send_demand_adv(struct parent_details *parent)
{
  uint32_t delay, timeout;
  struct conetsi_pkt *buf = (void *) &conetsi_buf;
  struct nsi_demand *demand_buf = (void *) &(buf->data);

  /* These functions are defined in the OAM process */
  buf->type = TYPE_DEMAND_ADVERTISEMENT;
  demand_buf->demand = demand();
  timeout = ticks2rticks(get_nsi_timeout());
  demand_buf->bytes = get_bytes();

  if(parent != NULL) {
    demand_buf->demand += parent->demand;
    demand_buf->bytes += parent->bytes;
    if(parent->timeout < demand_buf->time_left) {
      timeout = parent->timeout;
    }
  }

  /* Convert to host order */
  HTONS(demand_buf->demand);
  HTONS(demand_buf->bytes);

  LOG_INFO("Sending DA to ");
  LOG_INFO_6ADDR(&mcast_addr);
  LOG_INFO_("\n");

  /* subtract predicted delay incurred in TX */
  delay = sicslowpan_avg_pertx_delay()
          * sicslowpan_queue_len()
          * sicslowpan_avg_tx_count();

  timeout = timeout - delay;
  demand_buf->time_left = rticks2msec(timeout);
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

  simple_udp_sendto(&nsi_conn, &conetsi_buf, SIZE_DA, &mcast_addr);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
send_ack(const uip_ipaddr_t *parent)
{
  struct conetsi_pkt *buf = (void *) &conetsi_buf;
  
  buf->type = TYPE_ACK;
  LOG_INFO("Sending ACK to ");
  LOG_INFO_6ADDR(parent);
  LOG_INFO_("\n");
  simple_udp_sendto(&nsi_conn, &conetsi_buf, SIZE_ACK, parent);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
send_join_req(uint32_t timeout)
{
  struct conetsi_pkt *buf = (void *) &conetsi_buf;
  struct join_request *req = (void *) &(buf->data);
  uint32_t delay;

  LOG_INFO("Sending JOIN_REQ to ");
  LOG_INFO_6ADDR(&mcast_addr);
  LOG_INFO_("\n");

  buf->type = TYPE_JOIN_REQUEST;
  uip_ipaddr_copy(&(req->chosen_child), &(me.child_node));

  /* subtract predicted delay incurred in TX */
  delay = sicslowpan_avg_pertx_delay()
          * (sicslowpan_queue_len() + 1)
          * sicslowpan_avg_tx_count();

  LOG_DBG("Predicted TX delay: %d\n", delay);
  timeout = timeout - delay;
  req->time_left = rticks2msec(timeout);
  HTONS(req->time_left);

  /* WARN: comment this out if not necessary */
  /* This printing loop will introduce delay */
  /*
  int i;
  LOG_DBG("Request buffer: ");
  for(i=0; i<SIZE_JOIN_REQ; i++) {
    LOG_DBG_("%02x:", *((uint8_t *)buf + i));
  }
  LOG_DBG_("\n");
  */

  simple_udp_sendto(&nsi_conn, &conetsi_buf, SIZE_JOIN_REQ, &mcast_addr);
  return 0;
}
/*---------------------------------------------------------------------------*/
int
send_nsi(const uint8_t *buf, int buf_len)
{
  /* first byte of buf is the length */
  uint8_t add_len = 0;
  uint8_t tmp;

  LOG_INFO("Sending NSI to ");
  LOG_INFO_6ADDR(&me.parent_node);
  LOG_INFO_("\n");

  tmp = TYPE_NSI;
  memcpy(&conetsi_buf, &tmp, 1);
  add_len += 1;

  if(buf != NULL) {
    memcpy(conetsi_buf + add_len, buf + 1, buf_len - 1);
    tmp = *((uint8_t *)(conetsi_buf + add_len)) + 1;
    add_len += buf_len - 1;
    LOG_INFO("Number of hops: %d\n", tmp);

  } else {
    /* if the buf is empty, I must initiate the NSI packet */
    tmp = 1;
    buf_len = 1;
    add_len += 1;
  }
  memcpy(conetsi_buf + 1, &tmp, 1);

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
  simple_udp_sendto(&nsi_conn, &conetsi_buf, add_len, &me.parent_node);

  return add_len;
}
/*---------------------------------------------------------------------------*/
int
my_join_req(void *pkt)
{
  struct join_request *req = pkt;
  return uip_ds6_is_my_addr(&(req->chosen_child));
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
set_child(const uip_ipaddr_t *c)
{
  uip_ipaddr_copy(&(me.child_node), c);
}
/*---------------------------------------------------------------------------*/
uip_ipaddr_t *
get_child()
{
  return &(me.child_node);
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

  return (uint16_t)timeout_rticks;
}
/*---------------------------------------------------------------------------*/
