/**
 * @file shell_keys.h
 * @brief Shell 终端按键编码、VT100/ANSI 转义序列与 UTF-8 判定宏定义
 */
#ifndef SHELL_KEYS_H
#define SHELL_KEYS_H

#include <stdint.h>

/* ========================================================================
 * 单字节 ASCII 控制字符 (0x00 ~ 0x1F, 0x7F)
 * ======================================================================== */

#define EK_KEY_NUL    (0x00) /**< 空字符 '\0' */
#define EK_KEY_BEL    (0x07) /**< 终端蜂鸣响铃 '\a' */
#define EK_KEY_BS     (0x08) /**< Backspace 退格键 '\b' */
#define EK_KEY_TAB    (0x09) /**< Tab 水平制表符（自动补全）'\t' */
#define EK_KEY_LF     (0x0A) /**< 换行 Line Feed '\n' */
#define EK_KEY_FF     (0x0C) /**< 换页 Form Feed '\f' (清屏) */
#define EK_KEY_CR     (0x0D) /**< 回车 Carriage Return '\r' */
#define EK_KEY_ESC    (0x1B) /**< Escape 退出键 / 转义起始符 */
#define EK_KEY_DEL    (0x7F) /**< Delete 键（部分终端发 Backspace 时发此码） */

#define EK_KEY_CTRL_A (0x01) /**< Ctrl+A: 移动光标至行首 */
#define EK_KEY_CTRL_B (0x02) /**< Ctrl+B: 光标向左移动一格 (同左箭头) */
#define EK_KEY_CTRL_C (0x03) /**< Ctrl+C: 中断当前任务 / 清空当前行 */
#define EK_KEY_CTRL_D (0x04) /**< Ctrl+D: 行空时退出终端(EOF)，行非空时向后删除 */
#define EK_KEY_CTRL_E (0x05) /**< Ctrl+E: 移动光标至行尾 */
#define EK_KEY_CTRL_F (0x06) /**< Ctrl+F: 光标向右移动一格 (同右箭头) */
#define EK_KEY_CTRL_H (0x08) /**< Ctrl+H: 等价于 Backspace 退格 */
#define EK_KEY_CTRL_K (0x0B) /**< Ctrl+K: 剪切/删除从光标到行尾的内容 */
#define EK_KEY_CTRL_L (0x0C) /**< Ctrl+L: 清屏并重新绘制当前行 */
#define EK_KEY_CTRL_N (0x0E) /**< Ctrl+N: 下一条历史记录 (同下箭头) */
#define EK_KEY_CTRL_P (0x10) /**< Ctrl+P: 上一条历史记录 (同上箭头) */
#define EK_KEY_CTRL_U (0x15) /**< Ctrl+U: 清除整行 / 清除光标前所有字符 */
#define EK_KEY_CTRL_W (0x17) /**< Ctrl+W: 删除光标前的一个单词 */

/* ========================================================================
 * 多字节 ANSI / VT100 键盘输入转义序列识别码
 * 当键盘按下功能键时，终端会依次发送 3~4 个字节：
 *   第一字节: 0x1B (ESC)
 *   第二字节: 0x5B ('['，控制序列引导符 CSI)
 *   后续字节: 下列特定键码
 * ======================================================================== */

/* ESC [ 之后的第三字节：方向键与基本导航 */
#define EK_KEY_ANSI_CODE_UP      ('A') /**< 上方向键: ESC [ A */
#define EK_KEY_ANSI_CODE_DOWN    ('B') /**< 下方向键: ESC [ B */
#define EK_KEY_ANSI_CODE_RIGHT   ('C') /**< 右方向键: ESC [ C */
#define EK_KEY_ANSI_CODE_LEFT    ('D') /**< 左方向键: ESC [ D */
#define EK_KEY_ANSI_CODE_HOME_VT ('H') /**< Home 键 (VT100 模式): ESC [ H */
#define EK_KEY_ANSI_CODE_END_VT  ('F') /**< End 键 (VT100 模式): ESC [ F */

/* ESC [ <num> ~ 系列的数字编码（多用于 PC 扩展键盘，以 '~' 结尾） */
#define EK_KEY_ANSI_NUM_HOME      ('1') /**< Home 键: ESC [ 1 ~ */
#define EK_KEY_ANSI_NUM_INSERT    ('2') /**< Insert 键: ESC [ 2 ~ */
#define EK_KEY_ANSI_NUM_DELETE    ('3') /**< Delete 键(向后删): ESC [ 3 ~ */
#define EK_KEY_ANSI_NUM_END       ('4') /**< End 键: ESC [ 4 ~ */
#define EK_KEY_ANSI_NUM_PAGE_UP   ('5') /**< PageUp 键: ESC [ 5 ~ */
#define EK_KEY_ANSI_NUM_PAGE_DOWN ('6') /**< PageDown 键: ESC [ 6 ~ */

