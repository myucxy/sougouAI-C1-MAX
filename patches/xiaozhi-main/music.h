#ifndef C208_MUSIC_H
#define C208_MUSIC_H
#include <stdint.h>
enum {MUSIC_TOGGLE=1,MUSIC_STOP=2,MUSIC_PREVIOUS=4,MUSIC_NEXT=5,MUSIC_REFRESH=6,MUSIC_PREVIOUS_TRACK=7,MUSIC_NEXT_TRACK=8};
enum {MUSIC_REPEAT_ONE,MUSIC_REPEAT_ALL,MUSIC_ORDERED,MUSIC_SHUFFLE};
enum {MUSIC_TITLE,MUSIC_STATUS,MUSIC_NAME,MUSIC_DELETE_NAME};
int music_close(void);
int music_pause_to(unsigned page);
unsigned music_navigation_pending(void);
int music_request_ai(void);
void music_poll(void);
int music_action(unsigned command);
int music_select(unsigned index,unsigned revision);
int music_delete_prepare(unsigned index,unsigned revision);
int music_delete_confirm(unsigned revision);
void music_delete_cancel(void);
unsigned music_delete_state(void);
void music_progress(unsigned out[3]);
void music_text(unsigned field,unsigned index,char *out,unsigned capacity);
unsigned music_count(void);
unsigned music_revision(void);
void music_visible(unsigned visible);
int music_loaded(void);
void music_event(void *player,unsigned info,void *token);
void music_page_init(void);
void music_page_deinit(void);
void music_page_message(const uint8_t *message);
void music_page_gesture(unsigned gesture);
extern const char music_page_name[];
extern const unsigned music_working_bytes;
unsigned music_page_volume_allowed(void);
unsigned music_settings_active(void);
int music_visit_settings(void);
int music_confirm_ai_later(void);
unsigned music_confirmation_pending(void);
void music_keys_event(unsigned type);
void music_keys_poll(void);
void music_keys_reset(void);
unsigned music_page_keys_allowed(void);
unsigned music_play_mode(void);
const char *music_play_mode_name(void);
int music_cycle_play_mode(void);
#endif

