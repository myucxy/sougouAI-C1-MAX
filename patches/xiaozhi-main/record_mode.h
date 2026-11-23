#ifndef C208_RECORD_MODE_H
#define C208_RECORD_MODE_H
#include <stdint.h>
enum {RECORD_MODE=1,XIAOZHI_MODE=2,MUSIC_MODE=3};
enum {RECORD_PAGE=8,AI_HOME_PAGE=59,AI_MENU_PAGE=60,MUSIC_PAGE=61};
unsigned record_home_page(void);
void record_page_ready(unsigned mode);
void record_mode_request(unsigned raw);
int record_navigate(unsigned page);
int record_feature_request(unsigned feature);
void record_mode_poll(void);
unsigned record_mode_active(void);
unsigned record_mode_pending(void);
unsigned record_mode_error(void);
int record_ai_allowed(void);
void record_page_init(void);
const void *record_mode_state(void);
#endif
