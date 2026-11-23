#include <stdint.h>
#include "boot_frames.h"

#define FN(ret,a,...) ((ret(*)(__VA_ARGS__))((uintptr_t)(a)|1u))

static struct {
    void *title, *subtitle, *animation, *task;
    unsigned frame,started;
    void *boot_images[QF_BOOT_PARTS+1u];
} brand;

static void cleanup(void)
{
    void *task = brand.task;
    brand.task = 0;
    brand.title = 0;
    brand.subtitle = 0;
    brand.animation = 0;
    brand.frame = brand.started = 0;
    for(unsigned i=0;i<QF_BOOT_PARTS+1u;i++)brand.boot_images[i]=0;
    if (task) FN(void,0x1809270c,void *)(task);
}

static void *label(void *root, void *style, const char *text, int y)
{
    void *object = FN(void *,0x18098300,void *,void *)(root, 0);
    if (!object) return 0;
    /* Both native roots provide white text on black and the shipped 30px font. */
    if (style) FN(void,0x1808969c,void *,void *)(object, style);
    FN(void,0x180981f0,void *,unsigned)(object, 1);
    FN(void,0x18089c34,void *,unsigned)(object, 224);
    FN(void,0x18097548,void *,unsigned)(object, 1);
    FN(void,0x180885bc,void *,unsigned)(object, 0);
    FN(void,0x18098084,void *,const char *)(object, text);
    FN(void,0x18088eb0,void *,int,int)(object, 8, y);
    return object;
}

static void boot_frame(unsigned frame)
{
    for(unsigned i=0;i<QF_BOOT_PARTS;i++){
        void *object=brand.boot_images[i+1u];const struct qf_part *part=&qf_frames[frame][i];
        if(!part->image){FN(void,0x1808970c,void *,unsigned)(object,1);continue;}
        FN(void,0x18096d38,void *,const void *)(object,part->image);
        FN(void,0x18088eb0,void *,int,int)(object,part->x,part->y);
        FN(void,0x1808970c,void *,unsigned)(object,0);
    }
    brand.frame=frame;
}

void ai_power_animation_tick(void *task)
{
    if(task&&task==brand.task&&brand.boot_images[0]){
        unsigned frame=(unsigned)(FN(unsigned,0x1806161c,void)()-brand.started)/QF_BOOT_PERIOD_MS;
        if(frame>=QF_BOOT_FRAMES)frame=QF_BOOT_FRAMES-1u;
        if(frame!=brand.frame)boot_frame(frame);
        return;
    }
    if (!task || task != brand.task || !brand.animation) return;
    brand.frame = (brand.frame + 1u) % 3u;
    FN(void,0x18098084,void *,const char *)(brand.animation,
        brand.frame == 0 ? "." : brand.frame == 1 ? ".." : "...");
}

static void create_brand(void *root, const char *subtitle)
{
    cleanup();
    if (!root) return;
    void *style = FN(void *,0x1808a218,void *)(root);
    brand.title = label(root, style, "清风惹尘丶", 84);
    brand.subtitle = label(root, style, subtitle, 126);
    brand.animation = label(root, style, ".", 166);
    if (brand.animation)
        brand.task = FN(void *,0x18092924,void (*)(void *),unsigned,unsigned,void *)(
            ai_power_animation_tick, 250, 2, 0);
}

void ai_boot_brand(void *root)
{
    cleanup();if(!root)return;
    for(unsigned i=0;i<QF_BOOT_PARTS+1u;i++){
        brand.boot_images[i]=FN(void *,0x18096ef8,void *,void *)(root,0);
        if(!brand.boot_images[i])goto fallback;
        FN(void,0x180885bc,void *,unsigned)(brand.boot_images[i],0);
    }
    FN(void,0x18096d38,void *,const void *)(brand.boot_images[0],&qf_image_0);
    FN(void,0x18088eb0,void *,int,int)(brand.boot_images[0],0,0);
    brand.started=FN(unsigned,0x1806161c,void)();boot_frame(0);
    brand.task=FN(void *,0x18092924,void (*)(void *),unsigned,unsigned,void *)(ai_power_animation_tick,QF_BOOT_PERIOD_MS,2,0);
    if(!brand.task)boot_frame(QF_BOOT_FRAMES-1u);
    return;
fallback:
    for(unsigned i=QF_BOOT_PARTS+1u;i;i--){if(brand.boot_images[i-1u])FN(void,0x1808a0d8,void *)(brand.boot_images[i-1u]);}
    create_brand(root,"正在开机");
}

void ai_shutdown_brand(void *root)
{
    create_brand(root, "正在关机");
}

void ai_boot_deinit(void)
{
    cleanup();
    FN(void,0x1805603c,void)();
}

void ai_shutdown_deinit(void)
{
    cleanup();
    FN(void,0x18057438,void)();
}
