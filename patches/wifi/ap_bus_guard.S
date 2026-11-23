.syntax unified
.thumb
.cpu cortex-m4
.section .text,"ax",%progbits
.balign 4
.global wifi_ap_bus_guard
.type wifi_ap_bus_guard,%function
.thumb_func
wifi_ap_bus_guard:
    /* Called at the original AP initializer's bus-up BL. Preserve its frame.
     * On error, use its own reference-count cleanup and error epilogue. */
    push {r3, lr}
    bl wifi_ap_bus_probe
    pop {r3, lr}
    cmp r0, #0
    bne 1f
    bx lr
1:
    mov r10, r0
    ldr ip, =0x180d8e65
    bx ip
.balign 4
.ltorg
.size wifi_ap_bus_guard, .-wifi_ap_bus_guard
