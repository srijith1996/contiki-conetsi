/*---------------------------------------------------------------------------*/
#define OAM_POLL_INTERVAL   (CLOCK_SECOND * 2)
#define OAM_ENTRY_BASE_SIZE 8
/*---------------------------------------------------------------------------*/
struct oam_stats {
  uint16_t bytes;
  uint16_t exp_time;
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
  uint16_t priority;
  uint16_t bytes;
  void (* get_val) (struct oam_val *)
  void *data;
};
/*---------------------------------------------------------------------------*/
/* functions for registering and unregistering */
void register_oam(int oam_id, void (* value_callback) (struct oam_val *),
                  void (* reset_callback) (void),
                  void* (* get_conf_callback) (void),
                  void (* set_conf_callback) (void *),
                  int (* start_callback) (void), int (* stop_callback) (void));

void unregister_oam(int oam_id);
/*---------------------------------------------------------------------------*/
