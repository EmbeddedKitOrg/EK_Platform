/**
 * @file ek_str_view.h
 * @brief 字符串视图
 * @author N1netyNine99
 *
 * 提供非拥有型的字符串视图：只记录字符串指针与长度，不复制、不分配内存。
 * - 支持左/右裁剪、去除两端空白
 * - 支持按分隔符逐段切分
 *
 * @note 视图不拥有底层内存，需保证被引用的字符串在视图使用期间始终有效
 */

#ifndef EK_STR_VIEW_H
#define EK_STR_VIEW_H

#include "ek_conf_internal.h"

#if EKCFG_STR_VIEW == 1

#    include "ek_def.h"

/**
 * @brief 字符串视图结构体
 */
typedef struct ek_str_view ek_str_view_t;

#    ifdef __cplusplus
extern "C"
{
#    endif

struct ek_str_view
{
    const char *str; /**< 字符串指针（不拥有） */
    size_t str_len; /**< 视图长度（不包含结尾的 '\0'） */
};

/**
 * @brief 从 C 字符串创建视图
 * @param cstr C 风格字符串（以 '\0' 结尾，不能为 NULL）
 * @return 字符串视图
 * @note 不复制内容，需保证 cstr 在视图使用期间有效
 */
ek_str_view_t ek_str_view_create(const char *cstr);

/**
 * @brief 从左侧切掉 n 个字符
 * @param sv 字符串视图指针
 * @param n 切掉的字符数，超过视图长度时全部切掉
 * @return 0 成功
 * @return -EINVAL sv 为 NULL
 */
int ek_str_view_chop_left(ek_str_view_t *sv, size_t n);

/**
 * @brief 从右侧切掉 n 个字符
 * @param sv 字符串视图指针
 * @param n 切掉的字符数，超过视图长度时全部切掉
 * @return 0 成功
 * @return -EINVAL sv 为 NULL
 */
int ek_str_view_chop_right(ek_str_view_t *sv, size_t n);

/**
 * @brief 去掉视图两端的空白字符
 * @param sv 字符串视图指针
 * @return 0 成功
 * @return -EINVAL sv 为 NULL
 * @note 使用 isspace() 判断空白字符
 */
int ek_str_view_trim(ek_str_view_t *sv);

/**
 * @brief 按分隔符切出一段
 * @param sv 字符串视图指针
 * @param c 分隔符
 * @return 切出的视图段
 * @note 找到分隔符时返回其之前的段，并从 sv 中移除该段与分隔符
 * @note 未找到分隔符时返回剩余全部内容，并将 sv 清空
 */
ek_str_view_t ek_str_view_split(ek_str_view_t *sv, char c);

#    ifdef __cplusplus
}
#    endif

#endif // EKCFG_STR_VIEW

#endif // EK_STR_VIEW_H
