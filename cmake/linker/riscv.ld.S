/*
 * @file riscv.ld.S
 * @brief 通用 RISC-V 裸机链接脚本模板
 *
 * 由 ek_platform/cmake/ek_linker.cmake 用 C 预处理器生成 .ld。
 * 宏来源：
 *   EKCFG_*  ek_conf.h + ek_conf_internal.h
 *   EKLD_*   ek_board.h
 *
 * 默认入口为 _start；如果启动代码使用其他符号，覆盖模板或修改 ENTRY。
 * TCMRAM 为可选段，由 EKLD_TCMRAM_LENGTH 控制；没有该区域的板子设为 0。
 * 堆的放置段应由 EKCFG_HEAP_SECTION 指向本模板中存在的段。
 */

#ifndef EKLD_FLASH_ORIGIN
#    error "EKLD_FLASH_ORIGIN is not defined; set EK_BOARD_PATH to your ek_board.h"
#endif
#ifndef EKLD_FLASH_LENGTH
#    error "EKLD_FLASH_LENGTH is not defined"
#endif
#ifndef EKLD_RAM_ORIGIN
#    error "EKLD_RAM_ORIGIN is not defined"
#endif
#ifndef EKLD_RAM_LENGTH
#    error "EKLD_RAM_LENGTH is not defined"
#endif
#ifndef EKLD_STACK_SIZE
#    error "EKLD_STACK_SIZE is not defined"
#endif
#ifndef EKLD_MIN_HEAP_SIZE
#    error "EKLD_MIN_HEAP_SIZE is not defined"
#endif

/* Entry Point */
ENTRY(_start)

/* Highest address of the user mode stack */
_sp = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = EKLD_MIN_HEAP_SIZE;
_Min_Stack_Size = EKLD_STACK_SIZE;

/* Memories definition */
MEMORY
{
    FLASH (rx)  : ORIGIN = EKLD_FLASH_ORIGIN, LENGTH = EKLD_FLASH_LENGTH
    RAM (rwx)   : ORIGIN = EKLD_RAM_ORIGIN, LENGTH = EKLD_RAM_LENGTH
#if defined(EKLD_TCMRAM_LENGTH) && (EKLD_TCMRAM_LENGTH > 0)
    TCMRAM (rw) : ORIGIN = EKLD_TCMRAM_ORIGIN, LENGTH = EKLD_TCMRAM_LENGTH
#endif
#if defined(EKLD_SDRAM0_LENGTH) && (EKLD_SDRAM0_LENGTH > 0)
    SDRAM0 (rwx) : ORIGIN = EKLD_SDRAM0_ORIGIN, LENGTH = EKLD_SDRAM0_LENGTH
#endif
#if defined(EKLD_SDRAM1_LENGTH) && (EKLD_SDRAM1_LENGTH > 0)
    SDRAM1 (rwx) : ORIGIN = EKLD_SDRAM1_ORIGIN, LENGTH = EKLD_SDRAM1_LENGTH
#endif
}

/* Sections */
SECTIONS
{
    /* Startup code and trap/IRQ entry points */
    .text.init :
    {
        . = ALIGN(4);
        KEEP(*(.text.init))
        KEEP(*(.init))
        . = ALIGN(4);
    } >FLASH

    /* Program code and read-only data */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.srodata)
        *(.srodata*)
        *(.eh_frame)

        . = ALIGN(4);
        _etext = .;
    } >FLASH

    .preinit_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN(__preinit_array_start = .);
        PROVIDE_HIDDEN(__bothinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .init_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
        PROVIDE_HIDDEN(__bothinit_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .fini_array :
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
        . = ALIGN(4);
    } >FLASH

    /* Used by the startup to initialize data */
    _sidata = LOADADDR(.data);

    /* Initialized data into RAM */
    .data :
    {
        . = ALIGN(8);
        _sdata = .;
        PROVIDE(__global_pointer$ = . + 0x800);

        *(.data)
        *(.data*)
        *(.sdata)
        *(.sdata*)

        . = ALIGN(8);
        _edata = .;
    } >RAM AT> FLASH

    /* Uninitialized data into RAM */
    .bss :
    {
        . = ALIGN(8);
        _sbss = .;
        __bss_start__ = _sbss;

        *(.bss)
        *(.bss*)
        *(.sbss)
        *(.sbss*)
        *(COMMON)

        . = ALIGN(8);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    /* Minimum heap and stack space check */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        PROVIDE(__heap_start = end);
        PROVIDE(__heap_end = end + _Min_Heap_Size);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

#if defined(EKLD_TCMRAM_LENGTH) && (EKLD_TCMRAM_LENGTH > 0)
    /* Used by the startup to initialize tcmram */
    _sitcmram = LOADADDR(.tcmram);

    .tcmram :
    {
        . = ALIGN(4);
        _stcmram = .;
        *(.tcmram)
        *(.tcmram*)
        *(.ccmram)
        *(.ccmram*)
        . = ALIGN(4);
        _etcmram = .;
    } >TCMRAM AT> FLASH
#endif

    .riscv.attributes 0 : { *(.riscv.attributes) }

    /* EK EXPORT */
#if defined(EKCFG_EXPORT) && (EKCFG_EXPORT != 0)
    .ek_export :
    {
        . = ALIGN(4);
        _ek_export_fn_start = .;
        KEEP(*(SORT(.ek_export_fn*)))
        . = ALIGN(4);
        _ek_export_fn_end = .;
    } > FLASH
#endif

    /* Static component initialization registrations */
#if defined(EKCFG_STATIC_ALLOC) && (EKCFG_STATIC_ALLOC != 0)
    .ek_static_alloc :
    {
        . = ALIGN(4);
        _ek_static_alloc_start = .;
        KEEP(*(SORT(.ek_static_alloc*)))
        . = ALIGN(4);
        _ek_static_alloc_end = .;
    } > FLASH
#endif

    /* SDRAM0 Data */
#if defined(EKLD_SDRAM0_LENGTH) && (EKLD_SDRAM0_LENGTH > 0)
    .sdram0_data (NOLOAD) :
    {
        . = ALIGN(4);
        _sdram0_data_start = .;
        *(.sdram0_data)
        *(.sdram0_data*)
        *(.sdram_data)
        *(.sdram_data*)
        . = ALIGN(4);
        _sdram0_data_end = .;
    } > SDRAM0
#endif

    /* SDRAM1 Data */
#if defined(EKLD_SDRAM1_LENGTH) && (EKLD_SDRAM1_LENGTH > 0)
    .sdram1_data (NOLOAD) :
    {
        . = ALIGN(4);
        _sdram1_data_start = .;
        *(.sdram1_data)
        *(.sdram1_data*)
        . = ALIGN(4);
        _sdram1_data_end = .;
    } > SDRAM1
#endif
}
