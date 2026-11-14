#include "ek/shell/shell.h"

#if EKCFG_SHELL == 1

#    include "ek/mem/heap.h"
#    include "ek/container/list.h"
#    include "ek/shell/shell_keys.h"
#    include "ek/vfs/vfs_core.h"
#    include "ek/core/io.h"

enum
{

    ANSI_STATE_NORMAL, // 正常接收普通字符
    ANSI_STATE_ESC, // 收到 0x1B 转义前缀 (ESC)
    ANSI_STATE_BRACKET, // 收到 '[' 控制序列引导符
    ANSI_STATE_ARG, // 收到数字参数（如 '3'），等待 '~'
};

// Shell 实例状态与行编辑上下文
typedef struct
{
    char buf[EKCFG_SHELL_BUF_SIZE];
    size_t len;
    size_t cursor;

    int in_fd;
    int out_fd;
    const char *prompt;

    uint8_t ansi_state;
    char ansi_arg;
} shell_t;

// 动态注册命令链表节点
typedef struct
{
    ek_shell_cmd_t cmd;
    ek_list_node_t node;
} dynamic_cmd_t;

// 链接脚本中标记的静态命令段起止符号
extern const ek_shell_cmd_t _ek_shell_cmd_start;
extern const ek_shell_cmd_t _ek_shell_cmd_end;

static shell_t s_default_shell;
static ek_list_node_t s_dynamic_cmd_head = { .next = &s_dynamic_cmd_head, .prev = &s_dynamic_cmd_head };

// 发送原始数据块到输出文件描述符
static inline ssize_t _shell_raw_write(shell_t *sh, const void *data, size_t len)
{
    return ek_vfs_write(sh->out_fd, data, len);
}

// 发送单个字符
static inline void _shell_putc(shell_t *sh, char ch)
{
    ek_vfs_write(sh->out_fd, &ch, 1);
}

// 发送以空字符结尾的字符串
static inline void _shell_puts(shell_t *sh, const char *str)
{
    ek_vfs_write(sh->out_fd, str, strlen(str));
}

// 在静态链接器段与动态链表中查找匹配的命令
static const ek_shell_cmd_t *_shell_cmd_find(const char *name)
{
    if (!name) return NULL;

    const ek_shell_cmd_t *start = &_ek_shell_cmd_start;
    const ek_shell_cmd_t *end = &_ek_shell_cmd_end;

    // 优先遍历静态导出的命令段
    for (const ek_shell_cmd_t *cmd = start; cmd < end; cmd++)
    {
        if (!strncmp(name, cmd->name, strlen(name))) return cmd;
    }

    // 随后遍历运行时动态注册的命令链表
    ek_list_node_t *pnode;
    ek_list_foreach(pnode, &s_dynamic_cmd_head)
    {
        dynamic_cmd_t *dy_cmd = EK_CONTAINER_OF(pnode, dynamic_cmd_t, node);
        if (!strncmp(name, dy_cmd->cmd.name, strlen(name))) return &dy_cmd->cmd;
    }

    return NULL;
}

int ek_shell_cmd_register(const char *name, ek_shell_cmd_fn_t fn)
{
    if (!name || !fn) return -EINVAL;

    dynamic_cmd_t *dy_cmd = ek_malloc(sizeof(*dy_cmd));
    if (!dy_cmd) return -ENOMEM;

    ek_list_init(&dy_cmd->node);
    dy_cmd->cmd = (ek_shell_cmd_t){ .name = name, .fn = fn };
    ek_list_insert_tail(&s_dynamic_cmd_head, &dy_cmd->node);

    return 0;
}

void ek_shell_cmd_unregister(const char *name)
{
    ek_list_node_t *pnode = NULL;
    dynamic_cmd_t *dy_cmd = NULL;
    ek_list_foreach(pnode, &s_dynamic_cmd_head)
    {
        dy_cmd = EK_CONTAINER_OF(pnode, dynamic_cmd_t, node);
        if (!strncmp(name, dy_cmd->cmd.name, strlen(name))) break;
    }

    if (dy_cmd)
    {
        ek_list_remove(&dy_cmd->node);
        ek_free(dy_cmd);
    }
}

int ek_shell_init(ek_shell_cfg_t *cfg)
{
    if (!cfg) return -EINVAL;

    s_default_shell.in_fd = cfg->in_fd;
    s_default_shell.out_fd = cfg->out_fd;
    s_default_shell.len = 0;
    s_default_shell.cursor = 0;
    s_default_shell.ansi_state = ANSI_STATE_NORMAL;
    s_default_shell.buf[0] = EK_KEY_NUL;
    if (!s_default_shell.prompt)
    {
        s_default_shell.prompt = EKCFG_SHELL_PROMPT;
    }

    // 初始化完成即进入交互，先输出一次提示符
    ek_vfs_write(s_default_shell.out_fd, s_default_shell.prompt, strlen(s_default_shell.prompt));

    return 0;
}

