/* The recording player owns these native images, arc styles and LVGL controls. */
#include "music.h"
#include "record_mode.h"
#include "battery.h"
#include "volume_sheet.h"
#include "play_mode_icons.h"
#include "../xiaozhi-official/platform.h"
#include <stdint.h>
#include <string.h>
#define FN(r,a,...) ((r(*)(__VA_ARGS__))((uintptr_t)(a)|1u))
#define U16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define U32(a) (*(volatile uint32_t *)(uintptr_t)(a))
static struct {
 void *root,*title,*status,*time,*play,*arc,*list_panel,*list,*rows[6],*list_status,*list_title;
 void *confirm,*confirm_title,*confirm_name,*confirm_note,*cancel,*remove,*held;
 unsigned playing,view,revision,confirm_revision,list_failed,confirm_failed;
 void *battery,*mode_icon;uint16_t *battery_pixels;
 unsigned mode_drawn;
 struct {uint32_t header,bytes;const void *pixels;} battery_image;
 unsigned battery_percent,battery_charge;
} V;
const char music_page_name[]="MUSIC";
enum {OPEN_LIST=16,PLAYER_BACK=17,DELETE_OK=18,DELETE_CANCEL=19,PLAY_MODE=20};
static void refresh(void);
static void text(void *object,const char *value);
static int show_list(void);
static int show_confirm(void);
static void hide(void *object,unsigned hidden){if(object)FN(void,0x1808970c,void *,unsigned)(object,hidden);}
static void show_player(void){V.view=0;hide(V.list_panel,1);hide(V.confirm,1);}
static void back(void){
 if(V.view==3||V.view==5)return;
 if(V.view==4){show_player();return;}
 if(V.view==2){music_delete_cancel();show_list();}
 else if(V.view==1)show_player();else music_request_ai();
}
static void click(void *object,unsigned event){
 unsigned command=U32((uintptr_t)object+0x4c);
 if(command>=32&&command<38){
  if(event==0)V.held=0;
  if(V.view!=1)return;
  if(event==4){V.held=object;if(music_delete_prepare(command-32,V.revision)){V.confirm_revision=V.revision;if(!show_confirm())music_delete_cancel();}}
  else if(event==6&&V.held!=object&&music_select(command-32,V.revision))show_player();
  return;
 }
 if(event!=6||(V.view>=2&&command!=DELETE_OK&&command!=DELETE_CANCEL))return;
 if(command==PLAY_MODE){
  if(!V.view&&U16(0x20011dda)==MUSIC_PAGE&&!volume_sheet_active()&&music_cycle_play_mode())refresh();return;
 }
 if(command==OPEN_LIST){show_list();return;}
 if(command==PLAYER_BACK){back();return;}
 if(command==DELETE_CANCEL&&V.view==4){show_player();return;}
 if(command==DELETE_OK&&V.view==4){
  if(music_pause_to(AI_HOME_PAGE)){V.view=5;text(V.confirm_title,"正在切换");hide(V.cancel,1);hide(V.remove,1);}
  return;
 }
 if(command==DELETE_CANCEL&&V.view==2){music_delete_cancel();show_list();return;}
 if(command==DELETE_OK&&V.view==2){
  if(music_delete_confirm(V.confirm_revision)){V.view=3;hide(V.cancel,1);hide(V.remove,1);refresh();}
  else {music_delete_cancel();show_list();}return;
 }
 if(V.view!=2&&V.view!=3)music_action(command);
}
static void bounds(void *object,int x,int y,unsigned width,unsigned height){
 FN(void,0x18089b70,void *,unsigned,unsigned)(object,width,height);
 FN(void,0x18088eb0,void *,int,int)(object,x,y);
}
static void bind(void *object,unsigned command){
 U32((uintptr_t)object+0x4c)=command;
 FN(void,0x180885bc,void *,unsigned)(object,1);
 FN(void,0x18088758,void *,void *)(object,click);
}
static void text(void *object,const char *value){
 if(object&&strcmp(FN(const char *,0x180975fc,void *)(object),value))FN(void,0x18098084,void *,const char *)(object,value);
}
static void *label(void *parent,const char *value,int x,int y,unsigned width,unsigned command,unsigned scroll){
 void *object=FN(void *,0x18098300,void *,void *)(parent,0);if(!object)return 0;
 void *header=(void *)(uintptr_t)U32(0x20011dd0);
 if(header)FN(void,0x1808969c,void *,void *)(object,FN(void *,0x1808a218,void *)(header));
 FN(void,0x180981f0,void *,unsigned)(object,scroll?3:2);
 FN(void,0x18097548,void *,unsigned)(object,1);
 FN(void,0x180885bc,void *,unsigned)(object,0);
 bounds(object,x,y,width,30);text(object,value);if(command)bind(object,command);return object;
}
static void *image_button(void *parent,uintptr_t released,uintptr_t pressed){
 void *object=FN(void *,0x18097188,void *,void *)(parent,0);if(!object)return 0;
 FN(void,0x1805b9d4,void *,uintptr_t,uintptr_t)(object,released,pressed);return object;
}
static int skip_button(int x,unsigned command,uintptr_t released,uintptr_t pressed){
 /* Clip the original 120px recording buttons to 64px, keeping the same pixels. */
 void *box=FN(void *,0x180897c0,void *,void *)(V.root,0);if(!box)return 0;
 FN(void,0x1808969c,void *,void *)(box,FN(void *,0x1808a218,void *)(V.root));
 bounds(box,x,158,64,64);bind(box,command);
 FN(void,0x1808856c,void *,int,int,int,int)(box,x<80?4:12,x<80?12:4,22,18);
 void *image=image_button(box,released,pressed);if(!image)return 0;
 FN(void,0x18088eb0,void *,int,int)(image,-28,0);
 FN(void,0x180885bc,void *,unsigned)(image,0);return 1;
}
static void *arc(unsigned color){
 unsigned style=FN(unsigned,0x1805b7f4,uintptr_t)(0x20025218);
 void *data=FN(void *,0x1805b83c,unsigned)(style);if(!data)return 0;
 U16((uintptr_t)data+0x34)=U16(0x180e0ed0+color);U16((uintptr_t)data+0x36)=4;
 void *object=FN(void *,0x180937c4,void *,void *)(V.root,0);
 if(object){FN(void,0x1808969c,void *,void *)(object,data);bounds(object,68,136,104,104);FN(void,0x180885bc,void *,unsigned)(object,0);}return object;
}
static char *time_text(char *out,unsigned milliseconds){
 unsigned seconds=milliseconds/1000u,minutes=seconds/60u;char reverse[10];unsigned n=0;
 do {reverse[n++]=(char)('0'+minutes%10u);minutes/=10u;}while(minutes);
 while(n)*out++=reverse[--n];*out++=':';*out++=(char)('0'+seconds%60u/10u);*out++=(char)('0'+seconds%10u);return out;
}
static void *panel(void){
 void *object=FN(void *,0x180897c0,void *,void *)(V.root,0);if(!object)return 0;
 FN(void,0x1808969c,void *,void *)(object,FN(void *,0x1808a218,void *)(V.root));
 bounds(object,0,0,240,240);FN(void,0x180885bc,void *,unsigned)(object,1);hide(object,1);return object;
}
static void refresh_list(void){
 unsigned revision=music_revision(),count=music_count();char value[256];
 if(revision!=V.revision){
  for(unsigned i=0;i<6;i++){
   hide(V.rows[i],i>=count);
   if(i<count){music_text(MUSIC_NAME,i,value,sizeof(value));text(FN(void *,0x18099538,void *)(V.rows[i]),value);}
  }
  V.revision=revision;
 }
 music_text(MUSIC_STATUS,0,value,sizeof(value));
 unsigned notice=!strncmp(value,"已删除",9)||!strncmp(value,"删除失败",12)||!strncmp(value,"停止失败",12)||!strncmp(value,"列表已变化",15)||!strncmp(value,"切换中",9);
 text(V.list_title,notice?value:"歌曲列表");
 text(V.list_status,value);hide(V.list_status,count!=0);
}
static int show_list(void){
 if(V.list_failed)return 0;
 if(!V.list_panel){
  V.list_panel=panel();if(!V.list_panel)goto failed;
  if(!label(V.list_panel,"返回",0,2,64,PLAYER_BACK,0))goto failed;
  V.list_title=label(V.list_panel,"歌曲列表",72,2,160,0,1);if(!V.list_title)goto failed;
  V.list=FN(void *,0x1805c22c,unsigned,void *,void *)(1,V.list_panel,0);if(!V.list)goto failed;
  for(unsigned i=0;i<6;i++){
   V.rows[i]=FN(void *,0x1805c42c,void *,void *,const char *,const char *)(V.list,0,"MP3",0);if(!V.rows[i])goto failed;
   bind(V.rows[i],32+i);
   void *name=FN(void *,0x18099538,void *)(V.rows[i]);if(!name)goto failed;
   FN(void,0x180981f0,void *,unsigned)(name,3);FN(void,0x18089b70,void *,unsigned,unsigned)(name,208,30);
   FN(void,0x180885bc,void *,unsigned)(name,0);
  }
  FN(void,0x1805c4dc,void *)(V.list);bounds(V.list,0,36,240,166);
  V.list_status=label(V.list_panel,"",8,96,224,0,1);
  if(!V.list_status||!label(V.list_panel,"上一页",0,206,90,MUSIC_PREVIOUS,0)||!label(V.list_panel,"刷新",90,206,60,MUSIC_REFRESH,0)||!label(V.list_panel,"下一页",150,206,90,MUSIC_NEXT,0))goto failed;
  V.revision=~0u;
 }
 V.view=1;hide(V.confirm,1);hide(V.list_panel,0);refresh_list();return 1;
failed:
 V.list_failed=1;hide(V.list_panel,1);show_player();text(V.status,"列表内存不足");return 0;
}
static int show_confirm(void){
 if(V.confirm_failed)return 0;
 if(!V.confirm){
  V.confirm=panel();if(!V.confirm)goto failed;
  V.confirm_title=label(V.confirm,"确认删除？",8,16,224,0,0);
  V.confirm_name=label(V.confirm,"",8,76,224,0,1);
  V.confirm_note=label(V.confirm,"删除后无法恢复",8,126,224,0,0);
  if(!V.confirm_title||!V.confirm_name||!V.confirm_note)goto failed;
  V.cancel=label(V.confirm,"取消",16,180,80,DELETE_CANCEL,0);
  V.remove=label(V.confirm,"删除",144,180,80,DELETE_OK,0);if(!V.cancel||!V.remove)goto failed;
  bounds(V.cancel,16,180,80,42);bounds(V.remove,144,180,80,42);
 }
 char value[256];music_text(MUSIC_DELETE_NAME,0,value,sizeof(value));text(V.confirm_name,value);
 text(V.confirm_title,"确认删除？");text(V.confirm_note,"删除后无法恢复");text(V.remove,"删除");hide(V.cancel,0);hide(V.remove,0);hide(V.list_panel,1);hide(V.confirm,0);V.view=2;return 1;
failed:
 V.confirm_failed=1;hide(V.confirm,1);show_list();return 0;
}
int music_request_ai(void){
 if(record_mode_pending()||music_navigation_pending())return 0;
 unsigned progress[3];music_progress(progress);
 if(progress[0]!=1)return music_pause_to(AI_HOME_PAGE);
 if(!V.root)return music_confirm_ai_later();
 if(V.view>=2)return 0;
 if(!show_confirm()){show_player();text(V.status,"内存不足，未切换");return 0;}
 text(V.confirm_title,"切换到小智？");text(V.confirm_name,"将暂停当前音乐");text(V.confirm_note,"是否继续切换？");text(V.remove,"切换");V.view=4;return 1;
}
static void refresh(void){
 if(!V.root)return;
 unsigned mode=music_play_mode();
 if(V.mode_icon&&mode!=V.mode_drawn){V.mode_drawn=mode;FN(void,0x18096d38,void *,const void *)(V.mode_icon,&music_mode_images[mode]);}
 if(V.battery&&V.battery_pixels){
  unsigned percent=FN(unsigned,0x18051320,void)(),charge=ai_battery_charging();
  if(percent!=V.battery_percent||charge!=V.battery_charge){
   V.battery_percent=percent;V.battery_charge=charge;
   ai_battery_draw(V.battery_pixels,percent,charge?AI_BATTERY_CHARGING:AI_BATTERY_DISCHARGING);
   FN(void,0x18096d38,void *,const void *)(V.battery,&V.battery_image);
  }
 }
 if(V.view==5&&!music_navigation_pending())show_player();
 if(V.view==1)refresh_list();
 if(V.view==2&&!music_delete_state()){show_list();}
 if(V.view==3){if(!music_delete_state())show_list();else text(V.confirm_title,"正在删除");}
 char value[256];music_text(MUSIC_TITLE,0,value,sizeof(value));text(V.title,*value?value:"MP3");
 music_text(MUSIC_STATUS,0,value,sizeof(value));text(V.status,value);
 unsigned progress[3];music_progress(progress);
 char *end=time_text(value,progress[1]);*end++=' ';*end++='/';*end++=' ';
 if(progress[2])end=time_text(end,progress[2]);else {memcpy(end,"--:--",5);end+=5;}*end=0;text(V.time,value);
 unsigned playing=progress[0]==1;
 if(V.play&&V.playing!=playing){
  FN(void,0x1805b9d4,void *,uintptr_t,uintptr_t)(V.play,playing?0x181878d0:0x1818dce8,playing?0x1818aadc:0x18190ef4);V.playing=playing;
 }
 if(V.arc){
  unsigned ratio=progress[2]?(unsigned)((uint64_t)progress[1]*360u/progress[2]):0;if(ratio>360)ratio=360;
  FN(void,0x1808970c,void *,unsigned)(V.arc,!ratio);
  FN(void,0x18093724,void *,unsigned,unsigned)(V.arc,ratio==360?0:270,ratio==360?360:(270+ratio)%360);
 }
}
void music_page_init(void){
 memset(&V,0,sizeof(V));V.playing=V.mode_drawn=~0u;
 FN(void,0x18086994,void *)(music_page_gesture);
 V.root=FN(void *,0x1805afe8,unsigned,void *)(29,0);if(!V.root){music_visible(0);return;}
 volume_sheet_bind(V.root);
 bounds(V.root,0,0,240,240);
 void *header=(void *)(uintptr_t)U32(0x20011dd0);if(header)FN(void,0x1808970c,void *,unsigned)(header,1);
 void *back_icon=(void *)(uintptr_t)U32(0x20011dcc);
 if(back_icon){hide(back_icon,1);FN(void,0x180885bc,void *,unsigned)(back_icon,0);}
 void *list_entry=label(V.root,"列表",8,2,64,OPEN_LIST,0);
 if(!list_entry)goto failed;
 FN(void,0x18097548,void *,unsigned)(list_entry,0);
 V.battery_percent=V.battery_charge=~0u;
 V.battery_pixels=xp_alloc(AI_BATTERY_PIXELS*2);
 if(V.battery_pixels){
  V.battery_image.header=4u|(AI_BATTERY_WIDTH<<10)|(AI_BATTERY_HEIGHT<<21);V.battery_image.bytes=AI_BATTERY_PIXELS*2;V.battery_image.pixels=V.battery_pixels;
  V.battery=FN(void *,0x18096ef8,void *,void *)(V.root,0);
  if(V.battery){FN(void,0x18088eb0,void *,int,int)(V.battery,156,2);FN(void,0x180885bc,void *,unsigned)(V.battery,0);}
  else {xp_free(V.battery_pixels);V.battery_pixels=0;}
 }
 V.title=label(V.root,"MP3",8,36,224,OPEN_LIST,1);V.status=label(V.root,"正在准备播放器",8,70,224,MUSIC_REFRESH,1);V.time=label(V.root,"0:00 / --:--",76,104,156,0,0);
 if(!V.title||!V.status||!V.time)goto failed;
 FN(void,0x18097548,void *,unsigned)(V.time,0);
 V.mode_icon=FN(void *,0x18096ef8,void *,void *)(V.root,0);
 if(V.mode_icon){
  FN(void,0x18088eb0,void *,int,int)(V.mode_icon,36,105);bind(V.mode_icon,PLAY_MODE);
  FN(void,0x1808856c,void *,int,int,int,int)(V.mode_icon,4,4,4,3);
 }
 V.play=image_button(V.root,0x1818dce8,0x18190ef4);if(!V.play)goto failed;
 FN(void,0x18088eb0,void *,int,int)(V.play,80,148);bind(V.play,MUSIC_TOGGLE);
 FN(void,0x1808856c,void *,int,int,int,int)(V.play,0,0,12,12);
 if(!skip_button(4,MUSIC_PREVIOUS_TRACK,0x18194acc,0x181986d8)||!skip_button(172,MUSIC_NEXT_TRACK,0x181800b8,0x18183cc4))goto failed;
 void *track=arc(8);V.arc=arc(12);if(!track||!V.arc)goto failed;
 FN(void,0x18093724,void *,unsigned,unsigned)(track,0,360);
 music_visible(1);refresh();if(music_confirmation_pending())music_request_ai();return;
failed:
 music_page_deinit();
}
void music_page_deinit(void){
 music_keys_reset();
 volume_sheet_destroy();
 music_delete_cancel();music_visible(0);FN(void,0x18086994,void *)(0);
 uint16_t *pixels=V.battery_pixels;
 FN(void,0x1805b1dc,void)();memset(&V,0,sizeof(V));if(pixels)xp_free(pixels);
}
void music_page_message(const uint8_t *message){volume_sheet_message(message);if(message&&(message[0]==251||message[0]==10||message[0]==9))refresh();}
void music_page_gesture(unsigned gesture){
 if(volume_sheet_gesture(gesture))return;
 if(gesture==6){if(!V.view&&!music_navigation_pending())volume_sheet_request(V.root);return;}
 if(gesture!=4&&gesture!=5)return;
 if(V.view){back();return;}
 if(gesture==5)music_request_ai();
 else music_visit_settings();
}

unsigned music_page_volume_allowed(void){return !V.view&&!music_navigation_pending();}

unsigned music_page_keys_allowed(void){return V.root&&!V.view&&U16(0x20011dda)==MUSIC_PAGE&&U32(0x20011dc8)==(uint32_t)(uintptr_t)V.root;}