/* ========================================================================
 * 单片机向终端输出的 VT100 终端控制字符串宏
 * 直接通过 ek_vfs_write(1, ...) 打印这些字符串，终端即可执行对应动作
 * ======================================================================== */

#define EK_KEY_VT100_ERASE_BACK     "\b \b" /**< 标准退格擦除（左移、打空格、再左移） */
#define EK_KEY_VT100_CLEAR_SCREEN   "\033[2J\033[H" /**< 清空整个屏幕并将光标移回左上角 */
#define EK_KEY_VT100_CLEAR_TO_EOL   "\033[K" /**< 清除从当前光标位置到该行行尾的所有内容 */
#define EK_KEY_VT100_CURSOR_LEFT    "\033[D" /**< 光标向左微调 1 列（不擦除字符） */
#define EK_KEY_VT100_CURSOR_RIGHT   "\033[C" /**< 光标向右微调 1 列 */
#define EK_KEY_VT100_CURSOR_SAVE    "\033[s" /**< 保存当前光标坐标 */
#define EK_KEY_VT100_CURSOR_RESTORE "\033[u" /**< 恢复上次保存的光标坐标 */

/* ========================================================================
 * UTF-8 编码特征掩码与判定宏
 * ======================================================================== */

/**
 * @brief 判断是否为标准 7-bit ASCII 字符 (0x00 ~ 0x7F)
 */
#define EK_KEY_IS_ASCII(c) (((uint8_t)(c) & 0x80) == 0x00)

/**
 * @brief 判断是否为 UTF-8 连续后续字节 (10xxxxxx, 0x80 ~ 0xBF)
 * @note 逆向回溯字符起点时使用：只要是连续字节，就不是字符头部
 */
#define EK_KEY_IS_UTF8_CONT_BYTE(c) (((uint8_t)(c) & 0xC0) == 0x80)

/**
 * @brief 判断是否为 UTF-8 双字节起始头部 (110xxxxx, 0xC0 ~ 0xDF)
 */
#define EK_KEY_IS_UTF8_2BYTE_HEAD(c) (((uint8_t)(c) & 0xE0) == 0xC0)

/**
 * @brief 判断是否为 UTF-8 三字节起始头部 (1110xxxx, 0xE0 ~ 0xEF)
 * @note 常用中文汉字（CJK）绝大部分落在此区间！
 */
#define EK_KEY_IS_UTF8_3BYTE_HEAD(c) (((uint8_t)(c) & 0xF0) == 0xE0)

/**
 * @brief 判断是否为 UTF-8 四字节起始头部 (11110xxx, 0xF0 ~ 0xF7)
 * @note Emoji 表情与生僻古文字在此区间
 */
#define EK_KEY_IS_UTF8_4BYTE_HEAD(c) (((uint8_t)(c) & 0xF8) == 0xF0)

/**
 * @brief 根据首字节快速计算该 UTF-8 字符总字节数
 * @param c 字符的起始首字节
 * @return 字节数 (1..4)
 */
#define EK_KEY_UTF8_CHAR_LEN(c)         \
    (EK_KEY_IS_ASCII(c)             ? 1 \
     : EK_KEY_IS_UTF8_2BYTE_HEAD(c) ? 2 \
     : EK_KEY_IS_UTF8_3BYTE_HEAD(c) ? 3 \
     : EK_KEY_IS_UTF8_4BYTE_HEAD(c) ? 4 \
                                    : 1)

/**
 * @brief 快速估算字符在终端屏幕上占据的列宽（半角 1 列 vs 全角 2 列）
 * @param c 字符的起始首字节
 * @return 1 表示单列宽(半角 ASCII/西文), 2 表示双列宽(中文汉字/全角标点/Emoji)
 */
#define EK_KEY_CHAR_DISPLAY_WIDTH(c)    \
    (EK_KEY_IS_ASCII(c)             ? 1 \
     : EK_KEY_IS_UTF8_2BYTE_HEAD(c) ? 1 \
     : EK_KEY_IS_UTF8_3BYTE_HEAD(c) ? 2 \
     : EK_KEY_IS_UTF8_4BYTE_HEAD(c) ? 2 \
                                    : 1)

#endif // SHELL_KEYS_H
