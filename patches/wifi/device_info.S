.syntax unified
.thumb
.cpu cortex-m4
.section .text,"ax",%progbits
.balign 4
.global wifi_device_info_entry
.type wifi_device_info_entry,%function
.thumb_func
wifi_device_info_entry:
    push {r4, lr}
    mov r1, r6
    mov r2, r7
    bl wifi_device_info_update
    pop {r4, pc}
.size wifi_device_info_entry, .-wifi_device_info_entry
