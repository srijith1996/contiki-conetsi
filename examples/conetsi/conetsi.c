/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"

#include "conetsi.h"
#include "oam.h"
/*---------------------------------------------------------------------------*/
char conetsi_buf[THRESHOLD_PKT_SIZE];

struct conetsi_node me;
static struct simple_udp_connection conetsi_conn;
static struct simple_udp_connection nms_conn;
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
send_nsi(char *buf, int buf_len)
{
  /* first byte of buf is the length */
  int add_len;

  struct conetsi_pkt *pkt = (void *) &conetsi_buf;

  pkt->type = uip_htons(TYPE_NSI);

  if(buf != NULL) {
    memcpy(&(pkt->data), buf, buf_len);
  } else {
    buf_len = 0;
  }

  memcpy(&(pkt->data) + buf_len, &uip_lladdr, LINKADDR_SIZE);
  add_len = oam_string((char *)&(pkt->data) + buf_len + LINKADDR_SIZE);

  if(uip_ipaddr_cmp(&(me.parent_node), &host_addr)) {
    simple_udp_sendto(&nms_conn, &(pkt->data),
                    (buf_len + add_len + LINKADDR_SIZE), &me.parent_node);
  } else {
    simple_udp_sendto(&conetsi_conn, &conetsi_buf,
                    (buf_len + add_len + LINKADDR_SIZE), &me.parent_node);
  }
  return (buf_len + add_len + LINKADDR_SIZE);
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
