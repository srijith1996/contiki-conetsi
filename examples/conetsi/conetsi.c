/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"

#include "conetsi.h"
#include "oam.h"
/*---------------------------------------------------------------------------*/
char conetsi_buf[THRESHOLD_PKT_SIZE];

struct conetsi_node me;
static struct simple_udp_connection conetsi_conn;
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

  /* register to listen to incoming CoNetSI connections */
  simple_udp_register(&conetsi_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  simple_udp_register(&nsi_conn, UDP_SERVER_PORT, &host_addr,
                      UDP_CLIENT_PORT, NULL);
}
/*---------------------------------------------------------------------------*/
int
send_demand_adv()
{
  struct conetsi_pkt *buf = (void *) &conetsi_buf;
  struct nsi_demand *demand_buf = (void *) &(buf->data);

  /* These functions are defined in the OAM process */
  buf->type = uip_htons(TYPE_DEMAND_ADVERTISEMENT);
  demand_buf->demand = uip_htons(demand());
  demand_buf->time_left = uip_htons(get_nsi_timeout());
  demand_buf->bytes = uip_htons(get_bytes());

  simple_udp_sendto(&conetsi_conn, &buf, SIZE_DA, &mcast_addr);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
send_ack(const uip_ipaddr_t *parent)
{
  struct conetsi_pkt *buf = (void *) &conetsi_buf;
  
  buf->type = TYPE_ACK;
  simple_udp_sendto(&conetsi_conn, &buf, SIZE_ACK, parent);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
send_join_req(int exp_time)
{
  struct conetsi_pkt *buf = (void *) &conetsi_buf;
  struct join_request *req = (void *) &(buf->data);

  buf->type = TYPE_JOIN_REQUEST;
  req->time_left = exp_time;
  uip_ipaddr_copy(&(req->chosen_child), &(me.child_node));

  simple_udp_sendto(&conetsi_conn, &buf, SIZE_JOIN_REQ, &mcast_addr);
  return 0;
}
/*---------------------------------------------------------------------------*/
int
send_nsi(const uint8_t *buf, int buf_len)
{
  /* first byte of buf is the length */
  uint8_t add_len = 0;
  uint8_t tmp;
  int i;

  tmp = TYPE_NSI;
  memcpy(&conetsi_buf, &tmp, 1);
  add_len += 1;

  if(buf != NULL) {
    memcpy(&conetsi_buf + add_len, buf + 1, buf_len - 1);
    add_len += buf_len - 1;
    tmp = *((uint8_t *)(&conetsi_buf + add_len)) + 1;
    printf("Number of hops: %d\n", tmp);

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
float
get_backoff(int demand, int time_left)
{
  /* Strictly lesser than time left */
  return BACKOFF_FACTOR * time_left * demand;
}
/*---------------------------------------------------------------------------*/
