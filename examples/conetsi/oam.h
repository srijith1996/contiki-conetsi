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
struct oam_entry {
  uint16_t id;
  uint16_t timeout;
  uint16_t priority;
  uint16_t bytes;
  void (* get_val) (struct oam_val *)
  void *data;
};
/*---------------------------------------------------------------------------*/
