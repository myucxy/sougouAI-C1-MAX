.syntax unified
.thumb
.cpu cortex-m4
.section .text,"ax",%progbits
.balign 4
.global trim96_page_replace_original
.type trim96_page_replace_original,%function
.thumb_func
trim96_page_replace_original:
    push {lr}
    ldr r3, =0x20011dd8
    ldr.w ip, =0x1805b541
    bx ip
.size trim96_page_replace_original, .-trim96_page_replace_original
.balign 4
.global trim96_page_push_original
.type trim96_page_push_original,%function
.thumb_func
trim96_page_push_original:
    push {r4, lr}
    ldr r3, =0x20011dd8
    ldr.w ip, =0x1805b5bd
    bx ip
.size trim96_page_push_original, .-trim96_page_push_original
.balign 4
.global trim96_mbox_original
.type trim96_mbox_original,%function
.thumb_func
trim96_mbox_original:
    push {r4-r6, lr}
    movs r1, #0x44
    ldr.w ip, =0x1805cd55
    bx ip
.size trim96_mbox_original, .-trim96_mbox_original
.balign 4
.ltorg
