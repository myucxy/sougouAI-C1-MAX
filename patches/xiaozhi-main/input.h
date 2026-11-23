#ifndef C208_AI_MAIN_INPUT_H
#define C208_AI_MAIN_INPUT_H
#include <stdint.h>
enum ai_mode {AI_HOLD,AI_TOGGLE,AI_CONTINUOUS};
enum ai_phase {AI_IDLE,AI_LISTENING,AI_WAITING,AI_SPEAKING};
enum ai_action {AI_START=1,AI_SEND,AI_CANCEL,AI_AUTO,AI_NOT_READY};
struct ai_event {uint32_t generation;unsigned action,automatic;};
struct ai_effects {unsigned count;struct ai_event items[3];};
struct ai_input {uint32_t generation,down_at,last_release;unsigned mode,phase,active,online,down,owned,tap,automatic;};
void ai_init(struct ai_input *,unsigned mode);
void ai_enter(struct ai_input *,int online,struct ai_effects *);
void ai_leave(struct ai_input *,struct ai_effects *);
void ai_set_mode(struct ai_input *,unsigned mode,struct ai_effects *);
void ai_online(struct ai_input *,int online,struct ai_effects *);
int ai_key(struct ai_input *,unsigned key,unsigned subtype,uint32_t now,struct ai_effects *);
void ai_reply(struct ai_input *,uint32_t generation,int speaking,struct ai_effects *);
#endif