#    if EKCFG_SHELL_UTF8 == 1

// 光标向前跳过一个完整 UTF-8 字符，返回该字符的显示列宽
static inline size_t _utf8_prev_char(shell_t *sh)
{
    if (!sh->cursor) return 0;

    size_t i = sh->cursor - 1;
    while (i > 0 && EK_KEY_IS_UTF8_CONT_BYTE(sh->buf[i])) i--;

    size_t cols = EK_KEY_CHAR_DISPLAY_WIDTH(sh->buf[i]);
    sh->cursor = i;

    return cols;
}

// 光标向后跳过一个完整 UTF-8 字符，返回该字符的显示列宽
static inline size_t _utf8_next_char(shell_t *sh)
{
    if (sh->cursor >= sh->len) return 0;

    size_t cols = EK_KEY_CHAR_DISPLAY_WIDTH(sh->buf[sh->cursor]);
    size_t step = EK_KEY_UTF8_CHAR_LEN(sh->buf[sh->cursor]);

    sh->cursor += step;
    if (sh->cursor > sh->len) sh->cursor = sh->len;

    return cols;
}

// 统计 [from, to) 字节区间对应的显示列宽
static inline size_t _utf8_cols(const shell_t *sh, size_t from, size_t to)
{
    size_t cols = 0;

    for (size_t i = from; i < to;)
    {
        cols += EK_KEY_CHAR_DISPLAY_WIDTH(sh->buf[i]);
        i += EK_KEY_UTF8_CHAR_LEN(sh->buf[i]);
    }

    return cols;
}

#    endif // EKCFG_SHELL_UTF8 == 1

// 终端光标左移 cols 列
static inline void _move_left(shell_t *sh, size_t cols)
{
    if (!cols) return;

    if (cols == 1)
    {
        _shell_putc(sh, EK_KEY_BS);
        return;
    }

    char seq[16];
    int n = ek_snprintf(seq, sizeof(seq), "\033[%dD", (int)cols);
    _shell_raw_write(sh, seq, n);
}

// 终端光标右移 cols 列
static inline void _move_right(shell_t *sh, size_t cols)
{
    if (!cols) return;

    if (cols == 1)
    {
        _shell_puts(sh, EK_KEY_VT100_CURSOR_RIGHT);
        return;
    }

    char seq[16];
    int n = ek_snprintf(seq, sizeof(seq), "\033[%dC", (int)cols);
    _shell_raw_write(sh, seq, n);
}

// 光标移到行首
static inline void _cursor_home(shell_t *sh)
{
    if (!sh->cursor) return;

#    if EKCFG_SHELL_UTF8 == 1
    _move_left(sh, _utf8_cols(sh, 0, sh->cursor));
#    else
    char seq[16];
    int n = ek_snprintf(seq, sizeof(seq), "\033[%dD", (int)sh->cursor);
    _shell_raw_write(sh, seq, n);
#    endif

    sh->cursor = 0;
}

// 光标移到行尾
static inline void _cursor_end(shell_t *sh)
{
    if (sh->cursor >= sh->len) return;

#    if EKCFG_SHELL_UTF8 == 1
    _move_right(sh, _utf8_cols(sh, sh->cursor, sh->len));
#    else
    char seq[16];
    int n = ek_snprintf(seq, sizeof(seq), "\033[%dC", (int)(sh->len - sh->cursor));
    _shell_raw_write(sh, seq, n);
#    endif

    sh->cursor = sh->len;
}

// 光标左移一格
static inline void _cursor_left(shell_t *sh)
{
    if (!sh->cursor) return;

#    if EKCFG_SHELL_UTF8 == 1
    size_t cols = _utf8_prev_char(sh);
#    else
    size_t cols = 1;
    sh->cursor--;
#    endif

    _move_left(sh, cols);
}

// 光标右移一格
static inline void _cursor_right(shell_t *sh)
{
    if (sh->cursor >= sh->len) return;

#    if EKCFG_SHELL_UTF8 == 1
    size_t cols = _utf8_next_char(sh);
#    else
    size_t cols = 1;
    sh->cursor++;
#    endif

    _move_right(sh, cols);
}

