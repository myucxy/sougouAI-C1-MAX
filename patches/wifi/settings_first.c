/* Prepend the existing Wi-Fi action to this firmware's native settings list. */
#include <stdint.h>
#include "settings_first_addresses.h"
#define FN(ret,addr,...) ((ret (*)(__VA_ARGS__))((uintptr_t)(addr)|1u))
typedef void (*event_fn)(void *,int);
void *wifi_first_create(int kind,void *root,event_fn callback){
    void *list=FN(void *,0x1805c22c,int,void *,event_fn)(kind,root,callback);
    if(!list)return list;
    void *row=FN(void *,0x1805c42c,void *,const void *,const char *,const char *)(
        list,(const void *)(uintptr_t)C208_EXISTING_WIFI_ICON,"Wi-Fi 设置",0);
    if(row){
        *(unsigned *)((uint8_t *)row+0x4c)=13;
        FN(void,0x18088758,void *,event_fn)(row,(event_fn)(uintptr_t)C208_EXISTING_WIFI_OPEN);
    }
    return list;
}
