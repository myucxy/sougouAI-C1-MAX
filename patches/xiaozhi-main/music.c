/* Borrow the existing recorder player; no new decoder, DMA or player threads. */
#include "music.h"
#include "record_mode.h"
#include "../xiaozhi-official/platform.h"
#include "../xiaozhi-official/session.h"
#include "../xiaozhi-official/audio.h"
#include <string.h>
#define FN(r,a,...) ((r(*)(__VA_ARGS__))((uintptr_t)(a)|1u))
#define U8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define U32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define PAGE 6u
struct music {
 void *player;uintptr_t callback,userdata;
 unsigned count,page,revision,command,index,busy,closing,played,playback,elapsed,total,remove_phase,remove_revision,remove_track;
 char names[PAGE][256],title[256],status[96],remove_name[256];
};
static struct music *P;
static struct {char path[264];unsigned seconds,playing,track,previous;} resume;
static struct {volatile unsigned visible,tag,accept,event,error,retry_at,retry_set,revision,navigation,settings,confirm_ai,mode,random;} U;
const unsigned music_working_bytes=sizeof(struct music);
static void copy(char *d,const char *s,unsigned capacity){
 if(!capacity)return;unsigned n=0;while(n+1<capacity&&s[n]){d[n]=s[n];n++;}d[n]=0;
}
static void notify(void){FN(int,0x1805b5e8,unsigned,unsigned,unsigned)(251,0,0);}
static void status(struct music *m,const char *text){unsigned k=xp_lock();copy(m->status,text,sizeof(m->status));xp_unlock(k);}
static unsigned state(struct music *m){return FN(unsigned,0x180a4464,void *)(m->player);}
static void lock_player(struct music *m){FN(void,0x180a4c78,void *)((void *)(uintptr_t)U32((uintptr_t)m->player+0x18));}
static void unlock_player(struct music *m){FN(void,0x180a4c80,void *)((void *)(uintptr_t)U32((uintptr_t)m->player+0x18));}
static int decoder_ready(void){
 for(uintptr_t node=U32(0x20021480),i=0;node&&i<32;node=U32(node),i++){
  const char *name=(const char *)(uintptr_t)U32(node+8);if(name&&!strcmp(name,"mp3"))return 1;
 }
 return 0;
}
static int filename(const char *name,unsigned length){
 if(length<5||length>255||name[0]=='.'||name[length])return 0;
 for(unsigned i=0;i<length;i++)if(!name[i]||name[i]=='/'||name[i]=='\\')return 0;
 return name[length-4]=='.'&&(name[length-3]=='m'||name[length-3]=='M')&&(name[length-2]=='p'||name[length-2]=='P')&&name[length-1]=='3';
}
static void scan(struct music *m){
 /* Scanning invalidates any outstanding long-press selection. */
 unsigned k=xp_lock();m->count=0;m->revision=++U.revision;if(m->remove_phase==1)m->remove_phase=0;xp_unlock(k);
 void *dir=FN(void *,0x1806a3d8,const char *)("/Music");
 if(!dir){status(m,"请将MP3放入Music目录");return;}
 unsigned found=0;
 for(unsigned i=0;i<4096;i++){
  const uint8_t *entry=FN(const uint8_t *,0x1806a44c,void *)(dir);if(!entry)break;
  const char *name=(const char *)entry+4;unsigned length=entry[1];
  if(entry[0]==2||!filename(name,length))continue;
  if(found++<m->page*PAGE)continue;if(m->count==PAGE)break;
  k=xp_lock();copy(m->names[m->count++],name,256);xp_unlock(k);
 }
 FN(int,0x1806a4f4,void *)(dir);k=xp_lock();m->revision=++U.revision;xp_unlock(k);status(m,m->count?"按播放开始 / 左右键切歌":"没有更多MP3");
}
void music_event(void *player,unsigned info,void *token){
 (void)player;
 /* Delayed callbacks carry a generation, never a pointer to the unloadable list. */
 unsigned k=xp_lock();if(U.accept&&(unsigned)(uintptr_t)token==U.tag&&(info==5||info==6))U.event=info;xp_unlock(k);
}
unsigned music_play_mode(void){return U.mode?U.mode-1u:MUSIC_ORDERED;}
const char *music_play_mode_name(void){
 static const char *const names[]={"单曲循环","全部循环","顺序播放","随机播放"};return names[music_play_mode()];
}
static unsigned track_count(void){
 void *directory=FN(void *,0x1806a3d8,const char *)("/Music");if(!directory)return 0;
 unsigned count=0;
 for(unsigned i=0;i<4096;i++){
  const uint8_t *entry=FN(const uint8_t *,0x1806a44c,void *)(directory);if(!entry)break;
  if(entry[0]!=2&&filename((const char *)entry+4,entry[1]))count++;
 }
 FN(int,0x1806a4f4,void *)(directory);return count;
}
static unsigned shuffled_track(unsigned total){
 if(total<=1)return 0;
 unsigned n=U.random?U.random:xp_ticks()|0x9e3779b9u;n^=n<<13;n^=n>>17;n^=n<<5;U.random=n;
 unsigned current=resume.track%total,index=n%(total-1u);return index>=current?index+1u:index;
}
static int start_path(struct music *m){
 if(!resume.path[0]){status(m,"请将MP3放入Music目录");return 0;}
 uint32_t cfg[16]={0};cfg[0]=(uint32_t)(uintptr_t)resume.path;cfg[1]=1;
 cfg[2]=0x18127368;cfg[3]=0x180a3509;cfg[4]=0x180a35a1;cfg[5]=0x180a35b5;cfg[6]=0x180a35c1;cfg[11]=resume.seconds;cfg[12]=1;
 lock_player(m);unsigned k=xp_lock();if(!++U.tag)++U.tag;U.event=0;U.accept=1;
 U32((uintptr_t)m->player+0x3c)=U.tag;xp_unlock(k);unlock_player(m);
 if(FN(int,0x180a453c,void *,void *)(m->player,cfg)){
  U.accept=0;resume.playing=0;status(m,"播放失败 / 请切换歌曲");return 0;
 }
 m->played=1;resume.playing=1;status(m,"正在播放");return 1;
}
static void select_path(struct music *m,unsigned index){
 if(resume.path[0]&&resume.track!=m->page*PAGE+index)resume.previous=resume.track+1u;
 copy(resume.path,"/Music/",sizeof(resume.path));copy(resume.path+7,m->names[index],sizeof(resume.path)-7);
 resume.seconds=0;resume.track=m->page*PAGE+index;
 unsigned k=xp_lock();copy(m->title,m->names[index],sizeof(m->title));xp_unlock(k);
}
static int progress(struct music *m){
 unsigned playing=state(m),elapsed=m->played?FN(unsigned,0x180a446c,void *)(m->player):resume.seconds*1000u;
 unsigned total=m->played?FN(unsigned,0x180a448c,void *)(m->player):0;
 if(total&&elapsed>total)elapsed=total;
 unsigned k=xp_lock();int changed=playing!=m->playback||elapsed/1000u!=m->elapsed/1000u||total!=m->total;
 m->playback=playing;m->elapsed=elapsed;m->total=total;xp_unlock(k);return changed;
}
static int open_music(void){
 if(P)return 1;
 uintptr_t player=U32(0x200118c8);
 if(!player||U8(0x2001185c)||!xs_quiescent()||!xa_quiescent()){U.error=2;return 0;}
 if(!decoder_ready()){U.error=3;return 0;}
 struct music *m=xp_alloc(sizeof(*m));if(!m){U.error=1;return 0;}m->player=(void *)player;m->busy=1;
 lock_player(m);
 if(state(m)||U32(player+0x38)!=0x1804670du){unlock_player(m);xp_free(m);U.error=2;return 0;}
 m->callback=U32(player+0x38);m->userdata=U32(player+0x3c);
 unsigned k=xp_lock();if(!++U.tag)++U.tag;U.event=U.accept=U.error=0;P=m;
 U32(player+0x38)=(uint32_t)(uintptr_t)music_event;U32(player+0x3c)=U.tag;xp_unlock(k);unlock_player(m);
 m->page=resume.track/PAGE;scan(m);
 if(resume.path[0]){
  k=xp_lock();copy(m->title,resume.path+7,sizeof(m->title));xp_unlock(k);
  if(resume.playing)start_path(m);else status(m,"已暂停 / 按播放继续");
 }
 else if(m->count)select_path(m,0);
 progress(m);k=xp_lock();m->busy=0;xp_unlock(k);notify();return 1;
}
int music_close(void){
 music_keys_reset();
 struct music *m=P;if(!m){U.settings=U.confirm_ai=0;return 1;}
 if(!m->closing){
  unsigned k=xp_lock();m->closing=1;m->command=0;U.accept=0;xp_unlock(k);
  unsigned current=state(m);
  if(m->played){if(current==1||current==2)resume.seconds=FN(unsigned,0x180a446c,void *)(m->player)/1000;resume.playing=current==1;}
  FN(int,0x180a4494,void *)(m->player);
 }
 lock_player(m);if(state(m)){unlock_player(m);return 0;}
 uintptr_t player=(uintptr_t)m->player;
 unsigned k=xp_lock();U32(player+0x38)=m->callback;U32(player+0x3c)=m->userdata;P=0;U.accept=U.event=U.retry_set=U.navigation=U.settings=U.confirm_ai=0;xp_unlock(k);
 unlock_player(m);xp_free(m);return 1;
}
static int available(void){return record_mode_active()==MUSIC_MODE&&!record_mode_pending()&&!xp_usb();}
int music_cycle_play_mode(void){
 if(!available())return 0;
 unsigned key=xp_lock();struct music *m=P;
 if(!m||m->closing||U.navigation){xp_unlock(key);return 0;}
 U.mode=(music_play_mode()+1u)%4u+1u;copy(m->status,music_play_mode_name(),sizeof(m->status));xp_unlock(key);
 notify();return 1;
}
extern int ai_original_page_replace(unsigned page);
unsigned music_settings_active(void){return U.settings&&record_mode_active()==MUSIC_MODE;}
int music_visit_settings(void){
 if(!available())return 0;
 unsigned key=xp_lock();struct music *m=P;
 if(m&&(m->closing||m->busy||m->command||m->remove_phase==2)){xp_unlock(key);return 0;}
 if(m)m->remove_phase=0;
 U.settings=1;xp_unlock(key);
 if(ai_original_page_replace(AI_MENU_PAGE)==1)return 1;
 key=xp_lock();U.settings=0;xp_unlock(key);return 0;
}
int music_confirm_ai_later(void){
 if(!available())return 0;
 unsigned key=xp_lock();U.confirm_ai=1;xp_unlock(key);
 if(ai_original_page_replace(MUSIC_PAGE)==1)return 1;
 key=xp_lock();U.confirm_ai=0;xp_unlock(key);return 0;
}
unsigned music_confirmation_pending(void){unsigned key=xp_lock(),pending=U.confirm_ai;U.confirm_ai=0;xp_unlock(key);return pending;}
int music_pause_to(unsigned page){
 if(page==AI_MENU_PAGE)return music_visit_settings();
 if((page!=AI_HOME_PAGE&&page!=AI_MENU_PAGE)||!available())return 0;
 unsigned k=xp_lock();struct music *m=P;
 if(!m){xp_unlock(k);return record_navigate(page);}
 int accepted=!m->closing&&!m->busy&&!m->command&&m->remove_phase!=2;
 if(accepted){m->remove_phase=0;m->command=10;m->index=page;U.navigation=page;}xp_unlock(k);return accepted;
}
unsigned music_navigation_pending(void){return U.navigation||record_mode_pending();}
int music_delete_prepare(unsigned index,unsigned revision){
 if(!available())return 0;
 unsigned k=xp_lock();struct music *m=P;
 int accepted=m&&!m->closing&&!m->busy&&!m->command&&revision==m->revision&&index<m->count;
 if(accepted){copy(m->remove_name,m->names[index],sizeof(m->remove_name));m->remove_revision=revision;m->remove_track=m->page*PAGE+index;m->remove_phase=1;}
 xp_unlock(k);return accepted;
}
int music_delete_confirm(unsigned revision){
 if(!available())return 0;
 unsigned k=xp_lock();struct music *m=P;
 int accepted=m&&!m->closing&&!m->busy&&!m->command&&m->remove_phase==1&&revision==m->remove_revision&&revision==m->revision;
 if(accepted){m->remove_phase=2;m->command=9;}xp_unlock(k);return accepted;
}
void music_delete_cancel(void){unsigned k=xp_lock();if(P&&P->remove_phase==1)P->remove_phase=0;xp_unlock(k);}
unsigned music_delete_state(void){unsigned k=xp_lock(),phase=P?P->remove_phase:0;xp_unlock(k);return phase;}
static void remove_song(struct music *m){
 char path[264];copy(path,"/Music/",sizeof(path));copy(path+7,m->remove_name,sizeof(path)-7);
 if(m->remove_phase!=2||m->remove_revision!=m->revision||!available()||!filename(path+7,(unsigned)strlen(path+7))){status(m,"列表已变化，请重试");goto done;}
 unsigned current=!strcmp(path,resume.path);
 if(current){
  unsigned playing=state(m);if(playing==1||playing==2)resume.seconds=FN(unsigned,0x180a446c,void *)(m->player)/1000u;
  U.accept=0;FN(int,0x180a4494,void *)(m->player);
  if(state(m)){status(m,"停止失败，未删除");goto done;}
  resume.playing=m->played=0;
 }
 /* Recheck handoff/USB immediately before entering the native filesystem. */
 if(!available()){status(m,"切换中，未删除");goto done;}
 if(FN(int,0x1806a278,const char *)(path)){status(m,"删除失败，请重试");goto done;}
 if(resume.previous==m->remove_track+1u)resume.previous=0;else if(resume.previous>m->remove_track+1u)resume.previous--;
 if(current){resume.path[0]=0;resume.seconds=resume.playing=resume.track=resume.previous=0;unsigned k=xp_lock();m->title[0]=0;xp_unlock(k);}
 else if(m->remove_track<resume.track)resume.track--;
 scan(m);if(!m->count&&m->page){m->page--;scan(m);}
 if(current&&m->count){unsigned index=m->remove_track-m->page*PAGE;if(index>=m->count)index=m->count-1;select_path(m,index);}
 status(m,m->count?"已删除":"已删除，目录为空");
done:
 {unsigned k=xp_lock();m->remove_phase=0;xp_unlock(k);}
}
static int queue(unsigned command,unsigned index,unsigned revision){
 if(record_mode_active()!=MUSIC_MODE||record_mode_pending()||xp_usb())return 0;
 unsigned k=xp_lock();struct music *m=P;
 if(!m){if(command==MUSIC_REFRESH){U.retry_set=0;xp_unlock(k);return 1;}xp_unlock(k);return 0;}
 int accepted=!m->closing&&!m->busy&&!m->command&&(command!=3||(revision==m->revision&&index<m->count));
 if(accepted){if(m->remove_phase==1)m->remove_phase=0;m->command=command;m->index=index;}xp_unlock(k);return accepted;
}
int music_action(unsigned command){return (command==1||command==2||command==4||command==5||command==6||command==7||command==8)&&queue(command,0,0);}
int music_select(unsigned index,unsigned revision){return queue(3,index,revision);}
void music_poll(void){
 if(xp_usb()||(!U.visible&&!U.settings)){music_close();return;}
 if(!U.visible){
  unsigned page=FN(unsigned,0x1805b5a0,void)();
  unsigned settings_page=page==MUSIC_PAGE||page==AI_MENU_PAGE||page==11||page==12||(page>=13&&page<=27)||page==29||page==58;
  if(!settings_page){music_close();return;}
 }
 if(!P){
  if(FN(unsigned,0x1805b5a0,void)()!=MUSIC_PAGE)return;
  if(U.retry_set&&(int32_t)(xp_ticks()-U.retry_at)<0)return;
  U.retry_set=1;U.retry_at=xp_ticks()+2000u;if(!open_music())notify();return;
 }
 music_keys_poll();
 struct music *m=P;if(m->closing){music_close();return;}
 unsigned k=xp_lock(),cmd=m->command,index=m->index,event=U.event;m->command=U.event=0;m->busy=1;xp_unlock(k);
 unsigned ended=event==5;
 if(event){
  U.accept=0;FN(int,0x180a4494,void *)(m->player);resume.seconds=resume.playing=0;
  status(m,state(m)?"播放器尚未停止，请重试":event==6?"播放出错 / 请切换歌曲":"播放结束");
 }
 if(ended&&!cmd&&!state(m)&&available()){
  if(music_play_mode()==MUSIC_REPEAT_ONE)start_path(m);
  else cmd=MUSIC_NEXT_TRACK;
 }
 if(cmd==1){
  unsigned current=state(m);
  if(current==1){FN(int,0x180a46c4,void *)(m->player);status(m,"已暂停");}
  else if(current==2){FN(int,0x180a46e8,void *)(m->player);status(m,"正在播放");}else start_path(m);
 }
 if(cmd==MUSIC_PREVIOUS_TRACK||cmd==MUSIC_NEXT_TRACK){
  unsigned track=resume.track,page=m->page,mode=music_play_mode();
  if(mode==MUSIC_SHUFFLE){
   unsigned total=track_count();
   track=cmd==MUSIC_PREVIOUS_TRACK&&resume.previous&&resume.previous<=total?resume.previous-1u:shuffled_track(total);
  }else if(cmd==MUSIC_NEXT_TRACK)track++;
  else if(track)track--;
  else if(mode==MUSIC_REPEAT_ONE||mode==MUSIC_REPEAT_ALL){unsigned total=track_count();track=total?total-1u:0;}
  if(track/PAGE!=m->page||mode==MUSIC_SHUFFLE){m->page=track/PAGE;scan(m);}
  index=track%PAGE;
  if(index>=m->count&&cmd==MUSIC_NEXT_TRACK&&(mode==MUSIC_REPEAT_ONE||mode==MUSIC_REPEAT_ALL)){
   m->page=0;scan(m);index=0;
  }
  if(index<m->count)cmd=3;
  else{m->page=page;scan(m);status(m,ended?"播放结束":"已是最后一首");cmd=0;notify();}
 }
 if(cmd==2||cmd==3){
  U.accept=0;FN(int,0x180a4494,void *)(m->player);
  if(state(m))status(m,"播放器尚未停止，请重试");
  else if(cmd==2){resume.seconds=resume.playing=m->played=0;status(m,"已停止");}
  else{
   select_path(m,index);start_path(m);
  }
 }
 if(cmd==9)remove_song(m);
 if(cmd==10){
  if(state(m)==1)FN(int,0x180a46c4,void *)(m->player);
  if(state(m)==1)status(m,"暂停失败，未切换");
  else {
   if(m->played&&state(m)==2)resume.seconds=FN(unsigned,0x180a446c,void *)(m->player)/1000u;
   resume.playing=0;
   status(m,record_navigate(index)?"已暂停，正在切换":"切换失败，请重试");
  }
  U.navigation=0;
 }
 if(cmd==4&&m->page)m->page--;
 if(cmd==5&&m->count==PAGE&&m->page<682u)m->page++;
 if(cmd==6)m->page=0;
 if(cmd>=4&&cmd<=6){scan(m);if(cmd==6&&!resume.path[0]&&m->count)select_path(m,0);}
 int changed=progress(m);k=xp_lock();m->busy=0;xp_unlock(k);if(cmd||event||changed)notify();
}
void music_progress(unsigned out[3]){
 unsigned k=xp_lock();struct music *m=P;
 out[0]=m?m->playback:0;out[1]=m?m->elapsed:0;out[2]=m?m->total:0;xp_unlock(k);
}
void music_text(unsigned field,unsigned index,char *out,unsigned capacity){
 unsigned k=xp_lock();struct music *m=P;const char *text="";
 if(field==MUSIC_TITLE)text=m?m->title:"MP3";
 else if(field==MUSIC_STATUS)text=m?m->status:U.error==1?"内存不足，请稍后重试":U.error==2?"播放器未就绪":U.error==3?"MP3解码器未就绪":"正在准备播放器";
 else if(field==MUSIC_NAME&&m&&index<m->count)text=m->names[index];
 else if(field==MUSIC_DELETE_NAME&&m&&m->remove_phase)text=m->remove_name;
 copy(out,text,capacity);xp_unlock(k);
}
unsigned music_count(void){unsigned k=xp_lock(),n=P?P->count:0;xp_unlock(k);return n;}
unsigned music_revision(void){unsigned k=xp_lock(),n=P?P->revision:U.revision;xp_unlock(k);return n;}
void music_visible(unsigned visible){unsigned k=xp_lock();if(visible&&!U.visible)U.retry_set=0;if(visible)U.settings=0;U.visible=visible;xp_unlock(k);}
int music_loaded(void){return P!=0;}
