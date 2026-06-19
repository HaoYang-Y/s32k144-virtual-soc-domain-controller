/**
 * @file    Log.c
 * @brief   分级日志模块实现 — DWT 时间戳 + 环形缓冲区 + UART 输出
 *
 * @note    不使用 printf/snprintf 等 libc 函数，避免 -nostdlib 链接问题。
 *          格式化为纯手写，支持 %s %d %u %lu %x %02X %04X 基本模式。
 */

#include "Log.h"
#include <stdarg.h>

/* ===================================================================
 *  ARM DWT 周期计数器基址 (Cortex-M4)
 * =================================================================== */
#define DWT_CTRL    (*((volatile uint32_t *)0xE0001000UL))
#define DWT_CYCCNT  (*((volatile uint32_t *)0xE0001004UL))
#define DEMCR       (*((volatile uint32_t *)0xE000EDFCUL))
#define DEMCR_TRCENA  (1UL << 24)
#define DWT_CTRL_CYCCNTENA (1UL << 0)

/* Core clock = 48 MHz → 48000 cycles = 1 ms */
#define CORE_CLK_HZ   48000000UL
#define CYCLES_PER_MS (CORE_CLK_HZ / 1000UL)

/* ===================================================================
 *  模块级静态变量
 * =================================================================== */

/** 环形缓冲区 */
static char   ringbuf[LOG_RINGBUF_SIZE];
static uint32_t ring_wr;     /* 写入指针 (字节) */
static uint32_t ring_cnt;    /* 已写入总字节数 (用于计算 full) */

/** 输出回调 */
static Log_OutputFn output_fn = NULL;

/** DWT 是否已初始化 */
static bool dwt_inited = false;

/* ===================================================================
 *  简易字符串操作 (避免依赖 libc)
 * =================================================================== */

static size_t log_strlen(const char *s)
{
    size_t n = 0U;
    if (s) { while (*s++) n++; }
    return n;
}

static void log_strcpy(char *dst, const char *src)
{
    while (*src) *dst++ = *src++;
}

static void log_memset(void *p, int c, size_t n)
{
    unsigned char *d = (unsigned char *)p;
    for (size_t i = 0U; i < n; i++) d[i] = (unsigned char)c;
}

/* ===================================================================
 *  简易整数格式化
 * =================================================================== */

/** 将 uint32 转为十进制字符串，返回字符串长度 */
static uint32_t log_utoa10(uint32_t val, char *buf)
{
    if (val == 0U) { buf[0] = '0'; buf[1] = '\0'; return 1U; }
    char tmp[11];
    uint32_t i = 0U;
    while (val > 0U) { tmp[i++] = (char)('0' + (val % 10U)); val /= 10U; }
    uint32_t len = i;
    while (i > 0U) { *buf++ = tmp[--i]; }
    *buf = '\0';
    return len;
}

/** 将 uint32 转为十六进制字符串 (小写)，返回字符串长度 */
static uint32_t log_utoa_hex(uint32_t val, char *buf, int digits)
{
    if (digits <= 0) {
        /* 自动判断所需位数 (1-8) */
        if (val == 0U) digits = 1;
        else {
            uint32_t t = val;
            digits = 0;
            while (t > 0U) { t >>= 4; digits++; }
        }
    }
    buf[digits] = '\0';
    for (int i = digits - 1; i >= 0; i--) {
        uint32_t nib = val & 0xFU;
        buf[i] = (char)(nib < 10U ? '0' + nib : 'a' + nib - 10U);
        val >>= 4;
    }
    return (uint32_t)digits;
}

/** 将 uint32 转为大写十六进制字符串，返回字符串长度 */
static uint32_t log_utoa_HEX(uint32_t val, char *buf, int digits)
{
    uint32_t len = log_utoa_hex(val, buf, digits);
    for (uint32_t i = 0U; i < len; i++) {
        if (buf[i] >= 'a' && buf[i] <= 'f') buf[i] -= 32;
    }
    return len;
}

/* ===================================================================
 *  简易 vsnprintf (仅支持 %s %d %u %lu %x %X %02X %04X %03lX %02lX)
 * =================================================================== */

