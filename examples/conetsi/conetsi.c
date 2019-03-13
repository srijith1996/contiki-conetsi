/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"

#include "conetsi.h"
#include "oam.h"
#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"
/*---------------------------------------------------------------------------*/
char conetsi_buf[THRESHOLD_PKT_SIZE];

struct conetsi_node me;
static struct simple_udp_connection nsi_conn;
static uip_ipaddr_t mcast_addr;
static uip_ipaddr_t host_addr;
/*---------------------------------------------------------------------------*/
int
ticks_msec(const int time_msecs) {
  return time_msecs * (CLOCK_SECOND / 1000.0);
}
/*---------------------------------------------------------------------------*/
int
msec(const uint32_t ticks) {
  return (ticks / CLOCK_SECOND) * 1000.0;
}
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
  int i;
  struct conetsi_pkt *buf = (void *) &conetsi_buf;
  struct nsi_demand *demand_buf = (void *) &(buf->data);

  /* These functions are defined in the OAM process */
  buf->type = TYPE_DEMAND_ADVERTISEMENT;
  demand_buf->demand = demand();
  demand_buf->time_left = msec(get_nsi_timeout());
  demand_buf->bytes = get_bytes();

  if(parent != NULL) {
    demand_buf->demand += parent->demand;
    demand_buf->bytes += parent->bytes;
    if(parent->timeout < demand_buf->time_left) {
      demand_buf->time_left = parent->timeout;
    }
  }

  /* Convert to host order */
  HTONS(demand_buf->demand);
  HTONS(demand_buf->time_left);
  HTONS(demand_buf->bytes);

  PRINTF("Demand buffer ");
  for(i=0; i<SIZE_DA; i++) {
    PRINTF("%02x:", *((uint8_t *)&conetsi_buf + i));
  }
  PRINTF("\n");

  PRINTF("Sending DA to ");
  PRINT6ADDR(&mcast_addr);
  PRINTF("\n");
  simple_udp_sendto(&nsi_conn, &conetsi_buf, SIZE_DA, &mcast_addr);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
send_ack(const uip_ipaddr_t *parent)
{
  struct conetsi_pkt *buf = (void *) &conetsi_buf;
  
  buf->type = TYPE_ACK;
  PRINTF("Sending ACK to ");
  PRINT6ADDR(parent);
  PRINTF("\n");
  simple_udp_sendto(&nsi_conn, &conetsi_buf, SIZE_ACK, parent);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
send_join_req(int exp_time)
{
  struct conetsi_pkt *buf = (void *) &conetsi_buf;
  struct join_request *req = (void *) &(buf->data);
  int i;

  buf->type = TYPE_JOIN_REQUEST;
  req->time_left = uip_htons(exp_time);
  uip_ipaddr_copy(&(req->chosen_child), &(me.child_node));

  PRINTF("Request buffer: ");
  for(i=0; i<SIZE_JOIN_REQ; i++) {
    PRINTF("%02x:", *((uint8_t *)buf + i));
  }
  PRINTF("\n");

  PRINTF("Sending JOIN_REQ to ");
  PRINT6ADDR(&mcast_addr); 
  PRINTF("\n");
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

  tmp = TYPE_NSI;
  memcpy(&conetsi_buf, &tmp, 1);
  add_len += 1;

  if(buf != NULL) {
    memcpy(&conetsi_buf + add_len, buf + 1, buf_len - 1);
    add_len += buf_len - 1;
    tmp = *((uint8_t *)(&conetsi_buf + add_len)) + 1;
    PRINTF("Number of hops: %d\n", tmp);

  } else {
    /* if the buf is empty, I must initiate the NSI packet */
    tmp = 1;
    buf_len = 1;
    add_len += 1;
  }
  memcpy((void *)&conetsi_buf + 1, &tmp, 1);

  memcpy((void *)&conetsi_buf + add_len, &uip_lladdr, LINKADDR_SIZE);
  add_len += LINKADDR_SIZE;

  /* Add my NSI data */
  add_len += oam_string((void *)&conetsi_buf + add_len);

  /* Send NSI to parent */
  simple_udp_sendto(&nsi_conn, &conetsi_buf, add_len, &me.parent_node);

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
int
get_backoff(int demand, int timeout_ticks)
{
  /* Strictly lesser than time left */
  PRINTF("Computing backoff: %d, %d\n", demand, timeout_ticks);

  /* wait for the entire duration if I have nothing to send */
  if(demand == 0) {
    return timeout_ticks;
  }
  timeout_ticks /= (BACKOFF_DIV_FACTOR * demand);

  /* Break ties randomly */
  int rand_ticks = (random_rand() % (2 * BACKOFF_RAND_RANGE));
  rand_ticks -= BACKOFF_RAND_RANGE;    /* range [-RAND_RANGE, RAND_RANGE-1] */
  timeout_ticks += rand_ticks;

  return timeout_ticks;
}
/*---------------------------------------------------------------------------*/
