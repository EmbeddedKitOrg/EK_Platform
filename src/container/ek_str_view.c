/**
 * @file ek_str_view.c
 * @brief 字符串视图实现
 * @author N1netyNine99
 */

#include "ek/container/ek_str_view.h"

#if EKCFG_STR_VIEW == 1

#    include <ctype.h>

ek_str_view_t ek_str_view_create(const char *cstr)
{
    return (ek_str_view_t){
        .str = cstr,
        .str_len = strlen(cstr),
    };
}

int ek_str_view_chop_left(ek_str_view_t *sv, size_t n)
{
    if (sv == NULL) return -EINVAL;
    if (n > sv->str_len) n = sv->str_len;
    sv->str += n;
    sv->str_len -= n;

    return 0;
}

int ek_str_view_chop_right(ek_str_view_t *sv, size_t n)
{
    if (sv == NULL) return -EINVAL;
    if (n > sv->str_len) n = sv->str_len;
    sv->str_len -= n;

    return 0;
}

int ek_str_view_trim(ek_str_view_t *sv)
{
    if (sv == NULL) return -EINVAL;

    while (sv->str_len > 0 && isspace(sv->str[0]))
    {
        ek_str_view_chop_left(sv, 1);
    }
    while (sv->str_len > 0 && isspace(sv->str[sv->str_len - 1]))
    {
        ek_str_view_chop_right(sv, 1);
    }

    return 0;
}

ek_str_view_t ek_str_view_split(ek_str_view_t *sv, char c)
{
    size_t i;
    for (i = 0; i < sv->str_len; i++)
    {
        if (sv->str[i] == c) break;
    }

    // 找到了分隔符
    if (i < sv->str_len)
    {
        ek_str_view_t res = {
            .str_len = i,
            .str = sv->str,
        };
        ek_str_view_chop_left(sv, i + 1);
        return res;
    }

    // 没找到分隔符就返回原来的字符串
    // 并且把原来的字符串都给清空
    ek_str_view_t res = *sv;
    ek_str_view_chop_left(sv, i);

    return res;
}

#endif // EKCFG_STR_VIEW
