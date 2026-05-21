/*
 * @brief GPIO 驱动实现 (S32K144)
 *        通过直接操作寄存器实现 GPIO 输入/输出功能
 *
 * @note 参考: S32K1xx Reference Manual, Chapter 46 (GPIO), Chapter 47 (PORT)
 *       对应书籍：第4章 §4.2~§4.3
 *
 * TODO：阅读 §4.2 后，按以下步骤实现每个函数
 *       关键寄存器：
 *         - PDOR: 端口数据输出寄存器 (偏移 0x00)
 *         - PSOR: 端口置位输出寄存器 (偏移 0x04)
 *         - PCOR: 端口清零输出寄存器 (偏移 0x08)
 *         - PTOR: 端口翻转输出寄存器 (偏移 0x0C)
 *         - PDIR: 端口数据输入寄存器 (偏移 0x10)
 *         - PDDR: 端口数据方向寄存器 (偏移 0x14)
 *         - PCR:  引脚控制寄存器 (PORT 模块, 每个引脚一个)
 *         - PCC:  时钟门控寄存器 (使能外设时钟)
 */
#include "gpio_driver.h"

/* ========== 寄存器结构体定义 ========== */

/*
 * TODO §4.2.2：定义 GPIO 寄存器结构体
 * 提示：包含 PDOR/PSOR/PCOR/PTOR/PDIR/PDDR 六个寄存器
 * 每个 32 位，按顺序排列
 */
typedef struct {
    /* GPIO 寄存器映射 */
} gpio_regs_t;

/*
 * TODO §4.2.1：定义 PORT 引脚控制寄存器结构体
 * 提示：包含 PCR[0~31] + GPCLR + GPCHR + 保留 + ISFR
 */
typedef struct {
    /* PORT 控制寄存器映射 */
} port_regs_t;

/* ========== 寄存器基址映射 ========== */

/* TODO：将 4 个 GPIO 端口的基地址映射为 gpio_regs_t* 指针数组 */
/* TODO：将 4 个 PORT 端口的基地址映射为 port_regs_t* 指针数组 */
/* 提示：PORT 基址 = GPIO 基址 + 0x1000 */

/* ========== 时钟门控 ========== */

/* TODO：定义 PCC 寄存器基址 (0x40065000) 和 PCC_PORT 偏移 */
/* 提示：PCC_PORTA 偏移 0x200, PCC_PORTB 偏移 0x204, 以此类推 */

/* ========== 函数实现 ========== */

int gpio_init(gpio_port_t port, uint8_t pin,
              gpio_direction_t dir, gpio_pull_t pull) {
    /*
     * TODO §4.2.3：实现 GPIO 初始化
     * 步骤：
     *   1. 参数检查 (port > GPIO_PORT_D || pin > 31 返回 -1)
     *   2. 使能 PORT 时钟: PCC_PORTCn[30] = 1 (CGC 位)
     *   3. 配置 PORT PCR 寄存器:
     *      - MUX[10:8] = 001 (GPIO 功能)
     *      - PE[1] / PS[0] 配置上拉/下拉
     *   4. 配置 PDDR 方向位:
     *      - 输出: PDDR |= (1 << pin)
     *      - 输入: PDDR &= ~(1 << pin)
     *   5. 返回 0
     */
    (void)port;
    (void)pin;
    (void)dir;
    (void)pull;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }

    return 0;
}

void gpio_write(gpio_port_t port, uint8_t pin, bool value) {
    /*
     * TODO §4.2.2：实现 GPIO 输出
     *   value == true  → PSOR 寄存器置位 (指定引脚输出高电平)
     *   value == false → PCOR 寄存器置位 (指定引脚输出低电平)
     * 提示：PSOR/PCOR 写 1 有效，写 0 不影响
     */
    (void)port;
    (void)pin;
    (void)value;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }
}

bool gpio_read(gpio_port_t port, uint8_t pin) {
    /*
     * TODO §4.2.2：实现 GPIO 输入读取
     *   返回 PDIR 寄存器对应引脚位的值
     * 提示：return (PDIR >> pin) & 1;
     */
    (void)port;
    (void)pin;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }

    return false;
}

void gpio_toggle(gpio_port_t port, uint8_t pin) {
    /*
     * TODO §4.2.2：实现 GPIO 输出翻转
     *   向 PTOR 寄存器对应位写 1
     * 提示：PTOR 写 1 翻转，写 0 不影响
     */
    (void)port;
    (void)pin;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }
}

void gpio_delay_ms(uint32_t ms) {
    /*
     * TODO：实现简单自旋延迟
     * 提示：48MHz 下约 16000 次空循环 ≈ 1ms
     *       用双层 for 循环实现
     */
    (void)ms;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }
}
