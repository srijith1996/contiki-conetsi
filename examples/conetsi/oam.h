/*---------------------------------------------------------------------------*/
#ifndef _OAM_H_
#define _OAM_H_
/*---------------------------------------------------------------------------*/
/* threshold defines */
#define THRESHOLD_PRIORITY 30
#define THRESHOLD_DEMAND   150
/*---------------------------------------------------------------------------*/
#define OAM_POLL_INTERVAL   (5 * CLOCK_SECOND)
#define MAX_OAM_ENTRIES     10
/*---------------------------------------------------------------------------*/
/* configured module IDs */
#define BAT_VOLT_ID          0
#define FRAME_DROP_RATE_ID   1
#define ETX_ID               2
#define QUEUE_STATE_ID       3
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
#define DEMAND_FACTOR    CLOCK_SECOND / 100
#define THRESHOLD_PKT_SIZE       90
#define THRESHOLD_TIMEOUT_MSEC   20
#define THRESHOLD_TIMEOUT_TICKS  THRESHOLD_TIMEOUT_MSEC * CLOCK_SECOND / 1000
/*---------------------------------------------------------------------------*/
struct oam_stats {
  uint16_t bytes;
  uint16_t priority;
  struct timer *exp_timer;
};
/*---------------------------------------------------------------------------*/
struct oam_val {
  uint16_t timeout; /* timeout in ticks */
  uint16_t priority;
  uint16_t bytes;
  char data[10];
};
/*---------------------------------------------------------------------------*/
struct oam_module {
  uint16_t id;
  uint16_t bytes;
  uint16_t priority;
  struct ctimer exp_timer;
  
  void (* get_val) (struct oam_val *);
  void (* reset) (void);
  void* (* get_config) (void);
  void (* set_config) (void *);
  int (* start) (void);
  int (* stop) (void);

  void *data;
};
#define OAM_ENTRY_BASE_SIZE 2
/*---------------------------------------------------------------------------*/
/* functions for registering and unregistering */
void register_oam(int oam_id, void (* value_callback) (struct oam_val *),
                  void (* reset_callback) (void),
                  void* (* get_conf_callback) (void),
                  void (* set_conf_callback) (void *),
                  int (* start_callback) (void), int (* stop_callback) (void));

void unregister_oam(int oam_id);

int demand();
uint16_t get_nsi_timeout();
uint16_t get_bytes();

int oam_string(char *buf);
/*---------------------------------------------------------------------------*/
#endif /* _OAM_H_ */
/*---------------------------------------------------------------------------*/
