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

#ifndef EKLD_VECTOR_ALIGN
#    define EKLD_VECTOR_ALIGN 0x200
#endif

#if ((EKLD_VECTOR_ALIGN) <= 0) || (((EKLD_VECTOR_ALIGN) & ((EKLD_VECTOR_ALIGN) - 1)))
#   error "EKLD_VECTOR_ALIGN must be a power of 2 (e.g. 0x80, 0x100, 0x200, 0x400)!"
#endif

ENTRY(Reset_Handler)

_sp = ORIGIN(RAM) + LENGTH(RAM);

_Min_Heap_Size  = EKLD_MIN_HEAP_SIZE;
_Min_Stack_Size = EKLD_STACK_SIZE;

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

SECTIONS
{
    .vectors :
    {
        . = ALIGN(EKLD_VECTOR_ALIGN);
        KEEP(*(.vectors))
        KEEP(*(.isr_vector))
        KEEP(*(.isr_vectors))
        . = ALIGN(EKLD_VECTOR_ALIGN);
    } >FLASH

    .ram_vector (NOLOAD) :
    {
        . = ALIGN(EKLD_VECTOR_ALIGN);
        _sram_vtor_start = .;
        . = . + SIZEOF(.vectors);
        _sram_vtor_end = .;
    } >RAM

    .text :
    {
        . = ALIGN(4);
        *(.text)           
        *(.text*)          
        *(.glue_7)         
        *(.glue_7t)        
        *(.eh_frame)

        KEEP (*(.init))
        KEEP (*(.fini))

        . = ALIGN(4);
        _etext = .;        
    } >FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)         
        *(.rodata*)        
        . = ALIGN(4);
    } >FLASH

    .ARM.extab (READONLY) : 
    {
        . = ALIGN(4);
        *(.ARM.extab* .gnu.linkonce.armextab.*)
        . = ALIGN(4);
    } >FLASH

    .ARM (READONLY) : 
    {
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
        . = ALIGN(4);
    } >FLASH

    .preinit_array (READONLY) : 
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN (__preinit_array_start = .);
        PROVIDE_HIDDEN (__bothinit_array_start = .);
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .init_array (READONLY) : 
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
        PROVIDE_HIDDEN (__bothinit_array_end = .);
        . = ALIGN(4);
    } >FLASH

    .fini_array (READONLY) : 
    {
        . = ALIGN(4);
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
        . = ALIGN(4);
    } >FLASH

    
    _sidata = LOADADDR(.data);

    
    .data :
    {
        . = ALIGN(4);
        _sdata = .;        
        *(.data)           
        *(.data*)          
        *(.RamFunc)        
        *(.RamFunc*)       

        . = ALIGN(4);
        _edata = .;        

    } >RAM AT> FLASH

    
    . = ALIGN(4);
    .bss :
    {
        
        _sbss = .;         
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)

        . = ALIGN(4);
        _ebss = .;         
        __bss_end__ = _ebss;
    } >RAM

    
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

    
#if defined(EKCFG_PICOLIBC) && (EKCFG_PICOLIBC != 0)
    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }
#endif

    .ARM.attributes 0 : { *(.ARM.attributes) }

    
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
