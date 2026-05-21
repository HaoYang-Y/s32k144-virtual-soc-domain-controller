/*
 * @brief 定时器驱动实现 (S32K144)
 *        通过直接操作寄存器实现 SysTick 和 LPIT 定时器
 *
 * @note 参考: ARM Cortex-M4 内核参考手册 (SysTick)
 *             S32K1xx Reference Manual, Chapter 57 (LPIT)
 *       对应书籍：第7章 定时器
 *
 * TODO：阅读 §7.3~§7.4 后，按以下步骤实现每个函数
 *       关键寄存器 (SysTick):
 *         - SYST_CSR:  控制和状态 (ENABLE, TICKINT, CLKSOURCE)
 *         - SYST_RVR:  重装载值
 *         - SYST_CVR:  当前值
 *         - SYST_CALIB: 校准值
 *       关键寄存器 (LPIT):
 *         - MCR:  模块控制 (M_CEN, DBG_EN, DOZE_EN)
 *         - MSR:  模块状态 (TIFn)
 *         - TCTRL0~3: 通道控制 (T_EN, MODE, TSOT, TSOI)
 *         - TVAL0~3:  通道定时值
 *         - SET_T_EN_0~3: 通道启动寄存器
 */
#include "timer_driver.h"

/* ========== 全局变量 ========== */

/* TODO：定义系统心跳计数器，在 SysTick_Handler 中递增 */
/* static volatile uint64_t system_tick = 0; */

/* ========== 寄存器结构体定义 ========== */

/*
 * TODO §7.3.2：定义 SysTick 寄存器结构体
 * 提示：CSR / RVR / CVR / CALIB 四个寄存器，每个 32 位
 */
typedef struct {
    /* SysTick 寄存器映射 */
} systick_regs_t;

/*
 * TODO §7.4.2：定义 LPIT 寄存器结构体
 * 提示：MCR / MSR / M_SET_T_EN / M_CLR_T_EN 等
 *       以及 4 组通道寄存器 (TCTRLi / TVALi / ...)
 */
typedef struct {
    /* LPIT 寄存器映射 */
} lpit_regs_t;

/* ========== 寄存器基址映射 ========== */

/* TODO：映射 SysTick 和 LPIT 寄存器指针 */

/* ========== 函数实现 ========== */

int systick_init(uint32_t cpu_freq_hz) {
    /*
     * TODO §7.3.2：实现 SysTick 初始化
     * 步骤：
     *   1. 参数检查 (cpu_freq_hz == 0 返回 -1)
     *   2. reload = cpu_freq_hz / 1000 - 1
     *   3. 写 SYST_RVR = reload
     *   4. 写 SYST_CVR = 0 (清零)
     *   5. 写 SYST_CSR = 0x7 (ENABLE | TICKINT | CLKSOURCE)
     *   6. 返回 0
     */
    (void)cpu_freq_hz;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }

    return 0;
}

void systick_delay_ms(uint32_t ms) {
    /*
     * TODO §7.3.2：实现 SysTick 延时
     * 步骤：
     *   1. 记录当前 tick: start = systick_get_tick()
     *   2. while (systick_get_tick() - start < ms) { }
     * 注意：防止编译器优化，将 system_tick 声明为 volatile
     */
    (void)ms;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }
}

uint64_t systick_get_tick(void) {
    /*
     * TODO：返回 system_tick
     * 提示：在 SysTick_Handler 中递增 system_tick
     *       每次 SysTick 中断 = 1ms
     */
    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }

    return 0;
}

int lpit_init(lpit_channel_t channel, uint32_t period_us,
              uint32_t cpu_freq_hz) {
    /*
     * TODO §7.4.2：实现 LPIT 初始化
     * 步骤：
     *   1. 参数检查
     *   2. 使能 LPIT 时钟 (PCC_LPIT0[CGC])
     *   3. MCR[M_CEN] = 0 (先禁能模块进行配置)
     *   4. TVAL = period_us * (cpu_freq_hz / 1000000) - 1
     *   5. 配置 TCTRL:
     *      - T_EN = 1 (通道使能，但暂不启动)
     *      - MODE = 0 (周期性)
     *      - TSOT = 0 (软件触发)
     *   6. MCR[M_CEN] = 1 (使能模块)
     *   7. 返回 0
     */
    (void)channel;
    (void)period_us;
    (void)cpu_freq_hz;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }

    return 0;
}

void lpit_start(lpit_channel_t channel) {
    /*
     * TODO §7.4.2：启动指定 LPIT 通道
     *   SET_T_EN_n = 1 启动，M_CLR_T_EN_n = 1 停止
     * 提示：向 SET_T_EN 寄存器对应位写 1
     */
    (void)channel;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }
}

bool lpit_is_timeout(lpit_channel_t channel) {
    /*
     * TODO §7.4.2：检查超时标志
     *   读取 MSR[TIFn] 位
     *   读后自动清零
     */
    (void)channel;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }

    return false;
}

void SysTick_Handler(void) {
    /*
     * TODO：递增系统心跳计数器
     *   1. system_tick++
     * 提示：这是 SysTick 中断的服务函数
     *       不需要清除中断标志，硬件自动完成
     */
    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 未实现 */ }
}