/** 将格式化字符串写入 buf，返回写入字符数 (不含 '\0') */
static int log_vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    if ((buf == NULL) || (size == 0U)) return 0;

    char *p = buf;
    char *end = buf + size - 1U;  /* 预留 '\0' */
    const char *f = fmt;

    if (f == NULL) { *p = '\0'; return 0; }

    while (*f && (p < end)) {
        if (*f != '%') {
            *p++ = *f++;
            continue;
        }
        f++; /* 跳过 '%' */

        /* 解析宽度修饰符 */
        int width = 0;
        bool pad_zero = false;

        if (*f == '0') { pad_zero = true; f++; }
        while (*f >= '0' && *f <= '9') { width = width * 10 + (*f - '0'); f++; }

        /* 长度修饰符 l */
        bool is_long = false;
        if (*f == 'l') { is_long = true; f++; }

        char spec = *f;
        if (spec == '\0') break;
        f++;

        char num_buf[32];
        const char *str_val = NULL;
        uint32_t num_len = 0U;

        switch (spec) {
        case 's':
            str_val = va_arg(args, const char *);
            if (str_val == NULL) str_val = "(null)";
            num_len = (uint32_t)log_strlen(str_val);
            /* 截断到剩余空间 */
            if (num_len > (uint32_t)(end - p)) num_len = (uint32_t)(end - p);
            for (uint32_t j = 0U; j < num_len; j++) *p++ = str_val[j];
            continue;

        case 'd':
        {
            int val = va_arg(args, int);
            if (val < 0) { *p++ = '-'; val = -val; }
            num_len = log_utoa10((uint32_t)val, num_buf);
            break;
        }
        case 'u':
        {
            uint32_t val;
            if (is_long) val = (uint32_t)va_arg(args, unsigned long);
            else         val = va_arg(args, unsigned int);
            num_len = log_utoa10(val, num_buf);
            break;
        }
        case 'x':
        {
            uint32_t val = va_arg(args, unsigned int);
            num_len = log_utoa_hex(val, num_buf, width > 0 ? width : 0);
            break;
        }
        case 'X':
        {
            uint32_t val;
            if (is_long) val = (uint32_t)va_arg(args, unsigned long);
            else         val = va_arg(args, unsigned int);
            num_len = log_utoa_HEX(val, num_buf, width > 0 ? width : 0);
            break;
        }
        default:
            *p++ = spec; /* 不认识的格式符原样输出 */
            continue;
        }

        /* 填充 */
        if (pad_zero && (num_len < (uint32_t)width)) {
            uint32_t pad = (uint32_t)width - num_len;
            for (uint32_t j = 0U; (j < pad) && (p < end); j++) *p++ = '0';
        }
        for (uint32_t j = 0U; (j < num_len) && (p < end); j++) *p++ = num_buf[j];
    }
    *p = '\0';
    return (int)(p - buf);
}

/* ===================================================================
 *  DWT 周期计数器
 * =================================================================== */

static void dwt_init(void)
{
    if (dwt_inited) return;
    DEMCR |= DEMCR_TRCENA;
    DWT_CYCCNT = 0U;
    DWT_CTRL  |= DWT_CTRL_CYCCNTENA;
    dwt_inited = true;
}

uint32_t Log_GetTimeMs(void)
{
    if (!dwt_inited) return 0U;
    return DWT_CYCCNT / CYCLES_PER_MS;
}

/* ===================================================================
 *  环形缓冲区
 * =================================================================== */

static void ringbuf_write(char c)
{
    ringbuf[ring_wr] = c;
    ring_wr = (ring_wr + 1U) % LOG_RINGBUF_SIZE;
    ring_cnt++;
}

static void ringbuf_write_str(const char *s)
{
    while (*s) ringbuf_write(*s++);
}

static void ringbuf_reset(void)
{
    ring_wr  = 0U;
    ring_cnt = 0U;
    log_memset(ringbuf, 0, sizeof(ringbuf));
}

uint32_t Log_DumpRingBuf(char *buf, uint32_t size)
{
    if ((buf == NULL) || (size == 0U)) return 0U;

    uint32_t copied = 0U;
    uint32_t total  = (ring_cnt < LOG_RINGBUF_SIZE) ? ring_cnt : LOG_RINGBUF_SIZE;

    uint32_t start = (ring_cnt < LOG_RINGBUF_SIZE) ? 0U : ring_wr;

    for (uint32_t i = 0U; (i < total) && (copied < size - 1U); i++) {
        buf[copied++] = ringbuf[(start + i) % LOG_RINGBUF_SIZE];
    }
    buf[copied] = '\0';
    return copied;
}

/* ===================================================================
 *  核心日志函数
 * =================================================================== */

void Log_Write(char level, const char *tag, const char *fmt, ...)
{
    char line[128];
    char *p = line;
    char *end = line + sizeof(line) - 2U;  /* 留 \n 和 \0 */

    /* [ms] */
    *p++ = '[';
    {
        char ms_buf[16];
        log_utoa10(Log_GetTimeMs(), ms_buf);
        /* 左侧补 0 到 8 位 */
        uint32_t ms_len = (uint32_t)log_strlen(ms_buf);
        for (uint32_t i = ms_len; i < 8U && p < end; i++) *p++ = '0';
        for (uint32_t i = 0U; i < ms_len && p < end; i++) *p++ = ms_buf[i];
    }
    *p++ = ']';
    *p++ = ' ';

    /* [L] */
    *p++ = '[';
    *p++ = level;
    *p++ = ']';
    *p++ = ' ';

    /* tag: */
    {
        const char *t = (tag != NULL) ? tag : "?";
        while (*t && p < end) *p++ = *t++;
    }
    if (p < end) *p++ = ':';
    if (p < end) *p++ = ' ';

    /* 用户消息 */
    if (fmt != NULL) {
        va_list args;
        va_start(args, fmt);
        int n = log_vsnprintf(p, (size_t)(end - p + 1), fmt, args);
        va_end(args);
        p += n;
    }

    /* 追加换行 */
    if (p < line + sizeof(line) - 1) {
        *p++ = '\n';
        *p   = '\0';
    } else {
        line[sizeof(line) - 2] = '\n';
        line[sizeof(line) - 1] = '\0';
    }

    /* 写环形缓冲 + 输出 */
    ringbuf_write_str(line);
    if (output_fn != NULL) {
        output_fn(line);
    }
}

/* ===================================================================
 *  初始化
 * =================================================================== */

void Log_Init(void)
{
    ringbuf_reset();
    dwt_init();
    output_fn = NULL;
}

void Log_SetOutput(Log_OutputFn fn)
{
    output_fn = fn;
}
