/* C208-specific removal of retired companion UI and enforcement of zh-CN. */
#include <stdint.h>
#define FN(ret,addr,...) ((ret (*)(__VA_ARGS__))((uintptr_t)(addr)|1u))
#define U8(a) (*(volatile uint8_t *)(uintptr_t)(a))
extern int trim96_page_replace_original(unsigned page);
extern int trim96_page_push_original(unsigned page,unsigned mode,void *param);
extern int trim96_mbox_original(const uint32_t *param);
int trim96_language(unsigned requested) {
    (void)requested;
    if(U8(0x20011624)!=0) {
        U8(0x20011624)=0;
        FN(void,0x18041300,void)();
    }
    return 1;
}
int trim96_page_replace(unsigned page) {
    if(page==6||page==7)page=8;
    else if(page==19)page=13;
    return trim96_page_replace_original(page);
}
int trim96_page_push(unsigned page,unsigned mode,void *param) {
    if(page==6||page==7)return trim96_page_replace_original(8);
    if(page==19)return trim96_page_replace_original(13);
    return trim96_page_push_original(page,mode,param);
}
int trim96_mbox(const uint32_t *param) {
    if(!param)return trim96_mbox_original(param);
    if(param[4]==15||param[4]==23)return 0;
    if(param[4]==38||param[4]==39) {
        uint32_t clean[15];
        for(unsigned i=0;i<15;i++)clean[i]=param[i];
        clean[4]=param[4]==38?36:37;
        clean[5]=8;clean[6]=9;
        /* The original negative action closes/returns; both buttons use it. */
        clean[11]=clean[12];
        return trim96_mbox_original(clean);
    }
    return trim96_mbox_original(param);
}
