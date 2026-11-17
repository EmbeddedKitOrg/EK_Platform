/**
 * @file ek_critical.h
 * @brief 临界区抽象（关中断）
 *
 * 供需要在 ISR 与主循环/线程之间共享数据的模块使用（ek_kfifo、ek_evoke 等）。
 *
 * 默认实现（弱符号，可在用户代码中提供同名强函数覆盖）：
 * - Cortex-M（GCC / armclang / armcc5）：保存 PRIMASK 后关闭全部可屏蔽中断
 * - RISC-V（GCC）：保存 mstatus.MIE 后关闭机器模式中断
 * - 其他平台（如主机测试）：空实现
 *
 * 默认实现支持嵌套：只有最外层的 ek_critical_exit() 才会恢复进入前的中断状态。
 *
 * @note 默认实现面向单核 MCU；多核或需要只屏蔽部分优先级（BASEPRI）的场景
 *       请自行实现这两个函数。
 */

#ifndef EK_CRITICAL_H
#define EK_CRITICAL_H

#include "ek/core/ek_def.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 进入临界区（可嵌套，可在 ISR 中调用）
 */
void ek_critical_enter(void);

/**
 * @brief 退出临界区，与 ek_critical_enter() 成对调用
 */
void ek_critical_exit(void);

#ifdef __cplusplus
}
#endif

#endif /* EK_CRITICAL_H */
