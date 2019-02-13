/*---------------------------------------------------------------------------*/
#include "conetsi.h"
#include "oam.h"
/*---------------------------------------------------------------------------*/
char conetsi_buf[MAX_OAM_SIZE];

struct conetsi_node me;
static struct simple_udp_connection conetsi_conn;
static uip_ipaddr_t mcast_addr;
/*---------------------------------------------------------------------------*/
int
reg_mcast_addr(const uip_ipaddr_t *ipaddr)
{
  uip_ipaddr_copy(&mcast_addr, ipaddr);
  uip_ds6_maddr_add(ipaddr);

  /* register to listen to incoming CoNetSI connections */
  simple_udp_register(&conetsi_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);
}
/*---------------------------------------------------------------------------*/
int
send_demand_adv()
{
  /* TODO: These functions will be defined
   * in the OAM process
   */
  struct conetsi_pkt *buf = conetsi_buf;
  struct nsi_demand *demand_buf = buf->data;

  buf->type = TYPE_DEMAND_ADVERTISEMENT;
  demand_buf->demand = uip_htons(get_demand());
  demand_buf->time_left = uip_htons(get_nsi_timeout());
  demand_buf->bytes_left = get_bytes();
  /* get_bytes_left(&(demand_buf->bytes_left)); */

  demand_buf->bytes_left = uip_htons(demand_buf->bytes_left);
  simple_udp_sendto(&conetsi_conn, &buf, SIZE_DA, &mcast_addr);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
send_ack(const uip_ipaddr_t *parent)
{
  struct conetsi_pkt *buf = conetsi_buf;
  
  buf->type = TYPE_ACK;
  simple_udp_sendto(&conetsi_conn, &buf, SIZE_ACK, parent);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
send_join_req(const uip_ipaddr_t *child)
{
  struct conetsi_pkt *buf = conetsi_buf;
  struct join_request *req = buf->data;

  buf->type = TYPE_JOIN_REQ;
  uip_ipaddr_copy(&req->chosen_child, child);

  simple_udp_sendto(&conetsi_conn, &buf, SIZE_JOIN_REQ, &mcast_addr);
  return 0;
}
/*---------------------------------------------------------------------------*/
int
send_nsi(char *buf, int buf_len)
{
  /* TODO: Add logic */
}
/*---------------------------------------------------------------------------*/
void
set_parent(struct uip_ipaddr_t *p)
{
  uip_ipaddr_copy(&me.parent_node, p);
}
/*---------------------------------------------------------------------------*/
struct uip_ipaddr_t *
get_parent()
{
  return &me.parent_node;
}
/*---------------------------------------------------------------------------*/
void
set_child(struct uip_ipaddr_t *c)
{
  uip_ipaddr_copy(&me.child_node, c);
}
/*---------------------------------------------------------------------------*/
struct uip_ipaddr_t *
get_child()
{
  return &me.child_node;
}
/*---------------------------------------------------------------------------*/
int
get_backoff(int necessity, int time_left)
{
  /* TODO: Add logic */
}
/*---------------------------------------------------------------------------*/
int
necessity(struct nsi_demand *demand_pkt)
{
  /* TODO: Add logic */
}
/*---------------------------------------------------------------------------*/