// 删除光标前一个字符（Backspace）
static inline void _shell_backspace(shell_t *sh)
{
    if (!sh->cursor) return;

#    if EKCFG_SHELL_UTF8 == 1
    size_t i = sh->cursor - 1;
    while (i > 0 && EK_KEY_IS_UTF8_CONT_BYTE(sh->buf[i])) i--;

    size_t bytes = sh->cursor - i;
    size_t cols = EK_KEY_CHAR_DISPLAY_WIDTH(sh->buf[i]);
    sh->cursor = i;
#    else
    size_t bytes = 1;
    size_t cols = 1;
    sh->cursor--;
#    endif

    memmove(sh->buf + sh->cursor, sh->buf + sh->cursor + bytes, sh->len - sh->cursor - bytes);
    sh->len -= bytes;
    sh->buf[sh->len] = EK_KEY_NUL;

    // 行尾删除：左移 cols 列、打 cols 个空格、再左移 cols 列
    if (sh->cursor == sh->len)
    {
        _move_left(sh, cols);
        for (size_t k = 0; k < cols; k++) _shell_putc(sh, ' ');
        _move_left(sh, cols);
        return;
    }

    // 行中删除：左移 cols 列后重绘剩余内容，补空格，再退回
    _move_left(sh, cols);
    _shell_raw_write(sh, sh->buf + sh->cursor, sh->len - sh->cursor);
    for (size_t k = 0; k < cols; k++) _shell_putc(sh, ' ');
    _move_left(sh, (sh->len - sh->cursor) + cols);
}

// 删除光标所在字符（Delete），光标位置不变
static inline void _shell_delete_forward(shell_t *sh)
{
    if (sh->cursor >= sh->len) return;

#    if EKCFG_SHELL_UTF8 == 1
    size_t bytes = EK_KEY_UTF8_CHAR_LEN(sh->buf[sh->cursor]);
    size_t cols = EK_KEY_CHAR_DISPLAY_WIDTH(sh->buf[sh->cursor]);
#    else
    size_t bytes = 1;
    size_t cols = 1;
#    endif

    if (sh->cursor + bytes > sh->len) bytes = sh->len - sh->cursor;

    memmove(sh->buf + sh->cursor, sh->buf + sh->cursor + bytes, sh->len - sh->cursor - bytes);
    sh->len -= bytes;
    sh->buf[sh->len] = EK_KEY_NUL;

    // 光标不动，从当前位置重绘剩余内容并补空格擦尾
    _shell_raw_write(sh, sh->buf + sh->cursor, sh->len - sh->cursor);
    for (size_t k = 0; k < cols; k++) _shell_putc(sh, ' ');
    _move_left(sh, (sh->len - sh->cursor) + cols);
}

// 清空当前行输入
static inline void _shell_clear_line(shell_t *sh)
{
    if (sh->len == 0) return;

    _cursor_home(sh);
    _shell_puts(sh, EK_KEY_VT100_CLEAR_TO_EOL);

    sh->cursor = 0;
    sh->len = 0;
    sh->buf[0] = EK_KEY_NUL;
}

// 清屏并重绘提示符与当前输入内容
static inline void _shell_refresh_screen(shell_t *sh)
{
    _shell_puts(sh, EK_KEY_VT100_CLEAR_SCREEN);
    _shell_puts(sh, sh->prompt);
    _shell_raw_write(sh, sh->buf, sh->len);

    if (sh->cursor >= sh->len) return;

#    if EKCFG_SHELL_UTF8 == 1
    _move_left(sh, _utf8_cols(sh, sh->cursor, sh->len));
#    else
    char seq[16];
    int n = ek_snprintf(seq, sizeof(seq), "\033[%dD", (int)(sh->len - sh->cursor));
    _shell_raw_write(sh, seq, n);
#    endif
}

static int _shell_parse_args(char *line, char *argv[], int max_args)
{
    int count = 0;
    char *p = line;
    bool in_quote = false;

    while (*p && count < max_args)
    {
        // 跳过前导空白符
        while (*p && (*p == ' ' || *p == EK_KEY_TAB) && !in_quote) p++;
        if (*p == EK_KEY_NUL) break;

        // 检测引号包裹参数起始
        if (*p == '"')
        {
            in_quote = true;
            p++;
        }

        // 记录参数起始位置
        argv[count++] = p;

        while (*p)
        {
            // 引号模式匹配配对的双引号
            if (in_quote)
            {
                if (*p == '"')
                {
                    in_quote = false;
                    *p++ = EK_KEY_NUL;
                    break;
                }
            }
            // 普通模式匹配空白分隔符
            else
            {
                if (*p == EK_KEY_TAB || *p == ' ')
                {
                    *p++ = EK_KEY_NUL;
                    break;
                }
            }
            p++;
        }
    }

    return count;
}

