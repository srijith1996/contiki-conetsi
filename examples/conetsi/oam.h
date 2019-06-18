/*---------------------------------------------------------------------------*/
#ifndef _OAM_H_
#define _OAM_H_
/*---------------------------------------------------------------------------*/
/* threshold defines */
#define THRESHOLD_PRIORITY 40
/*---------------------------------------------------------------------------*/
#define NTOHS(x)  (x = uip_ntohs(x))
#define HTONS(x)  (x = uip_htons(x))
/*---------------------------------------------------------------------------*/
#define OAM_POLL_INTERVAL   (50 * CLOCK_SECOND)
#define MAX_OAM_ENTRIES     10
/*---------------------------------------------------------------------------*/
/* Priority range */
#define LOWEST_PRIORITY     100
#define HIGHEST_PRIORITY      1

#define PRIORITY_LINEAR   0
#define PRIORITY_RAYLEIGH 1

#ifdef CONF_PRIORITY
#if (CONF_PRIORITY == PRIORITY_LINEAR)
#define global_priority(...)  global_priority_lin(__VA_ARGS__)
#elif (CONF_PRIORITY == PRIORITY_RAYLEIGH)
#define global_priority(...)  global_priority_ray(__VA_ARGS__)
#endif /* (CONF_PRIORITY == PRIORITY_LINEAR) */
#else /* CONF_PRIORITY */
#define global_priority(...) global_priority_lin(__VA_ARGS__)
#endif /* CONF_PRIORITY */
/*---------------------------------------------------------------------------*/
#define DEMAND_FACTOR    (CLOCK_SECOND / LOWEST_PRIORITY)
#define THRESHOLD_PKT_SIZE       90
#define THRESHOLD_TIMEOUT_MSEC   100
#define THRESHOLD_TIMEOUT_TICKS ((THRESHOLD_TIMEOUT_MSEC * CLOCK_SECOND)/1000)

#define CONF_TICK_DECR  3
/*---------------------------------------------------------------------------*/
#define MAX_DEMAND       ((DEMAND_FACTOR                            \
                        * THRESHOLD_PKT_SIZE                       \
                        * (LOWEST_PRIORITY - HIGHEST_PRIORITY))    \
                        / THRESHOLD_TIMEOUT_TICKS)
#define THRESHOLD_DEMAND   (MAX_DEMAND / 200)
/* #define THRESHOLD_DEMAND 0 */
/*---------------------------------------------------------------------------*/
struct oam_stats {
  uint8_t bytes;
  uint8_t priority;
  struct timer *exp_timer;
};
/*---------------------------------------------------------------------------*/
struct oam_val {
  uint16_t timeout; /* timeout in ticks */
  uint8_t priority;
  uint8_t bytes;
  char data[10];
};
/*---------------------------------------------------------------------------*/
typedef uint8_t oam_module_id_t;

struct oam_module {
  uint8_t id;
  uint8_t lock;
  uint8_t mod_priority;
  uint8_t bytes;
  uint8_t data_priority;
  struct ctimer exp_timer;
  
  void (* get_val) (struct oam_val *);
  void (* reset) (void);
  void* (* get_config) (void);
  void (* set_config) (void *);
  int (* start) (void);
  int (* stop) (void);

  char data[10];
};
#define OAM_ENTRY_BASE_SIZE 2
/*---------------------------------------------------------------------------*/
/* functions for getting ID registering and unregistering */
oam_module_id_t oam_alloc_module(void);

void register_oam(int id, int mod_p,
                  void (* value_callback) (struct oam_val *),
                  void (* reset_callback) (void),
                  void* (* get_conf_callback) (void),
                  void (* set_conf_callback) (void *),
                  int (* start_callback) (void),
                  int (* stop_callback) (void));

void unregister_oam(int oam_id);

int demand();
uint16_t get_nsi_timeout();
uint16_t get_bytes();

int oam_string(char *buf);
/*---------------------------------------------------------------------------*/
#endif /* _OAM_H_ */
/*---------------------------------------------------------------------------*/
