/* AI shell integration. Voice readiness stays false until a real session binds. */
#include "input.h"
#include "record_mode.h"
#include "music.h"
#include "volume_sheet.h"
#include "addresses.h"
#include "../xiaozhi-official/session.h"
#include "../xiaozhi-official/clock.h"
#include "../xiaozhi-official/platform.h"
#include "../xiaozhi-official/official.h"
#include <stddef.h>
#include <string.h>
#include "happy_frames.h"
#include "battery.h"
#include "wave_frames.h"
#include "mode_font.h"
#include "settings_icons.h"
#define FN(ret,a,...) ((ret(*)(__VA_ARGS__))((uintptr_t)(a)|1u))
#define U8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define U16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define U32(a) (*(volatile uint32_t *)(uintptr_t)(a))
struct ai_native {uint32_t magic;struct ai_input input;void *root,*mode,*status,*code,*footer,*face,*animation,*wave;unsigned page,effects,not_ready,feedback,feedback_until,frame,animation_ticks,caption_page,caption_revision,caption_ticks;char official_status[128],activation_code[33],caption[96];void *battery;uint16_t *battery_pixels;struct {uint32_t header,bytes;const void *pixels;} battery_image;unsigned battery_percent,battery_charge,wake_reconnect;void *mode_picker,*mode_summary,*small_style,*mode_options[3];unsigned mode_picker_open;void *menu_style;};
static struct ai_native state;
/* The stock sleep-inhibit flag is also set by Wi-Fi settings; track PMIC callbacks separately. */
static volatile unsigned power_connected;
unsigned ai_battery_charging(void){return power_connected;}
void ai_power_connected(void){power_connected=1;FN(void,0x1805156c,unsigned)(1);}
void ai_power_disconnected(void){power_connected=0;FN(void,0x1805156c,unsigned)(0);}
const unsigned ai_state_size=sizeof(struct ai_native);
const char ai_home_name[]="XIAOZHI HOME";
const char ai_menu_name[]="XIAOZHI MENU";
extern int ai_original_key(unsigned,unsigned);
extern int ai_original_app_message(const uint8_t *);
static unsigned irq(void){return FN(unsigned,0x1802b134,void)();}
static void restore(unsigned v){FN(void,0x1802b13c,unsigned)(v);}
static void init(void){
 if(state.magic==0x31494158u)return;
 unsigned k=irq();
 if(state.magic!=0x31494158u){state=(struct ai_native){0};ai_init(&state.input,AI_HOLD);state.magic=0x31494158u;}
 restore(k);
}
static void effects(const struct ai_effects *e){
 /* No audio action is reported as successful before transport/audio integration. */
 for(unsigned i=0;i<e->count;i++){state.effects++;if(e->items[i].action==AI_NOT_READY)state.not_ready=1;else xs_voice_request(e->items[i].action,e->items[i].generation,e->items[i].automatic);}
}
static void changed(void){if(state.page==AI_HOME_PAGE)FN(int,0x1805b5e8,int,int,int)(252,0,0);}
void ai_official_notify(void){changed();}
static unsigned reconnect_needed(void){unsigned s=xs_state_value();return (s==XS_FAILED||s==XS_CLOSED)&&!xp_usb();}
static void official_click(void *object,unsigned event){(void)object;if(event==6&&reconnect_needed()){state.wake_reconnect=0;xs_start();changed();}}
static void reconnect_after_wake(void){
 if(!state.wake_reconnect)return;
 if(state.page!=AI_HOME_PAGE||!state.root||U16(0x20011dda)!=AI_HOME_PAGE||!U8(0x20011c2d)||!record_ai_allowed()||xp_usb()){
  state.wake_reconnect=0;return;
 }
 unsigned connection=xs_state_value();
 if(connection!=XS_IDLE&&connection!=XS_FAILED&&connection!=XS_CLOSED){state.wake_reconnect=0;return;}
 if(!xp_network_ready()||!xs_quiescent())return;
 /* One wake requests one attempt; existing UI ticks handle Wi-Fi/cleanup delays. */
 state.wake_reconnect=0;xs_start();
}
static void home(void){FN(int,0x1805b53c,unsigned,unsigned)(AI_HOME_PAGE,1);}
static void menu(void){FN(int,0x1805b53c,unsigned)(AI_MENU_PAGE);}
static void mode_picker_back(void);
static void gesture(unsigned value){if(volume_sheet_gesture(value))return;if(state.page==AI_HOME_PAGE&&value==6){volume_sheet_request(state.root);return;}if(state.page==AI_MENU_PAGE&&state.mode_picker_open){if(value==4||value==5)mode_picker_back();return;}if(record_mode_pending())return;if(state.page==AI_HOME_PAGE&&value==5)menu();else if(state.page==AI_HOME_PAGE&&value==4)record_feature_request(MUSIC_MODE);else if(state.page==AI_MENU_PAGE&&value==4)home();else if(state.page==AI_MENU_PAGE&&value==5)record_navigate(MUSIC_PAGE);}
static void text(void *label,const char *value){
 if(!label)return;
 const char *old=FN(const char *,0x180975fc,void *)(label);
 if(!old||strcmp(old,value))FN(void,0x18098084,void *,const char *)(label,value);
}
static void visible(void *object,unsigned show){if(object)FN(void,0x1808970c,void *,unsigned)(object,!show);}
static unsigned feedback_active(void){return (state.feedback==1&&state.input.down)||(state.feedback&&(int32_t)(state.feedback_until-FN(unsigned,0x1806161c,void)())>0);}
static void refresh(void){
 if(state.page!=AI_HOME_PAGE||!state.root)return;
 if(state.battery&&state.battery_pixels){
  unsigned percent=FN(unsigned,0x18051320,void)(),charge=power_connected;
  if(percent!=state.battery_percent||charge!=state.battery_charge){
   state.battery_percent=percent;state.battery_charge=charge;
   ai_battery_draw(state.battery_pixels,percent,charge?AI_BATTERY_CHARGING:AI_BATTERY_DISCHARGING);
   FN(void,0x18096d38,void *,const void *)(state.battery,&state.battery_image);
  }
 }
 struct xv_status voice;xs_voice_status(&voice);
 unsigned key=irq();
 if(voice.ready&&state.input.active&&voice.generation==state.input.generation){state.input.phase=voice.phase==XV_CAPTURE?AI_LISTENING:voice.phase==XV_WAITING?AI_WAITING:voice.phase==XV_PLAYING?AI_SPEAKING:AI_IDLE;}
 if(voice.ready&&voice.automatic&&state.input.automatic){state.input.generation=voice.generation;state.input.phase=voice.phase==XV_CAPTURE?AI_LISTENING:voice.phase==XV_PLAYING?AI_SPEAKING:AI_WAITING;}
 if(state.input.online!=voice.ready){struct ai_effects e;ai_online(&state.input,voice.ready,&e);effects(&e);}restore(key);
 unsigned activation=xs_state_value()==XS_ACTIVATION,feedback=feedback_active()&&!voice.ready&&xs_state_value()!=XS_FAILED&&xs_state_value()!=XS_CLOSED;
 text(state.mode,activation?"等待关联":state.input.mode==AI_HOLD?"对讲模式":state.input.mode==AI_CONTINUOUS?"连续对话":"按键模式");
 xs_activation_code(state.activation_code,sizeof(state.activation_code));
 unsigned show_code=activation&&state.activation_code[0],pages=0,kind=0;
 unsigned revision=xs_caption_page(state.caption,sizeof(state.caption),state.caption_page,&pages,&kind);
 if(revision!=state.caption_revision){state.caption_revision=revision;state.caption_page=0;state.caption_ticks=FN(unsigned,0x1806161c,void)();if(revision)xs_caption_page(state.caption,sizeof(state.caption),0,&pages,&kind);}
 visible(state.code,show_code);visible(state.face,!show_code);
 /* State animation is intentionally independent of PCM amplitude. */
 unsigned wave=voice.ready&&voice.phase==XV_CAPTURE&&!show_code;
 visible(state.wave,wave);
 if(wave&&state.wave)FN(void,0x18096d38,void *,const void *)(state.wave,&ai_wave_images[(FN(unsigned,0x1806161c,void)()/AI_EMOJI_PERIOD_MS)%AI_WAVE_FRAMES]);
 if(show_code){text(state.code,state.activation_code);text(state.status,"小智后台添加设备\n输入上方激活码");}
 else if(activation)text(state.status,"等待服务器激活码\n请稍候");
 else if(xs_state_value()==XS_FAILED||xs_state_value()==XS_CLOSED){xs_status(state.official_status,sizeof(state.official_status));text(state.status,state.official_status);}
 else if(voice.ready&&voice.phase==XV_CAPTURE){text(state.mode,"正在收音");text(state.status,voice.automatic?"说完自动发送":state.input.mode==AI_HOLD?"松开发送":"再按一次发送");}
 else if(revision){text(state.mode,kind==XO_EVENT_STT?"你说的话":"小智回复");text(state.status,state.caption);}
 else if(feedback)text(state.status,state.feedback==1?"按键已响应\n语音尚未接通":"按键已松开\n语音尚未接通");
 else {xs_status(state.official_status,sizeof(state.official_status));text(state.status,state.official_status);}
 unsigned retry=reconnect_needed();text(state.footer,retry?"点击重连":"");visible(state.footer,retry);if(state.footer)FN(void,0x180885bc,void *,unsigned)(state.footer,retry);
}
static void *home_label(const char *value,unsigned y){
 void *label=FN(void *,0x18098300,void *,void *)(state.root,0);if(!label)return 0;
 void *title=(void *)(uintptr_t)U32(0x20011dd0);
 if(title){void *style=FN(void *,0x1808a218,void *)(title);if(style)FN(void,0x1808969c,void *,void *)(label,style);}
 /* BREAK derives the height from the real 30px font, including baseline/spacing. */
 FN(void,0x180981f0,void *,unsigned)(label,1);
 FN(void,0x18089c34,void *,unsigned)(label,224);
 FN(void,0x18097548,void *,unsigned)(label,1);
 FN(void,0x180885bc,void *,unsigned)(label,0);
 text(label,value);FN(void,0x18088eb0,void *,int,int)(label,8,y);return label;
}
void ai_animation_tick(void *task){
 (void)task;reconnect_after_wake();if(state.page!=AI_HOME_PAGE||!state.root||!U8(0x20011c2d))return;
 unsigned now=FN(unsigned,0x1806161c,void)();if(state.caption_revision&&(unsigned)(now-state.caption_ticks)>2500u){state.caption_ticks=now;state.caption_page++;}
 unsigned fast=state.input.down||feedback_active();
 if(state.face&&(fast||++state.animation_ticks>=3)){
  state.animation_ticks=0;state.frame=(state.frame+1)%AI_EMOJI_FRAMES;
  FN(void,0x18096d38,void *,const void *)(state.face,&ai_happy_images[state.frame]);
  FN(void,0x18088eb0,void *,int,int)(state.face,88,fast?(state.frame&1?42:44):44);
 }
 refresh();
}
static void subtitle_click(void *object,unsigned event){(void)object;if(event==6&&state.caption_revision){state.caption_page++;state.caption_ticks=FN(unsigned,0x1806161c,void)();refresh();}}
static const char *mode_name(unsigned mode){return mode==AI_HOLD?"按住说话":mode==AI_TOGGLE?"单击开停":"连续对话";}
static void menu_bounds(void *object,int x,int y,unsigned width,unsigned height){
 FN(void,0x18089b70,void *,unsigned,unsigned)(object,width,height);FN(void,0x18088eb0,void *,int,int)(object,x,y);
}
static void *menu_label(void *parent,const char *value,int x,int y,unsigned width,unsigned height,unsigned small,void *callback){
 void *object=FN(void *,0x18098300,void *,void *)(parent,0);if(!object)return 0;
 void *header=(void *)(uintptr_t)U32(0x20011dd0);void *style=header?FN(void *,0x1808a218,void *)(header):0;
 if(!state.menu_style&&style){
  state.menu_style=FN(void *,0x1805b83c,unsigned)(FN(unsigned,0x1805b7f4,void *)(style));
  if(state.menu_style){U16((uintptr_t)state.menu_style+0x20)=U16(0x180e0f44);U32((uintptr_t)state.menu_style+0x24)=0x2000d2c4u;}
 }
 if(state.menu_style)style=state.menu_style;
 if(small){
  ai_mode_font_init();
  if(!state.small_style&&style){state.small_style=FN(void *,0x1805b83c,unsigned)(FN(unsigned,0x1805b7f4,void *)(style));if(state.small_style)U32((uintptr_t)state.small_style+0x24)=(uint32_t)(uintptr_t)&ai_mode_font;}
  style=state.small_style;
 }
 if(style)FN(void,0x1808969c,void *,void *)(object,style);
 FN(void,0x180981f0,void *,unsigned)(object,2);FN(void,0x18097548,void *,unsigned)(object,0);
 FN(void,0x180885bc,void *,unsigned)(object,callback!=0);menu_bounds(object,x,y,width,height);text(object,value);
 if(callback)FN(void,0x18088758,void *,void *)(object,callback);return object;
}
static void mode_summary(void){
 text(state.mode_summary,state.input.mode==AI_HOLD?"当前：按住说话":state.input.mode==AI_TOGGLE?"当前：单击开停":"当前：连续对话");
}
static void mode_picker_back(void){state.mode_picker_open=0;visible(state.mode_picker,0);}
static void mode_back_click(void *object,unsigned event){(void)object;if(event==6)mode_picker_back();}
static int menu_mode_allowed(void){return record_ai_allowed()||music_settings_active();}
static void mode_choose(void *object,unsigned event){
 if(event!=6||state.page!=AI_MENU_PAGE||!state.mode_picker_open||U16(0x20011dda)!=AI_MENU_PAGE||!menu_mode_allowed()||xp_usb())return;
 unsigned mode=U32((uintptr_t)object+0x4c);if(mode>AI_CONTINUOUS)return;
 struct ai_effects e;unsigned k=irq();ai_set_mode(&state.input,mode,&e);effects(&e);restore(k);
 mode_summary();mode_picker_back();
}
static void mode_click(void *object,unsigned event){
 (void)object;if(event!=6||state.page!=AI_MENU_PAGE||U16(0x20011dda)!=AI_MENU_PAGE||state.mode_picker_open||!menu_mode_allowed()||xp_usb())return;
 if(!state.mode_picker){
  void *panel=FN(void *,0x180897c0,void *,void *)(state.root,0);if(!panel)return;
  FN(void,0x1808969c,void *,void *)(panel,FN(void *,0x1808a218,void *)(state.root));menu_bounds(panel,0,0,240,240);
  FN(void,0x180885bc,void *,unsigned)(panel,1);state.mode_picker=panel;
  if(!menu_label(panel,"返回",8,2,64,30,0,mode_back_click)||!menu_label(panel,"对话模式",88,2,144,30,0,0))goto failed;
  for(unsigned mode=0;mode<=AI_CONTINUOUS;mode++){
   unsigned y=48+mode*62;void *option=menu_label(panel,mode_name(mode),8,y,224,36,0,mode_choose);if(!option)goto failed;
   U32((uintptr_t)option+0x4c)=mode;state.mode_options[mode]=option;
   void *note=menu_label(panel,mode==AI_HOLD?"按住录音键说话":mode==AI_TOGGLE?"点击开始或发送":"单击开始或结束",8,y+34,224,18,1,mode_choose);if(!note)goto failed;U32((uintptr_t)note+0x4c)=mode;
  }
 }
 for(unsigned mode=0;mode<=AI_CONTINUOUS;mode++){
  text(state.mode_options[mode],mode==state.input.mode?(mode==AI_HOLD?"已选：按住说话":mode==AI_TOGGLE?"已选：单击开停":"已选：连续对话"):mode_name(mode));
 }
 state.mode_picker_open=1;visible(state.mode_picker,1);return;
failed:
 FN(void,0x1808a0d8,void *)(state.mode_picker);state.mode_picker=0;
}
static void settings_click(void *object,unsigned event){(void)object;if(event==6&&!state.mode_picker_open)FN(int,0x1805b5b8,unsigned,unsigned,void *)(13,1,0);}
static void build_page(unsigned page){
 init();state.wake_reconnect=0;state.mode_picker=state.mode_summary=state.small_style=state.menu_style=0;state.mode_picker_open=0;state.page=page;state.mode=state.status=state.code=state.footer=state.face=state.animation=0;state.feedback=state.frame=state.animation_ticks=state.caption_page=state.caption_revision=0;
 state.root=FN(void *,0x1805afe8,unsigned,void (*)(void))(42,page==AI_HOME_PAGE?0:home);
 if(!state.root)return;
 if(page==AI_HOME_PAGE)volume_sheet_bind(state.root);
 FN(void,0x1805b1a0,const char *)(page==AI_HOME_PAGE?"小智AI":"设置");
 if(page==AI_HOME_PAGE){
  /* The stock 24px title clips this 30px font too; draw our own heading. */
  visible((void *)(uintptr_t)U32(0x20011dd0),0);
  void *back=(void *)(uintptr_t)U32(0x20011dcc);
  if(back){FN(void,0x180885bc,void *,unsigned)(back,0);FN(void,0x1808970c,void *,unsigned)(back,1);}
  struct ai_effects e;unsigned k=irq();struct xv_status voice;xs_voice_status(&voice);ai_enter(&state.input,voice.ready,&e);effects(&e);restore(k);
  state.mode=home_label("对讲模式",8);
  if(state.mode){FN(void,0x18089c34,void *,unsigned)(state.mode,144);FN(void,0x18097548,void *,unsigned)(state.mode,0);}
  state.status=home_label("",134);
  if(state.status){FN(void,0x180885bc,void *,unsigned)(state.status,1);FN(void,0x18088758,void *,void *)(state.status,subtitle_click);}
  state.code=home_label("",68);visible(state.code,0);
  state.face=FN(void *,0x18096ef8,void *,void *)(state.root,0);
  if(state.face){FN(void,0x18096d38,void *,const void *)(state.face,&ai_happy_images[0]);FN(void,0x18088eb0,void *,int,int)(state.face,88,44);FN(void,0x180885bc,void *,unsigned)(state.face,0);}
  state.footer=home_label("",204);visible(state.footer,0);
  if(state.footer){FN(void,0x180885bc,void *,unsigned)(state.footer,1);FN(void,0x18088758,void *,void *)(state.footer,official_click);}
  state.wave=FN(void *,0x18096ef8,void *,void *)(state.root,0);
  if(state.wave){FN(void,0x18096d38,void *,const void *)(state.wave,&ai_wave_images[0]);FN(void,0x18088eb0,void *,int,int)(state.wave,56,110);FN(void,0x180885bc,void *,unsigned)(state.wave,0);visible(state.wave,0);}
  state.battery_percent=state.battery_charge=~0u;
  state.battery_pixels=xp_alloc(AI_BATTERY_PIXELS*2);
  if(state.battery_pixels){
   state.battery_image.header=4u|(AI_BATTERY_WIDTH<<10)|(AI_BATTERY_HEIGHT<<21);state.battery_image.bytes=AI_BATTERY_PIXELS*2;state.battery_image.pixels=state.battery_pixels;
   state.battery=FN(void *,0x18096ef8,void *,void *)(state.root,0);
   if(state.battery){FN(void,0x18088eb0,void *,int,int)(state.battery,156,8);FN(void,0x180885bc,void *,unsigned)(state.battery,0);}
   else {xp_free(state.battery_pixels);state.battery_pixels=0;}
  }
  state.animation=FN(void *,0x18092924,void *,unsigned,unsigned,void *)(ai_animation_tick,AI_EMOJI_PERIOD_MS,2,0);
 }else{
  visible((void *)(uintptr_t)U32(0x20011dd0),0);visible((void *)(uintptr_t)U32(0x20011dcc),0);
  if(U32(0x20011dcc))FN(void,0x180885bc,void *,unsigned)((void *)(uintptr_t)U32(0x20011dcc),0);
  home_label("设置",2);
  void *icon=FN(void *,0x18096ef8,void *,void *)(state.root,0);
  if(icon){FN(void,0x18096d38,void *,const void *)(icon,&menu_gear_image);menu_bounds(icon,12,52,40,40);FN(void,0x180885bc,void *,unsigned)(icon,1);FN(void,0x18088758,void *,void *)(icon,settings_click);}
  menu_label(state.root,"本机设置",64,55,168,42,0,settings_click);
  icon=FN(void *,0x18096ef8,void *,void *)(state.root,0);
  if(icon){FN(void,0x18096d38,void *,const void *)(icon,&menu_mic_image);menu_bounds(icon,12,118,40,40);FN(void,0x180885bc,void *,unsigned)(icon,1);FN(void,0x18088758,void *,void *)(icon,mode_click);}
  menu_label(state.root,"对话模式",64,119,168,36,0,mode_click);
  state.mode_summary=menu_label(state.root,"",64,157,168,18,1,mode_click);mode_summary();
 }
 FN(void,0x18086994,void *)(gesture);if(page==AI_HOME_PAGE)xs_start();refresh();
}
void ai_home_init(void){record_page_ready(XIAOZHI_MODE);build_page(AI_HOME_PAGE);}
void ai_menu_init(void){build_page(AI_MENU_PAGE);}
void ai_page_deinit(void){
 volume_sheet_destroy();
 state.wake_reconnect=0;state.mode_picker=state.mode_summary=state.small_style=state.menu_style=0;state.mode_picker_open=0;
 if(state.animation){FN(void,0x1809270c,void *)(state.animation);state.animation=0;}
 struct ai_effects e;unsigned k=irq();ai_leave(&state.input,&e);effects(&e);state.root=state.mode=state.status=state.code=state.footer=state.face=state.animation=0;state.feedback=state.frame=state.animation_ticks=state.caption_page=state.caption_revision=0;state.page=0;state.battery=0;restore(k);
 FN(void,0x1805b1dc,void)();state.wave=0;if(state.battery_pixels){xp_free(state.battery_pixels);state.battery_pixels=0;}
}
static void input_resume(void){
 if(state.page!=AI_HOME_PAGE||!state.root||U16(0x20011dda)!=AI_HOME_PAGE)return;
 struct xv_status voice;xs_voice_status(&voice);unsigned k=irq();
 state.input.active=1;state.input.online=voice.ready;
 state.input.down=state.input.owned=state.input.tap=0;
 state.input.generation=voice.generation;
 state.input.automatic=voice.ready&&voice.automatic;
 state.input.phase=voice.ready?(voice.phase==XV_CAPTURE?AI_LISTENING:voice.phase==XV_WAITING?AI_WAITING:voice.phase==XV_PLAYING?AI_SPEAKING:AI_IDLE):AI_IDLE;
 state.feedback=0;restore(k);
}
extern void ai_original_screen_wake(unsigned);
void ai_screen_wake(unsigned mode){
 unsigned asleep=!U8(0x20011c2d);ai_original_screen_wake(mode);
 if(asleep&&U8(0x20011c2d)){input_resume();state.wake_reconnect=1;reconnect_after_wake();changed();}
}
void ai_page_message(const uint8_t *m){volume_sheet_message(m);if(m&&(m[0]==252||m[0]==22))refresh();}
void ai_page_gesture(unsigned g){gesture(g);}
int ai_boot_hook(void *thread){init();int result=FN(int,C208_WIFI_BOOT,void *)(thread);xt_start();return result;}
void ai_initial_page(void){init();FN(int,0x1805b53c,unsigned)(FN(int,0x18041788,void)()?record_home_page():3);}
int ai_key_hook(unsigned key,unsigned type){
 init();if(key!=2)return ai_original_key(key,type);
 if(record_mode_pending()){music_keys_reset();return 1;}
 if(record_mode_active()==RECORD_MODE){music_keys_reset();return ai_original_key(key,type);}
 if(record_mode_active()==MUSIC_MODE){if(type==1)FN(void,0x18051524,unsigned)(0);music_keys_event(type);return 1;}
 music_keys_reset();
 /* Record bypasses legacy dispatch; retain its normal activity/wake update. */
 if(type==1&&state.page==AI_HOME_PAGE&&state.root&&U16(0x20011dda)==AI_HOME_PAGE)FN(void,0x18051524,unsigned)(0);
 struct ai_effects e;unsigned k=irq();struct xv_status voice;xs_voice_status(&voice);
 if(state.page==AI_HOME_PAGE&&state.root&&U16(0x20011dda)==AI_HOME_PAGE&&!state.input.active){ai_enter(&state.input,voice.ready,&e);effects(&e);}
 if(state.input.online!=voice.ready){ai_online(&state.input,voice.ready,&e);effects(&e);}
 if(state.input.active&&U16(0x20011dda)!=AI_HOME_PAGE){ai_leave(&state.input,&e);effects(&e);}
 unsigned now=FN(unsigned,0x1806161c,void)();unsigned was_down=state.input.down;
 ai_key(&state.input,key,type,now,&e);effects(&e);
 if(state.page==AI_HOME_PAGE&&state.input.active&&(type==1||((type==3||type==4)&&was_down))){state.feedback=type==1?1:2;state.feedback_until=now+1800;}
 restore(k);changed();return 1;
}
int ai_app_message(const uint8_t *message){
 if(!message||message[1]!=6)return ai_original_app_message(message);
 unsigned raw=message[4];if(raw>2)return 0;
 FN(void,0x18051524,unsigned)(0);FN(void,0x1803ed74,unsigned)(raw==1);record_mode_request(raw);return 0;
}
void *ai_state_address(void){return &state;}
extern int ai_original_page_replace(unsigned page);
extern int ai_original_page_push(unsigned page,unsigned mode,void *parameter);
static unsigned route_page(unsigned page){
 if(page==19)return 13;
 if(page==6||page==7||(page>=34&&page<=55))return record_home_page();
 if(record_mode_active()==XIAOZHI_MODE&&(page==8||page==9||page==10||page==28||page==30))return AI_HOME_PAGE;
 if(record_mode_active()==RECORD_MODE&&(page==AI_HOME_PAGE||page==AI_MENU_PAGE||page==MUSIC_PAGE))return RECORD_PAGE;
 if(page==MUSIC_PAGE&&record_mode_active()!=MUSIC_MODE){record_feature_request(MUSIC_MODE);return record_home_page();}
 if(record_mode_active()==MUSIC_MODE&&(page==8||page==9||page==10||page==28||page==30))return MUSIC_PAGE;
 return page;
}
int ai_page_replace(unsigned page){
 if(record_mode_active()==MUSIC_MODE&&(page==AI_HOME_PAGE||page==AI_MENU_PAGE))return page==AI_HOME_PAGE?music_request_ai():music_visit_settings();
 return ai_original_page_replace(route_page(page));
}
int ai_page_push(unsigned page,unsigned mode,void *parameter){
 if(record_mode_active()==MUSIC_MODE&&(page==AI_HOME_PAGE||page==AI_MENU_PAGE))return page==AI_HOME_PAGE?music_request_ai():music_visit_settings();
 unsigned target=route_page(page);
 if(target!=page)return ai_original_page_replace(target);
 return ai_original_page_push(page,mode,parameter);
}

extern void ai_original_standby(void);
void ai_standby_hook(void){
 /* Called after stock screen-off: skip only deep standby while radio is in use. */
 if(!xp_usb()&&U32(0x20020930)&&((U8(0x20020992)&6)||U8(0x2002092a))){return;}
 ai_original_standby();
}
