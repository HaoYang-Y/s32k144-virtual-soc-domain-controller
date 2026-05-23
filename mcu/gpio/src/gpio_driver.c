/*
 * @brief GPIO 驱动实现 (S32K144)
 *        通过直接操作寄存器实现 GPIO 输入/输出功能
 *
 * @note 参考: S32K1xx Reference Manual, Chapter 46 (GPIO), Chapter 47 (PORT)
 *       对应书籍：第4章 §4.2~§4.3
 *
 *       关键寄存器（GPIO 模块）：
 *         - PDOR: 端口数据输出寄存器 (偏移 0x00)
 *         - PSOR: 端口置位输出寄存器 (偏移 0x04)
 *         - PCOR: 端口清零输出寄存器 (偏移 0x08)
 *         - PTOR: 端口翻转输出寄存器 (偏移 0x0C)
 *         - PDIR: 端口数据输入寄存器 (偏移 0x10)
 *         - PDDR: 端口数据方向寄存器 (偏移 0x14)
 *
 *       关键寄存器（PORT 模块）：
 *         - PCRn: 引脚控制寄存器 (偏移 0x00 + 4*n)
 *
 *       时钟门控 (PCC 模块)：
 *         - PCC_PORTCn: PORT 时钟使能寄存器
 */
#include "gpio_driver.h"
#include <stddef.h>

/* ========== 寄存器结构体定义 ========== */

/**
 * @brief GPIO 寄存器映射结构体
 *        S32K1xx RM §46.3: GPIO memory map
 *        每个寄存器 32 位，连续排列
 */
typedef struct {
    volatile uint32_t PDOR; /**< 端口数据输出寄存器 (偏移 0x00) */
    volatile uint32_t PSOR; /**< 端口置位输出寄存器 (偏移 0x04) */
    volatile uint32_t PCOR; /**< 端口清零输出寄存器 (偏移 0x08) */
    volatile uint32_t PTOR; /**< 端口翻转输出寄存器 (偏移 0x0C) */
    volatile uint32_t PDIR; /**< 端口数据输入寄存器 (偏移 0x10) */
    volatile uint32_t PDDR; /**< 端口数据方向寄存器 (偏移 0x14) */
} gpio_regs_t;

/**
 * @brief PORT 引脚控制寄存器映射结构体
 *        S32K1xx RM §47.4: PORT memory map
 *        PCR[0..31] 每个引脚一个 32 位寄存器
 */
typedef struct {
    volatile uint32_t PCR[32];    /**< 引脚控制寄存器 (偏移 0x00~0x7C) */
    volatile uint32_t GPCLR;      /**< 全局引脚控制低半字 (偏移 0x80) */
    volatile uint32_t GPCHR;      /**< 全局引脚控制高半字 (偏移 0x84) */
    uint32_t        RESERVED0;    /**< 保留 (偏移 0x88) */
    uint32_t        RESERVED1;    /**< 保留 (偏移 0x8C) */
    volatile uint32_t ISFR;       /**< 中断状态标志寄存器 (偏移 0x90) */
} port_regs_t;

/* ========== PORT 模块基地址 ========== */
/* S32K1xx RM §47.4: PORT memory map */
#define PORT_PTA_BASE  (0x40049000u)
#define PORT_PTB_BASE  (0x4004A000u)
#define PORT_PTC_BASE  (0x4004B000u)
#define PORT_PTD_BASE  (0x4004C000u)

/* ========== 寄存器基址映射 ========== */

/* GPIO 模块基地址指针数组 */
static gpio_regs_t *const g_gpio_base[] = {
    (gpio_regs_t *)GPIO_PTA_BASE,  /* PORT A */
    (gpio_regs_t *)GPIO_PTB_BASE,  /* PORT B */
    (gpio_regs_t *)GPIO_PTC_BASE,  /* PORT C */
    (gpio_regs_t *)GPIO_PTD_BASE,  /* PORT D */
};

/* PORT 模块基地址（注意：PORT 与 GPIO 是独立的两个模块，地址不连续）*/
static port_regs_t *const g_port_base[] = {
    (port_regs_t *)PORT_PTA_BASE,  /* PORTA @ 0x40049000 */
    (port_regs_t *)PORT_PTB_BASE,  /* PORTB @ 0x4004A000 */
    (port_regs_t *)PORT_PTC_BASE,  /* PORTC @ 0x4004B000 */
    (port_regs_t *)PORT_PTD_BASE,  /* PORTD @ 0x4004C000 */
};

/* ========== 时钟门控 (PCC) 定义 ========== */
/* S32K1xx RM §35.3: PCC memory map
 * PCC 使用 "PCC 索引"（Index）而非连续偏移
 * PORTA=73, PORTB=74, PORTC=75, PORTD=76
 * 寄存器偏移 = PCC_BASE + (Index * 4)
 */
#define PCC_BASE        (0x40065000u)

/* PCC_PORT 寄存器偏移 = PCC_BASE + (Index * 4) */
#define PCC_PORTA_OFFSET (0x124u)  /* Index 73 */
#define PCC_PORTB_OFFSET (0x128u)  /* Index 74 */
#define PCC_PORTC_OFFSET (0x12Cu)  /* Index 75 */
#define PCC_PORTD_OFFSET (0x130u)  /* Index 76 */

