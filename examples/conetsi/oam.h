/*---------------------------------------------------------------------------*/
#ifndef _OAM_H_
#define _OAM_H_
/*---------------------------------------------------------------------------*/
#define PRIORITY_THRESHOLD 30
/*---------------------------------------------------------------------------*/
#define OAM_POLL_INTERVAL   (CLOCK_SECOND * 2)
/*---------------------------------------------------------------------------*/
/* configure module ID here */
#define BAT_VOLT_ID          0
#define FRAME_DROP_RATE_ID   1
#define ETX_ID               2
#define QUEUE_STATE_ID       3
/*---------------------------------------------------------------------------*/
struct oam_stats {
  uint16_t bytes;
  uint16_t exp_time;
  float priority;
};
/*---------------------------------------------------------------------------*/
struct oam_val {
  uint16_t timeout;
  uint16_t priority;
  uint16_t bytes;
  void *data;
};
/*---------------------------------------------------------------------------*/
struct oam_module {
  uint16_t id;
  uint16_t timeout;
  uint16_t bytes;
  float priority;
  
  void (* get_val) (struct oam_val *);
  void (* reset) (void);
  (void *) (* get_config) (void);
  void (* set_config) (void *);
  int (* start) (void);
  int (* stop) (void);

  void *data;
};
#define OAM_ENTRY_BASE_SIZE 4
/*---------------------------------------------------------------------------*/
/* functions for registering and unregistering */
void register_oam(int oam_id, void (* value_callback) (struct oam_val *),
                  void (* reset_callback) (void),
                  void* (* get_conf_callback) (void),
                  void (* set_conf_callback) (void *),
                  int (* start_callback) (void), int (* stop_callback) (void));

void unregister_oam(int oam_id);

uint16_t demand();
uint16_t get_nsi_timeout();
uint16_t get_bytes();
/*---------------------------------------------------------------------------*/
#endif /* _OAM_H_ */
/*---------------------------------------------------------------------------*/