static void _shell_exec(shell_t *sh)
{
    // 输出换行
    _shell_puts(sh, CRLF);

    // 空行直接重显提示符
    if (!sh->len)
    {
        _shell_puts(sh, sh->prompt);
        return;
    }

    sh->buf[sh->len] = EK_KEY_NUL;

    char *argv[EKCFG_SHELL_CMD_MAX_ARGS];
    int argc = _shell_parse_args(sh->buf, argv, EKCFG_SHELL_CMD_MAX_ARGS);
    if (argc)
    {
        const ek_shell_cmd_t *cmd = _shell_cmd_find(argv[0]);
        if (cmd)
        {
            int ret = cmd->fn(argc, argv, sh);
            if (ret)
            {
                char err_buf[32];
                int n = ek_snprintf(err_buf, sizeof(err_buf), "[Process exited %d]." CRLF, ret);
                ek_vfs_write(sh->out_fd, err_buf, n);
            }
        }
        else
        {
            _shell_puts(sh, "shell: command not found: ");
            _shell_puts(sh, argv[0]);
            _shell_puts(sh, CRLF);
        }
    }

    // 重置行编辑缓冲区
    sh->len = 0;
    sh->cursor = 0;
    sh->buf[0] = EK_KEY_NUL;

    _shell_puts(sh, sh->prompt);
}

static void _shell_handle_char(shell_t *sh, char ch)
{
    // 解析 VT100/ANSI 多字节转义序列
    if (sh->ansi_state == ANSI_STATE_ESC)
    {
        sh->ansi_state = ch == '[' ? ANSI_STATE_BRACKET : ANSI_STATE_NORMAL;
        return;
    }
    else if (sh->ansi_state == ANSI_STATE_BRACKET)
    {
        if (ch >= '0' && ch <= '9')
        {
            sh->ansi_arg = ch;
            sh->ansi_state = ANSI_STATE_ARG;
            return;
        }

        sh->ansi_state = ANSI_STATE_NORMAL;
        switch (ch)
        {
            case EK_KEY_ANSI_CODE_UP:
                // TODO 上方向键：历史记录回溯
                break;
            case EK_KEY_ANSI_CODE_DOWN:
                // TODO 下方向键：历史记录前进
                break;
            case EK_KEY_ANSI_CODE_RIGHT:
                _cursor_right(sh);
                break;
            case EK_KEY_ANSI_CODE_LEFT:
                _cursor_left(sh);
                break;
            default:
                break;
        }
        return;
    }
    else if (sh->ansi_state == ANSI_STATE_ARG)
    {
        sh->ansi_state = ANSI_STATE_NORMAL;

        if (ch == '~')
        {
            switch (sh->ansi_arg)
            {
                case EK_KEY_ANSI_NUM_DELETE:
                    // Delete 键：向后删除
                    _shell_delete_forward(sh);
                    break;
                case EK_KEY_ANSI_NUM_HOME:
                    // Home 键
                    _cursor_home(sh);
                    break;
                case EK_KEY_ANSI_NUM_END:
                    // End 键
                    _cursor_end(sh);
                    break;
                default:
                    break;
            }
        }
        return;
    }

    switch (ch)
    {
        case EK_KEY_ESC:
            // 检测到 ESC 转义序列起始字符
            sh->ansi_state = ANSI_STATE_ESC;
            return;

        case EK_KEY_CR:
        case EK_KEY_LF:
            // 回车换行触发命令执行
            return _shell_exec(sh);

        case EK_KEY_TAB:
            // TODO 制表符触发自动补全
            return;

        case EK_KEY_BS:
        case EK_KEY_DEL:
            // 退格键：终端通常发 0x7F (DEL)，部分串口终端发 0x08 (BS)
            _shell_backspace(sh);
            return;

        case EK_KEY_CTRL_A:
            _cursor_home(sh);
            return;

        case EK_KEY_CTRL_E:
            _cursor_end(sh);
            return;

        case EK_KEY_CTRL_B:
            _cursor_left(sh);
            return;

        case EK_KEY_CTRL_F:
            _cursor_right(sh);
            return;

        case EK_KEY_CTRL_D:
            _shell_delete_forward(sh);
            return;

        case EK_KEY_CTRL_U:
            _shell_clear_line(sh);
            return;

        case EK_KEY_CTRL_L:
            _shell_refresh_screen(sh);
            return;

        default:
            break;
    }

    // 忽略未被上面处理的控制字符，避免污染行缓冲
    if ((uint8_t)ch < 0x20) return;

    // 普通可见字符输入
#    if EKCFG_SHELL_UTF8 == 1
    // UTF-8 模式下字节原样存储与回显，由终端负责拼字
    char disp_ch = ch;
#    else
    // 非 UTF-8 模式下多字节字符统一显示为 ?
    char disp_ch = EK_KEY_IS_ASCII(ch) ? ch : '?';
#    endif

    if (sh->len < EKCFG_SHELL_BUF_SIZE - 1)
    {
        // 在末尾追加字符
        if (sh->cursor == sh->len)
        {
            sh->buf[sh->cursor++] = disp_ch;
            sh->buf[++sh->len] = EK_KEY_NUL;
            _shell_putc(sh, disp_ch);
        }
        // 在中间插入字符并移动后续字符
        else if (sh->cursor < sh->len)
        {
            memmove(sh->buf + sh->cursor + 1, sh->buf + sh->cursor, sh->len - sh->cursor);
            sh->buf[sh->cursor++] = disp_ch;
            sh->buf[++sh->len] = EK_KEY_NUL;
            _shell_raw_write(sh, sh->buf + sh->cursor - 1, sh->len - sh->cursor + 1);

            // 将终端光标移回插入位置
            char restore_seq[16];
            int seq_len = ek_snprintf(restore_seq, sizeof(restore_seq), "\033[%dD", (int)(sh->len - sh->cursor));
            _shell_raw_write(sh, restore_seq, seq_len);
        }
    }
}

