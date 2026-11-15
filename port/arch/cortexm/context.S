#include "ek/conf_internal.h"

#if (EKCFG_ARCH == 1) && (EKCFG_RTOS == 1)

.syntax unified    
.thumb             

.extern g_cur_thrd
.extern g_next_thrd
.extern _sp

.section .text.ek_arch_start_first_thread, "ax", %progbits
.global ek_arch_start_first_thread
.type ek_arch_start_first_thread, %function
.thumb_func
ek_arch_start_first_thread:

    /* 重置 MSP 到栈顶，回收 main() 启动阶段占用的主栈 */
    ldr r0, =_sp
    msr msp, r0

    ldr r1, =g_cur_thrd /* r1 = g_cur_thrd */
    ldr r1, [r1]        /* r1 = *g_cur_thrd */
    ldr r0, [r1]        /* r0 = g_cur_thrd->sp */

    /* 从伪造栈帧弹出 sf_save */
#if defined(__ARM_FP) && (__ARM_FP != 0)
    ldr   lr, [r0], #4
    ldmia r0!, {r4-r11}
#else
    ldmia r0!, {r4-r11}
    ldr   lr, =0xFFFFFFFD
#endif

    /* hw_save 起点交给 psp */
    msr psp, r0

    /* 切回特权 Thread，清零 FPCA 浮点活动标志 */
    movs r0, #0
    msr control, r0
    isb

    cpsie i
    bx lr
    
.size ek_arch_start_first_thread, . - ek_arch_start_first_thread

.section .text.ek_arch_switch_thread, "ax", %progbits
.global ek_arch_switch_thread
.type ek_arch_switch_thread, %function
.thumb_func
ek_arch_switch_thread:

    mrs r3, primask
    cpsid i

    /* 保存当前的线程 */
    ldr r2, =g_cur_thrd       /* r2 = g_cur_thrd */
    ldr r1, [r2]              /* r1 = *g_cur_thrd */
    cbz r1, .Lswitch_thrd_ptr /* 切换 g_cur_thrd g_next_thrd */

    mrs r0, psp /* 将当前的 psp 存到 r0 */

#if defined(__ARM_FP) && (__ARM_FP != 0)
    /* 若 EXC_RETURN bit4 == 0，说明硬件压了 s0-s15+fpscr，软件需补压 s16-s31 */
    tst      lr, #0x10
    it       eq
    vstmdbeq r0!, {s16-s31}

    /* 按从高到低地址反向压入：先 r4-r11，再在最底端放 exc_return */
    stmdb    r0!, {r4-r11}
    str      lr, [r0, #-4]!
#else
    stmdb    r0!, {r4-r11}
#endif

    str r0, [r1] /* sp = g_cur_thrd->sp */

.Lswitch_thrd_ptr:
    /* 获取下一个线程 */
    ldr r1, =g_next_thrd /* r1 = g_next_thrd */
    ldr r1, [r1]         /* r1 = *g_next_thrd */
    str r1, [r2]         /* g_cur_thrd = g_next_thrd */

    /* 恢复下一个线程 */
    ldr r0, [r1]

#if defined(__ARM_FP) && (__ARM_FP != 0)
    ldr      lr, [r0], #4        /* 弹出 exc_return */
    ldmia    r0!, {r4-r11}       /* 弹出 r4-r11 */
    tst      lr, #0x10
    it       eq
    vldmiaeq r0!, {s16-s31}      /* 若上次切走时用过 FPU，恢复 s16-s31 */
#else
    ldmia    r0!, {r4-r11}
    ldr      lr, =0xFFFFFFFD
#endif

    msr   psp, r0
    msr   primask, r3
    isb
    bx    lr

.size ek_arch_switch_thread, . - ek_arch_switch_thread

#endif
