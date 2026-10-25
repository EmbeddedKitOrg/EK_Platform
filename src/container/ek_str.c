/**
 * @file ek_str.c
 * @brief 动态字符串管理实现
 * @author N1netyNine99
 */

#include "ek_str.h"

#if EKCFG_STR == 1

#    include "ek_io.h"
#    include "ek_heap.h"

#    define INDEX_CLAMP(idx, len)             \
        do                                    \
        {                                     \
            if ((idx) < 0) (idx) += (len);    \
            if ((idx) < 0) (idx) = 0;         \
            if ((idx) > (len)) (idx) = (len); \
        } while (0)

static int _str_ensure_cap(ek_str_t *s, uint32_t len);

ek_str_t *ek_str_create(const char *str)
{
    ek_str_t *s = ek_malloc(sizeof(ek_str_t));
    if (s == NULL) return NULL;

    s->buf = NULL;
    s->cap = 0;
    s->len = 0;

    // 传入 NULL 则创建一个空的字符串
    if (str == NULL) return s;

    if (ek_str_append(s, str) != 0)
    {
        ek_free(s);
        return NULL;
    }

    return s;
}

void ek_str_free(ek_str_t *s)
{
    if (s == NULL) return;

    s->cap = 0;
    s->len = 0;
    ek_free(s->buf);
    ek_free(s);
}

void ek_str_clear(ek_str_t *s)
{
    if (s == NULL) return;

    s->len = 0;
}

int ek_str_append_len(ek_str_t *s, const char *str, uint32_t len)
{
    if (s == NULL) return -EINVAL;

    // 如果传入追加内容为空，并且字符长度也为空
    // 什么也不做
    if (str == NULL && len == 0) return 0;
    if (str == NULL || len == 0) return -EINVAL;

    EK_ERR_RETURN(_str_ensure_cap(s, s->len + len + 1));
    memcpy(s->buf + s->len, str, len);

    s->len += len;
    s->buf[s->len] = '\0';

    return 0;
}

int ek_str_append_fmt(ek_str_t *s, const char *fmt, ...)
{
    if (s == NULL || fmt == NULL) return -EINVAL;

    // 获取格式化字符串的长度
    va_list args;
    va_start(args, fmt);
    int len = ek_vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0) return -EBADMSG;

    EK_ERR_RETURN(_str_ensure_cap(s, s->len + len));

    va_start(args, fmt);
    // +1 给 \0
    ek_vsnprintf(s->buf + s->len, len + 1, fmt, args);
    va_end(args);

    s->len += len;
    s->buf[s->len] = '\0';

    return 0;
}

int ek_str_append(ek_str_t *s, const char *str)
{
    return ek_str_append_len(s, str, strlen(str));
}

int ek_str_cat(ek_str_t *dst, ek_str_t *src)
{
    if (dst == NULL || src == NULL) return -EINVAL;
    return ek_str_append_len(dst, src->buf, src->len);
}

ek_str_t *ek_str_slice(const ek_str_t *s, int32_t start, int32_t end)
{
    if (s == NULL) return NULL;

    int32_t len = (int32_t)s->len;

    INDEX_CLAMP(start, len);
    INDEX_CLAMP(end, len);

    if (start == end) return ek_str_create("");

    uint32_t new_len = (uint32_t)(end - start);

    ek_str_t *new_s = ek_str_create(NULL);
    if (new_s == NULL) return NULL;

    new_s->buf = ek_malloc(new_len + 1);
    if (new_s->buf == NULL)
    {
        ek_free(new_s);
        return NULL;
    }

    memcpy(new_s->buf, s->buf + start, new_len);
    new_s->len = new_len;
    new_s->cap = new_len;
    new_s->buf[new_s->len] = '\0';

    return new_s;
}

int ek_str_reverse(ek_str_t *s)
{
    if (s == NULL || s->buf == NULL) return -EINVAL;

    char *left = s->buf;
    char *right = s->buf + s->len - 1;

    while (left < right)
    {
        char temp = *left;
        *left = *right;
        *right = temp;
        left++;
        right--;
    }

    return 0;
}

const char *ek_str_get_cstring(ek_str_t *s)
{
    if (s == NULL || s->buf == NULL) return "";
    return s->buf;
}

uint32_t ek_str_get_len(ek_str_t *s)
{
    if (s == NULL) return 0U;

    return s->len;
}

uint32_t ek_str_get_cap(ek_str_t *s)
{
    if (s == NULL) return 0U;

    return s->cap;
}

int ek_str_cmp(ek_str_t *s1, ek_str_t *s2)
{
    return strcmp(ek_str_get_cstring(s1), ek_str_get_cstring(s2));
}

int ek_str_ncmp(ek_str_t *s1, ek_str_t *s2, size_t n)
{
    uint32_t len1 = ek_str_get_len(s1);
    uint32_t len2 = ek_str_get_len(s2);

    uint32_t shorter_len = (len1 <= len2) ? len1 : len2;

    if (n > shorter_len) n = shorter_len;

    return strncmp(ek_str_get_cstring(s1), ek_str_get_cstring(s2), n);
}

static int _str_ensure_cap(ek_str_t *s, uint32_t len)
{
    if (s == NULL) return -EINVAL;

    if (s->cap >= len) return 0;

    uint32_t new_cap = s->cap;
    do
    {
        if (new_cap == 0) new_cap = 2;
        new_cap += new_cap / 2;
    } while (new_cap < len);

    char *buf = NULL;

    if (s->buf == NULL) buf = ek_malloc(len);
    else buf = ek_realloc(s->buf, new_cap);

    if (buf == NULL) return -ENOMEM;

    s->buf = buf;
    s->cap = new_cap;

    return 0;
}

#endif // EKCFG_STR