/* PCC_CGC 位：Clock Gate Control，位 [30] */
#define PCC_CGC_MASK     (1u << 30)

/* 获取指定 PORT 的 PCC 寄存器地址 */
#define PCC_PORT_ADDR(port) \
    ((volatile uint32_t *)(PCC_BASE + PCC_PORTA_OFFSET + ((uint32_t)(port) * 4u)))

/**
 * @brief 检查参数有效性
 * @param[in]  port  端口号
 * @param[in]  pin   引脚号
 * @return     0=有效, -1=无效
 */
static int check_param(gpio_port_t port, uint8_t pin)
{
    if (port > GPIO_PORT_D)
    {
        return -1;
    }
    if (pin > 31)
    {
        return -1;
    }
    return 0;
}

/* ========== 函数实现 ========== */

int gpio_init(gpio_port_t port, uint8_t pin,
              gpio_direction_t dir, gpio_pull_t pull)
{
    volatile uint32_t *pcc_reg;
    port_regs_t      *port_reg;
    gpio_regs_t      *gpio_reg;
    uint32_t          pcr_value;

    /* 1. 参数检查 */
    if (check_param(port, pin) != 0)
    {
        return -1;
    }

    /* 获取寄存器指针 */
    gpio_reg = g_gpio_base[port];
    port_reg = g_port_base[port];
    pcc_reg  = PCC_PORT_ADDR(port);

    /*
     * 2. 配置 PORT 时钟：
     *    PCC_PORTCn[5:3] = 011 (PCS = FIRCDIV2, 48MHz)
     *    PCC_PORTCn[30]  = 1   (CGC = 时钟门控使能)
     *
     * @note S32K1xx RM §35.3: PCC 寄存器
     *       PCS=000 表示时钟关闭（复位默认值），
     *       必须设置 PCS=011 选择 FIRC 48MHz 时钟源，
     *       再设 CGC=1 打开门控，PORT 模块才能工作。
     */
    *pcc_reg |= (3u << 3) | PCC_CGC_MASK;

    /* 3. 配置 PORT PCR 寄存器 */
    pcr_value = 0u;

    /* MUX[10:8] = 001 (GPIO 功能) */
    pcr_value |= (1u << 8);

    /* 上拉/下拉配置 */
    switch (pull)
    {
        case GPIO_PULL_UP:
            /* PE[1] = 1 (使能上拉/下拉), PS[0] = 1 (上拉) */
            pcr_value |= (3u << 0);
            break;

        case GPIO_PULL_DOWN:
            /* PE[1] = 1 (使能上拉/下拉), PS[0] = 0 (下拉) */
            pcr_value |= (2u << 0);
            break;

        case GPIO_PULL_DISABLE:
        default:
            /* PE[1] = 0, PS[0] = 0 (无上拉/下拉) */
            break;
    }

    /* 写入 PCR 寄存器 */
    port_reg->PCR[pin] = pcr_value;

    /* 4. 配置 PDDR 方向位 */
    if (dir == GPIO_DIR_OUTPUT)
    {
        /* 输出模式：PDDR 对应位置 1 */
        gpio_reg->PDDR |= (1u << pin);
    }
    else
    {
        /* 输入模式：PDDR 对应位清 0 */
        gpio_reg->PDDR &= ~(1u << pin);
    }

    return 0;
}

void gpio_write(gpio_port_t port, uint8_t pin, bool value)
{
    gpio_regs_t *gpio_reg;

    /* 参数检查，无效参数直接返回 */
    if (check_param(port, pin) != 0)
    {
        return;
    }

    gpio_reg = g_gpio_base[port];

    if (value)
    {
        /* 输出高电平：向 PSOR 对应位写 1 */
        gpio_reg->PSOR = (1u << pin);
    }
    else
    {
        /* 输出低电平：向 PCOR 对应位写 1 */
        gpio_reg->PCOR = (1u << pin);
    }
}

bool gpio_read(gpio_port_t port, uint8_t pin)
{
    gpio_regs_t *gpio_reg;

    /* 参数检查，无效参数返回 false */
    if (check_param(port, pin) != 0)
    {
        return false;
    }

    gpio_reg = g_gpio_base[port];

    /* 读取 PDIR 寄存器对应引脚位的值 */
    return (bool)((gpio_reg->PDIR >> pin) & 1u);
}

void gpio_toggle(gpio_port_t port, uint8_t pin)
{
    gpio_regs_t *gpio_reg;

    /* 参数检查，无效参数直接返回 */
    if (check_param(port, pin) != 0)
    {
        return;
    }

    gpio_reg = g_gpio_base[port];

    /* 翻转输出：向 PTOR 对应位写 1 */
    gpio_reg->PTOR = (1u << pin);
}

void gpio_delay_ms(uint32_t ms)
{
    /*
     * 简单的 for 循环延迟
     * S32K144 默认 Core Clock = 48MHz
     * 经过实测校准：48MHz 下 1ms ≈ 12000 条空循环
     * 使用 volatile 防止编译器优化掉循环
     */
    volatile uint32_t i, j;

    for (i = 0; i < ms; i++)
    {
        /* 48MHz 下 1ms 约需 12000 次空循环 */
        for (j = 0; j < 12000u; j++)
        {
            /* 空操作，消耗 CPU 周期 */
            __asm__ volatile ("nop");
        }
    }
}
