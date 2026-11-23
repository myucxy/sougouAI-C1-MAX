.syntax unified
.cpu cortex-m4
.thumb
.text
.global __aeabi_uldivmod
.type __aeabi_uldivmod,%function
.thumb_func
__aeabi_uldivmod:
 push {r4,lr}
 sub sp,sp,#16
 add r4,sp,#8
 str r4,[sp]
 bl __udivmoddi4
 ldrd r2,r3,[sp,#8]
 add sp,sp,#16
 pop {r4,pc}
.size __aeabi_uldivmod,.-__aeabi_uldivmod
.global __aeabi_ldivmod
.type __aeabi_ldivmod,%function
.thumb_func
__aeabi_ldivmod:
 push {r4,lr}
 sub sp,sp,#16
 add r4,sp,#8
 str r4,[sp]
 bl c208_sdivmod
 ldrd r2,r3,[sp,#8]
 add sp,sp,#16
 pop {r4,pc}
.size __aeabi_ldivmod,.-__aeabi_ldivmod
