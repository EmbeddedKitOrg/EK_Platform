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

#if (EKLD_FLASH_LENGTH) <= 0
#    error "EKLD_FLASH_LENGTH must be > 0"
#endif
#if (EKLD_RAM_LENGTH) <= 0
#    error "EKLD_RAM_LENGTH must be > 0"
#endif
#if (EKLD_STACK_SIZE) <= 0
#    error "EKLD_STACK_SIZE must be > 0"
#endif
#if (EKLD_MIN_HEAP_SIZE) < 0
#    error "EKLD_MIN_HEAP_SIZE must be >= 0"
#endif
#if ((EKLD_MIN_HEAP_SIZE) + (EKLD_STACK_SIZE)) > (EKLD_RAM_LENGTH)
#    error "EKLD_MIN_HEAP_SIZE + EKLD_STACK_SIZE must not exceed EKLD_RAM_LENGTH"
#endif

#if ((EKLD_FLASH_ORIGIN) & 3) || ((EKLD_FLASH_LENGTH) & 3)
#    error "EKLD_FLASH_ORIGIN must be 4-byte aligned and EKLD_FLASH_LENGTH a multiple of 4"
#endif
#if ((EKLD_RAM_ORIGIN) & 7) || ((EKLD_RAM_LENGTH) & 7)
#    error "EKLD_RAM_ORIGIN must be 8-byte aligned and EKLD_RAM_LENGTH a multiple of 8"
#endif
#if (((EKLD_RAM_ORIGIN) + (EKLD_RAM_LENGTH)) & 15)
#    error "RAM end (_sp) must be 16-byte aligned"
#endif

#if defined(EKLD_TCMRAM_LENGTH) && ((EKLD_TCMRAM_LENGTH) < 0)
#    error "EKLD_TCMRAM_LENGTH must be >= 0"
#endif
#if defined(EKLD_TCMRAM_ORIGIN) && !defined(EKLD_TCMRAM_LENGTH)
#    error "EKLD_TCMRAM_ORIGIN requires EKLD_TCMRAM_LENGTH"
#endif
#if defined(EKLD_TCMRAM_LENGTH) && ((EKLD_TCMRAM_LENGTH) > 0) && !defined(EKLD_TCMRAM_ORIGIN)
#    error "EKLD_TCMRAM_ORIGIN must be defined when EKLD_TCMRAM_LENGTH > 0"
#endif
#if defined(EKLD_TCMRAM_ORIGIN) && ((EKLD_TCMRAM_ORIGIN) & 3)
#    error "EKLD_TCMRAM_ORIGIN must be 4-byte aligned"
#endif

#if defined(EKLD_SDRAM0_LENGTH) && ((EKLD_SDRAM0_LENGTH) < 0)
#    error "EKLD_SDRAM0_LENGTH must be >= 0"
#endif
#if defined(EKLD_SDRAM0_ORIGIN) && !defined(EKLD_SDRAM0_LENGTH)
#    error "EKLD_SDRAM0_ORIGIN requires EKLD_SDRAM0_LENGTH"
#endif
#if defined(EKLD_SDRAM0_LENGTH) && ((EKLD_SDRAM0_LENGTH) > 0) && !defined(EKLD_SDRAM0_ORIGIN)
#    error "EKLD_SDRAM0_ORIGIN must be defined when EKLD_SDRAM0_LENGTH > 0"
#endif
#if defined(EKLD_SDRAM0_ORIGIN) && ((EKLD_SDRAM0_ORIGIN) & 3)
#    error "EKLD_SDRAM0_ORIGIN must be 4-byte aligned"
#endif

#if defined(EKLD_SDRAM1_LENGTH) && ((EKLD_SDRAM1_LENGTH) < 0)
#    error "EKLD_SDRAM1_LENGTH must be >= 0"
#endif
#if defined(EKLD_SDRAM1_ORIGIN) && !defined(EKLD_SDRAM1_LENGTH)
#    error "EKLD_SDRAM1_ORIGIN requires EKLD_SDRAM1_LENGTH"
#endif
#if defined(EKLD_SDRAM1_LENGTH) && ((EKLD_SDRAM1_LENGTH) > 0) && !defined(EKLD_SDRAM1_ORIGIN)
#    error "EKLD_SDRAM1_ORIGIN must be defined when EKLD_SDRAM1_LENGTH > 0"
#endif
#if defined(EKLD_SDRAM1_ORIGIN) && ((EKLD_SDRAM1_ORIGIN) & 3)
#    error "EKLD_SDRAM1_ORIGIN must be 4-byte aligned"
#endif

ENTRY(_start)

_sp = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = EKLD_MIN_HEAP_SIZE;
_Min_Stack_Size = EKLD_STACK_SIZE;

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

SECTIONS
{

    .text.init :
    {
        . = ALIGN(4);
        _flash_vtor_start = .;
        KEEP(*(.text.init))
        KEEP(*(.init))
        . = ALIGN(4);
        _flash_vtor_end = .;
    } >FLASH

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

    _sidata = LOADADDR(.data);

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
