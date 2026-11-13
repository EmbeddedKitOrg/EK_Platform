#include "ek/shell/ek_shell.h"

#if EKCFG_SHELL == 1

#    include "ek/mem/ek_heap.h"
#    include "ek/container/ek_list.h"
#    include "ek/shell/ek_shell_keys.h"
#    include "ek/vfs/ek_vfs_core.h"
#    include "ek/core/ek_io.h"

enum
{

    ANSI_STATE_NORMAL, // 正常接收普通字符
    ANSI_STATE_ESC, // 收到 0x1B 转义前缀 (ESC)
    ANSI_STATE_BRACKET, // 收到 '[' 控制序列引导符
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
    s_default_shell.buf[0] = '\0';
    if (!s_default_shell.prompt)
    {
        s_default_shell.prompt = EKCFG_SHELL_PROMPT;
    }

    // 初始化完成即进入交互，先输出一次提示符
    ek_vfs_write(s_default_shell.out_fd, s_default_shell.prompt, strlen(s_default_shell.prompt));

    return 0;
}

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

static int _shell_parse_args(char *line, char *argv[], int max_args)
{
    int count = 0;
    char *p = line;
    bool in_quote = false;

    while (*p && count < max_args)
    {
        // 跳过前导空白符
        while (*p && (*p == ' ' || *p == '\t') && !in_quote) p++;
        if (*p == '\0') break;

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
                    *p++ = '\0';
                    break;
                }
            }
            // 普通模式匹配空白分隔符
            else
            {
                if (*p == '\t' || *p == ' ')
                {
                    *p++ = '\0';
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
    _shell_puts(sh, "\r\n");

    // 空行直接重显提示符
    if (!sh->len)
    {
        _shell_puts(sh, sh->prompt);
        return;
    }

    sh->buf[sh->len] = '\0';

    char *argv[EKCFG_SHELL_CMD_MAX_ARGS];
    int argc = _shell_parse_args(sh->buf, argv, EKCFG_SHELL_CMD_MAX_ARGS);
    if (argc)
    {
        const ek_shell_cmd_t *cmd = _shell_cmd_find(argv[0]);
        if (cmd)
        {
            int ret = cmd->fn(argc, argv);
            if (ret)
            {
                char err_buf[32];
                int n = ek_snprintf(err_buf, sizeof(err_buf), "[Process exited %d]\r\n", ret);
                ek_vfs_write(sh->out_fd, err_buf, n);
            }
        }
        else
        {
            _shell_puts(sh, "ek: command not found: ");
            _shell_puts(sh, argv[0]);
            _shell_puts(sh, "\r\n");
        }
    }

    // 重置行编辑缓冲区
    sh->len = 0;
    sh->cursor = 0;
    sh->buf[0] = '\0';

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
        sh->ansi_state = ANSI_STATE_NORMAL;
        switch (ch)
        {
            case 'A':
                // 上方向键：历史记录回溯（待实现）
                break;
            case 'B':
                // 下方向键：历史记录前进（待实现）
                break;
            case 'C':
                // 右方向键：光标右移
                if (sh->cursor < sh->len)
                {
                    sh->cursor++;
                    _shell_puts(sh, "\033[C");
                }
                break;
            case 'D':
                // 左方向键：光标左移
                if (sh->cursor > 0)
                {
                    sh->cursor--;
                    _shell_puts(sh, "\b");
                }
                break;
            default:
                break;
        }
        return;
    }

    // 检测到 ESC 转义序列起始字符
    if ((uint8_t)ch == 0x1B)
    {
        sh->ansi_state = ANSI_STATE_ESC;
        return;
    }

    // 回车换行触发命令执行
    if (ch == '\r' || ch == '\n')
    {
        return _shell_exec(sh);
    }

    // 制表符触发自动补全（待实现）
    if (ch == '\t')
    {
        return;
    }

    // 退格键：终端通常发 0x7F (DEL)，部分串口终端发 0x08 (BS)
    // 注意必须放在可见字符判断之前，否则 0x7F 会被当成普通字符插入
    if (ch == '\b' || (uint8_t)ch == 0x7F)
    {
        if (sh->cursor > 0)
        {
            // 光标前移并原地删除一个字节
            sh->cursor--;
            memmove(sh->buf + sh->cursor, sh->buf + sh->cursor + 1, sh->len - sh->cursor - 1);
            sh->buf[--sh->len] = '\0';

            if (sh->cursor == sh->len)
            {
                // 删除行尾字符，退格后覆盖空格再退格
                _shell_puts(sh, "\b \b");
            }
            else
            {
                // 删除行中字符，光标左移后重绘剩余内容并补一个空格
                _shell_putc(sh, '\b');
                _shell_raw_write(sh, sh->buf + sh->cursor, sh->len - sh->cursor);
                _shell_putc(sh, ' ');

                // 把终端光标退回删除位置
                char seq[16];
                int n = ek_snprintf(seq, sizeof(seq), "\033[%dD", (int)(sh->len - sh->cursor) + 1);
                _shell_raw_write(sh, seq, n);
            }
        }
        return;
    }

    // 普通可见字符输入
    if ((uint8_t)ch >= 0x20)
    {
        if (sh->len < EKCFG_SHELL_BUF_SIZE - 1)
        {
            // 在末尾追加字符
            if (sh->cursor == sh->len)
            {
                sh->buf[sh->cursor++] = ch;
                sh->buf[++sh->len] = '\0';
                _shell_putc(sh, ch);
            }
            // 在中间插入字符并移动后续字符
            else if (sh->cursor < sh->len)
            {
                memmove(sh->buf + sh->cursor + 1, sh->buf + sh->cursor, sh->len - sh->cursor);
                sh->buf[sh->cursor++] = ch;
                sh->buf[++sh->len] = '\0';
                _shell_raw_write(sh, sh->buf + sh->cursor - 1, sh->len - sh->cursor + 1);

                // 将终端光标移回插入位置
                char restore_seq[16];
                int seq_len = ek_snprintf(restore_seq, sizeof(restore_seq), "\033[%dD", (int)(sh->len - sh->cursor));
                _shell_raw_write(sh, restore_seq, seq_len);
            }
        }
        return;
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

#endif // EKCFG_SHELL == 1
