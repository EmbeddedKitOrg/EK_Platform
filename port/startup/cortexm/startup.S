/**
 * @file startup.S
 * @brief 通用 Cortex-M 启动文件
 * @author N1netyNine99
 *
 * 提供向量表、Reset_Handler、默认异常处理和 SystemInit 弱桩。
 * Cortex-M 目标由 ek_platform 默认编译，编译时必须能通过 include
 * 搜索路径找到 ek_conf.h；EK_CONF_PATH 仅用于配置校验。
 *
 * 外部 IRQ 的向量统一指向 ek_default_irq_handler，通过以下方式绑定：
 *   1. EK_DEFINE_ARCH_IRQ(n, handler)（需要 EKCFG_STATIC_ALLOC=1）
 *   2. 运行时调用 ek_arch_irq_install(n, handler)（需要 VTOR 已重定向）
 *
 * @note EKCFG_ARCH_IRQ_COUNT 必须能被汇编器求值，写成 (91) 这类纯整数
 *       表达式，不要使用 U 后缀。
 * @note 本文件与厂商 startup 文件互斥，不能同时参与链接。
 */

.syntax unified
.thumb

#ifndef EK_CONF_PATH
#    error "EK_CONF_PATH is not set: add ek_conf.h to the project and pass its path via CMake CONF"
#endif

#include "ek_conf.h"

#if !defined(EKCFG_ARCH_IRQ_COUNT)
#    error "EKCFG_ARCH_IRQ_COUNT must be defined in ek_conf.h"
#endif

#if (EKCFG_ARCH_IRQ_COUNT < 1)
#    error "EKCFG_ARCH_IRQ_COUNT must be >= 1"
#endif

#if (EKCFG_ARCH_IRQ_COUNT > 240)
#    error "EKCFG_ARCH_IRQ_COUNT must be <= 240"
#endif

.weak _sitcmram
.weak _stcmram
.weak _etcmram

.weak __libc_init_array

.section .ek_vectors, "a", %progbits
.global __ek_vectors
.type __ek_vectors, %object
__ek_vectors:
    .word _sp
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler

.rept EKCFG_ARCH_IRQ_COUNT
    .word ek_default_irq_handler
.endr
.size __ek_vectors, . - __ek_vectors

.section .text.ek_default_irq_handler, "ax", %progbits
.weak ek_default_irq_handler
.type ek_default_irq_handler, %function
.thumb_func
ek_default_irq_handler:
    b ek_default_irq_handler
.size ek_default_irq_handler, . - ek_default_irq_handler

.weak NMI_Handler
.thumb_set NMI_Handler, ek_default_irq_handler

.weak HardFault_Handler
.thumb_set HardFault_Handler, ek_default_irq_handler

.weak MemManage_Handler
.thumb_set MemManage_Handler, ek_default_irq_handler

.weak BusFault_Handler
.thumb_set BusFault_Handler, ek_default_irq_handler

.weak UsageFault_Handler
.thumb_set UsageFault_Handler, ek_default_irq_handler

.weak SVC_Handler
.thumb_set SVC_Handler, ek_default_irq_handler

.weak DebugMon_Handler
.thumb_set DebugMon_Handler, ek_default_irq_handler

.weak PendSV_Handler
.thumb_set PendSV_Handler, ek_default_irq_handler

.weak SysTick_Handler
.thumb_set SysTick_Handler, ek_default_irq_handler

.section .text.Reset_Handler, "ax", %progbits
.weak Reset_Handler
.type Reset_Handler, %function
.thumb_func
Reset_Handler:
    ldr r0, =_sp
    mov sp, r0

    /* .data: FLASH -> RAM */
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
.Lek_copy_data:
    cmp r0, r1
    bhs .Lek_copy_data_done
    ldr r3, [r2]
    adds r2, r2, #4
    str r3, [r0]
    adds r0, r0, #4
    b .Lek_copy_data
.Lek_copy_data_done:

    /* .bss 清零 */
    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r3, #0
.Lek_zero_bss:
    cmp r0, r1
    bhs .Lek_zero_bss_done
    str r3, [r0]
    adds r0, r0, #4
    b .Lek_zero_bss
.Lek_zero_bss_done:

    ldr r0, =_stcmram
    ldr r1, =_etcmram
    ldr r2, =_sitcmram
    cmp r0, r1
    bhs .Lek_copy_tcmram_done
    cmp r2, #0
    beq .Lek_copy_tcmram_done
.Lek_copy_tcmram:
    cmp r0, r1
    bhs .Lek_copy_tcmram_done
    ldr r3, [r2]
    adds r2, r2, #4
    str r3, [r0]
    adds r0, r0, #4
    b .Lek_copy_tcmram
.Lek_copy_tcmram_done:

#if defined(__ARM_FP) && (__ARM_FP != 0)
    ldr r0, =0xE000ED88
    ldr r1, [r0]
    ldr r2, =0x00F00000
    orr r1, r1, r2
    str r1, [r0]
    dsb
    isb
#endif

    bl SystemInit

    ldr r3, =__libc_init_array
    cmp r3, #0
    beq .Lek_no_libc_init
    blx r3
.Lek_no_libc_init:

    bl main
.Lek_main_returned:
    b .Lek_main_returned
.size Reset_Handler, . - Reset_Handler

.section .text.SystemInit, "ax", %progbits
.weak SystemInit
.type SystemInit, %function
.thumb_func
SystemInit:
    bx lr
.size SystemInit, . - SystemInit
