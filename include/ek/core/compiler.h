/**
 * @file compiler.h
 * @brief 编译器属性与内联相关抽象宏
 * @author N1netyNine99
 *
 * 提供跨编译器的统一宏定义，支持 GCC、ARM Compiler 6、ARM Compiler 5。
 */

#ifndef EK_COMPILER_H
#define EK_COMPILER_H

/* ========== GCC 编译器宏定义 ========== */
#if defined(__GNUC__) && !defined(__ARMCC_VERSION)

#    define __EK_WEAK          __attribute__((weak))
#    define __EK_PACKED        __attribute__((packed))
#    define __EK_PACKED_STRUCT struct __attribute__((packed))
#    define __EK_ALIGNED(x)    __attribute__((aligned(x)))
#    define __EK_USED          __attribute__((used))
#    define __EK_SECTION(x)    __attribute__((section(x)))
#    define __EK_NO_RETURN     __attribute__((noreturn))
#    define __EK_UNUSED(x)     ((void)(x))
#    define __EK_INLINE        inline
#    define __EK_STATIC_INLINE static inline
#    define __EK_ALWAYS_INLINE __attribute__((always_inline)) static inline
#    define __EK_ASM           __asm volatile
#    define __EK_CLEANUP(fn)   __attribute__((cleanup(fn)))

/* ========== ARM Compiler 6 宏定义 ========== */
#elif defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)

#    define __EK_WEAK          __attribute__((weak))
#    define __EK_PACKED        __attribute__((packed))
#    define __EK_PACKED_STRUCT struct __attribute__((packed))
#    define __EK_ALIGNED(x)    __attribute__((aligned(x)))
#    define __EK_USED          __attribute__((used))
#    define __EK_SECTION(x)    __attribute__((section(x)))
#    define __EK_NO_RETURN     __attribute__((noreturn))
#    define __EK_UNUSED(x)     ((void)(x))
#    define __EK_INLINE        inline
#    define __EK_STATIC_INLINE static inline
#    define __EK_ALWAYS_INLINE __attribute__((always_inline)) static inline
#    define __EK_ASM           __asm volatile
#    define __EK_CLEANUP(fn)   __attribute__((cleanup(fn)))

/* ========== ARM Compiler 5 宏定义 ========== */
#elif defined(__CC_ARM)

#    define __EK_WEAK          __weak
#    define __EK_PACKED        __packed
#    define __EK_PACKED_STRUCT __packed struct
#    define __EK_ALIGNED(x)    __align(x)
#    define __EK_USED          __attribute__((used))
#    define __EK_SECTION(x)    __attribute__((section(x)))
#    define __EK_NO_RETURN     __declspec(noreturn)
#    define __EK_UNUSED(x)     ((void)(x))
#    define __EK_INLINE        __inline
#    define __EK_STATIC_INLINE static __inline
#    define __EK_ALWAYS_INLINE __forceinline
#    define __EK_ASM           __asm volatile
#    define __EK_CLEANUP(fn)

/* ========== 不支持的编译器（回退默认定义） ========== */
#else
#    define __EK_WEAK
#    define __EK_PACKED
#    define __EK_PACKED_STRUCT struct
#    define __EK_ALIGNED(x)
#    define __EK_USED
#    define __EK_SECTION(x)
#    define __EK_NO_RETURN
#    define __EK_UNUSED(x)     ((void)(x))
#    define __EK_INLINE        inline
#    define __EK_STATIC_INLINE static inline
#    define __EK_ALWAYS_INLINE static inline
#    define __EK_ASM           asm volatile
#    define __EK_CLEANUP(fn)

#endif

#endif /* EK_COMPILER_H */
