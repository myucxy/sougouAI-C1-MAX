.syntax unified
.cpu cortex-m4
.thumb
.text
.global ai_original_key
.type ai_original_key,%function
.thumb_func
ai_original_key:
 push.w {r4,r5,r6,r7,r8,lr}
 ldr.w pc,1f
.align 2
1: .word 0x18050cf9
.size ai_original_key,.-ai_original_key
.global ai_original_app_message
.type ai_original_app_message,%function
.thumb_func
ai_original_app_message:
 push {r4,r5,r6,lr}
 ldrb r3,[r0,#1]
 ldr.w pc,2f
.align 2
2: .word 0x180519f5
.size ai_original_app_message,.-ai_original_app_message
.global ai_original_page_replace
.type ai_original_page_replace,%function
.thumb_func
ai_original_page_replace:
 push {lr}
 ldr r3,=0x20011dd8
 ldr.w ip,=0x1805b541
 bx ip
.size ai_original_page_replace,.-ai_original_page_replace
.global ai_original_page_push
.type ai_original_page_push,%function
.thumb_func
ai_original_page_push:
 push {r4,lr}
 ldr r3,=0x20011dd8
 ldr.w ip,=0x1805b5bd
 bx ip
.size ai_original_page_push,.-ai_original_page_push
.ltorg

.global ai_original_standby
.type ai_original_standby,%function
.thumb_func
ai_original_standby:
 push {r4,r5,r6,lr}
 ldr r4,=0x20011c1c
 ldr.w pc,3f
.align 2
3: .word 0x18051805
.size ai_original_standby,.-ai_original_standby
.ltorg

.global ai_original_screen_wake
.type ai_original_screen_wake,%function
.thumb_func
ai_original_screen_wake:
 push {r3,r4,r5,lr}
 ldr r4,=0x20011c1c
 ldr.w pc,4f
.align 2
4: .word 0x18051459
.size ai_original_screen_wake,.-ai_original_screen_wake
.ltorg