int ek_shell_step(char *buf, size_t size)
{
    if (!buf || !size) return -EINVAL;

    shell_t *sh = &s_default_shell;

    ssize_t n = ek_vfs_read(sh->in_fd, buf, size);

    if (n > 0)
    {
        for (ssize_t i = 0; i < n; i++)
        {
            _shell_handle_char(sh, buf[i]);
        }
        return (int)n;
    }

    return (n == -EAGAIN) ? 0 : (int)n;
}

int ek_shell_input(const void *data, size_t len)
{
    if (!data || !len) return -EINVAL;

    shell_t *sh = &s_default_shell;
    const char *p = (const char *)data;
    for (size_t i = 0; i < len; i++)
    {
        _shell_handle_char(sh, p[i]);
    }

    return 0;
}

// 内置命令
#    if EKCFG_SHELL_BUILTIN_CMDS == 1

static int _builtin_cmd_hello(int argc, char *argv[], void *sh)
{
    __EK_UNUSED(argc);
    __EK_UNUSED(argv);
    __EK_UNUSED(sh);

    ek_printf("hello, ek_platform!" CRLF);
    return 0;
}
EK_DEFINE_SHELL_CMD(hello, _builtin_cmd_hello);

static int _builtin_cmd_echo(int argc, char *argv[], void *sh)
{
    for (int i = 1; i < argc; i++)
    {
        ek_printf("%s%s", argv[i], (i + 1 < argc) ? " " : "");
    }
    ek_printf(CRLF);

    return 0;
}
EK_DEFINE_SHELL_CMD(echo, _builtin_cmd_echo);

static int _builtin_cmd_tree(int argc, char *argv[], void *sh)
{
    __EK_UNUSED(argc);
    __EK_UNUSED(argv);
    __EK_UNUSED(sh);
    static char buf[256];

    int len = ek_vfs_tree_dump(buf, sizeof(buf));
    if (len < 0) return len;

    ek_printf("%s", buf);
    return 0;
}
EK_DEFINE_SHELL_CMD(tree, _builtin_cmd_tree);

static int _builtin_cmd_clear(int argc, char *argv[], void *sh)
{
    __EK_UNUSED(argc);
    __EK_UNUSED(argv);

    _shell_puts((shell_t *)sh, EK_KEY_VT100_CLEAR_SCREEN);
    return 0;
}
EK_DEFINE_SHELL_CMD(clear, _builtin_cmd_clear);

static int _builtin_cmd_version(int argc, char *argv[], void *sh)
{
    __EK_UNUSED(argc);
    __EK_UNUSED(argv);
    __EK_UNUSED(sh);

    int len = ek_printf("version: v%d.%d.%d." CRLF, EK_VERSION_MAJOR, EK_VERSION_MINOR, EK_VERSION_PATCH);
    return len > 0 ? 0 : 1;
}
EK_DEFINE_SHELL_CMD(version, _builtin_cmd_version);

#    endif // EKCFG_SHELL_BUILTIN_CMDS == 1

#endif // EKCFG_SHELL == 1
