/*
 * @file cortexm.ld.S
 * @brief 通用 Cortex-M 链接脚本模板
 *
 * 由 ek_platform/cmake/ek_linker.cmake 用 C 预处理器生成 .ld。
 * 宏来源：
 *   EKCFG_*  ek_conf.h + ek_conf_internal.h
 *   EKLD_*   ek_board.h
 *
 * 不要直接用 ld 链接本文件。
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
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_sp = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = EKLD_MIN_HEAP_SIZE;
_Min_Stack_Size = EKLD_STACK_SIZE;

/* Memories definition */
MEMORY
{
    FLASH (rx)  : ORIGIN = EKLD_FLASH_ORIGIN, LENGTH = EKLD_FLASH_LENGTH
    RAM (xrw)   : ORIGIN = EKLD_RAM_ORIGIN, LENGTH = EKLD_RAM_LENGTH
#if defined(EKLD_TCMRAM_LENGTH) && (EKLD_TCMRAM_LENGTH > 0)
    TCMRAM (rw) : ORIGIN = EKLD_TCMRAM_ORIGIN, LENGTH = EKLD_TCMRAM_LENGTH
#endif
#if defined(EKLD_SDRAM0_LENGTH) && (EKLD_SDRAM0_LENGTH > 0)
    SDRAM0 (xrw) : ORIGIN = EKLD_SDRAM0_ORIGIN, LENGTH = EKLD_SDRAM0_LENGTH
#endif
#if defined(EKLD_SDRAM1_LENGTH) && (EKLD_SDRAM1_LENGTH > 0)
    SDRAM1 (xrw) : ORIGIN = EKLD_SDRAM1_ORIGIN, LENGTH = EKLD_SDRAM1_LENGTH
#endif
}

/* Sections */
SECTIONS
{
    /* The startup code into "FLASH" Rom type memory */
    .vectors :
    {
        . = ALIGN(4);
        KEEP(*(.vectors)) /* Startup code */
        . = ALIGN(4);
    } >FLASH

    /* The program code and other data into "FLASH" Rom type memory */
    .text :
    {
        . = ALIGN(4);
        *(.text)           /* .text sections (code) */
        *(.text*)          /* .text* sections (code) */
        *(.glue_7)         /* glue arm to thumb code */
        *(.glue_7t)        /* glue thumb to arm code */
        *(.eh_frame)

        KEEP (*(.init))
        KEEP (*(.fini))

        . = ALIGN(4);
        _etext = .;        /* define a global symbols at end of code */
    } >FLASH

    /* Constant data into "FLASH" Rom type memory */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)         /* .rodata sections (constants, strings, etc.) */
        *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
        . = ALIGN(4);
    } >FLASH

    .ARM.extab (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
    {
        . = ALIGN(4);
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } >FLASH

    .ARM (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
    {
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
        . = ALIGN(4);
    } >FLASH

    .preinit_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN (__preinit_array_start = .);
        PROVIDE_HIDDEN (__bothinit_array_start = .);
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .init_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
        PROVIDE_HIDDEN (__bothinit_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .fini_array (READONLY) : /* The READONLY keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
        . = ALIGN(4);
    } >FLASH

    /* Used by the startup to initialize data */
    _sidata = LOADADDR(.data);

    /* Initialized data sections into "RAM" Ram type memory */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;        /* create a global symbol at data start */
        *(.data)           /* .data sections */
        *(.data*)          /* .data* sections */
        *(.RamFunc)        /* .RamFunc sections */
        *(.RamFunc*)       /* .RamFunc* sections */

        . = ALIGN(4);
        _edata = .;        /* define a global symbol at data end */

    } >RAM AT> FLASH

    /* Uninitialized data section into "RAM" Ram type memory */
    . = ALIGN(4);
    .bss :
    {
        /* This is used by the startup in order to initialize the .bss section */
        _sbss = .;         /* define a global symbol at bss start */
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)

        . = ALIGN(4);
        _ebss = .;         /* define a global symbol at bss end */
        __bss_end__ = _ebss;
    } >RAM

    /* User_heap_stack section, used to check that there is enough "RAM" Ram  type memory left */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        PROVIDE ( __heap_start = end );
        PROVIDE ( __heap_end = end + _Min_Heap_Size );
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

#if defined(EKLD_TCMRAM_LENGTH) && (EKLD_TCMRAM_LENGTH > 0)
    /* Used by the startup to initialize tcmram */
    _sitcmram = LOADADDR(.tcmram);

    /* Initialized tcmram sections into "TCMRAM" Ram type memory */
    .tcmram :
    {
        . = ALIGN(4);
        _stcmram = .;       /* create a global symbol at tcmram start */
        *(.tcmram)          /* .tcmram sections */
        *(.tcmram*)         /* .tcmram* sections */

        *(.ccmram)          /* .ccmram 与 ST 适配 */
        *(.ccmram*)

        . = ALIGN(4);
        _etcmram = .;       /* define a global symbol at tcmram end */
    } >TCMRAM AT> FLASH
#endif

    /* Remove information from the compiler libraries */
#if defined(EKCFG_PICOLIBC) && (EKCFG_PICOLIBC != 0)
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }
#endif

    .ARM.attributes 0 : { *(.ARM.attributes) }

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

    /* SDRAM0 Data (EXMC SDRAM device0) */
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

    /* SDRAM1 Data (EXMC SDRAM device1) */
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
