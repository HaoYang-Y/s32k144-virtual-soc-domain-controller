/*
 * @brief S32K144 启动代码 — 中断向量表 + Reset_Handler
 *        对应书籍：§4.5 工程文件组织框架
 *
 * @note 功能：
 *       1. 定义中断向量表（.isr_vector 段），包含栈顶和 Reset_Handler
 *       2. Reset_Handler 完成 .data 段复制和 .bss 段清零
 *       3. 提供其他异常/中断的默认 handler（弱符号）
 *
 * @warning 本文件使用 __attribute__ 实现，需 GCC 编译器
 *          链接脚本 mcu/s32k144_flash.ld 已定义 _estack 等符号
 */

#include <stdint.h>

/* ========== 外部符号声明 ========== */
extern uint32_t _estack;       /* 栈顶地址（来自链接脚本）*/
extern uint32_t _sidata;       /* .data 初始值在 Flash 中的起始地址 */
extern uint32_t _sdata;        /* .data 段在 SRAM 中的起始地址 */
extern uint32_t _edata;        /* .data 段在 SRAM 中的结束地址 */
extern uint32_t _sbss;         /* .bss 段在 SRAM 中的起始地址 */
extern uint32_t _ebss;         /* .bss 段在 SRAM 中的结束地址 */
extern int main(void);         /* 主函数 */

/* ========== 异常/中断处理函数声明 ========== */
void Reset_Handler(void);
static void Default_Handler(void);

/* ========== 弱符号定义（默认异常处理）========== */
/* 所有异常/中断的默认处理：死循环 */
void NMI_Handler(void)           __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)      __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)           __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)      __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)       __attribute__((weak, alias("Default_Handler")));

/* PORT 中断（用于按键 GPIO 中断）*/
void PORTC_IRQHandler(void)      __attribute__((weak, alias("Default_Handler")));

/**
 * @brief S32K144 Flash 配置字 (Flash Configuration Field)
 *        必须位于 0x400-0x40F，紧接 1KB 向量表之后
 *
 * @note 参见 S32K1xx RM §52.7.2 Flash Configuration Field 描述：
 *       0x400-0x403: Backdoor Comparison Key (默认 0xFF)
 *       0x404-0x407: Flash Protection (默认 0xFF)
 *       0x408-0x40B: Flash Nonvolatile Option (FOPT, 默认 0xFF)
 *       0x40C-0x40D: Flash Security (FSEC 低字节 + reserved)
 *       0x40E-0x40F: Flash Security (FSEC 高字节)
 *
 *       FSEC 字节详解 (0x40E):
 *         [1:0] KEYEN: 后门密钥使能 (2'b10=禁用, 推荐)
 *         [3:2] MEEN: Mass Erase 使能 (1'b10=使能)
 *         [5:4] FSLACC: Factory Sector 访问 (1'b10=允许)
 *         [7:6] SEC: 安全状态 (1'b10=unsecured)
 *       0xFE = 0b11111110:
 *         SEC=2'b10 (unsecured)
 *         FSLACC=2'b10 (factory access allowed)
 *         MEEN=2'b10 (mass erase enabled)
 *         KEYEN=2'b10 (backdoor key disabled)
 *
 * @warning 缺少此字段会导致芯片保持 secured 状态，
 *         复位后进入 LOCKUP，JLink 无法烧录新程序
 */
__attribute__((section(".flash_config"), used))
const uint32_t g_flash_config[4] = {
    0xFFFFFFFFu,  /* 0x400: Backdoor Comparison Key (word 0) */
    0xFFFFFFFFu,  /* 0x404: Backdoor Comparison Key (word 1) */
    0xFFFFFFFFu,  /* 0x408: Flash Protection = 无保护 (FPROT=0xFF) */
    0xFFFEFFFFu,  /* 0x40C: FOPT[7:0]=0xFF, reserved[15:8]=0xFF, */
                  /*       FSEC[23:16]=0xFE(unsecured), reserved[31:24]=0xFF */
};

/* ========== 中断向量表 ========== */
/*
 * Cortex-M4 向量表结构（数组索引对应）：
 *   [0]    = 栈顶地址（MSP 初始值）
 *   [1]    = Reset_Handler（Exception #0）
 *   [2]    = NMI_Handler（Exception #2）
 *   [3]    = HardFault_Handler（Exception #3）
 *   [4-10] = MemManage/BusFault/UsageFault/保留/SVCall/DebugMon/保留
 *   [11]   = PendSV_Handler（Exception #14）
 *   [12]   = SysTick_Handler（Exception #15）
 *   [13]   = 保留（Exception #16）
 *   [14]   = 保留（Exception #17）
 *   [15]   = 保留（Exception #18）
 *   [16+]  = 外设中断（External IRQ #0 ~ #N）
 * S32K144 PORTC 中断 = IRQ #31 → 数组索引 = 16 + 31 = 47
 * 总行数 = 48（0~47）
 *
 * 注意：每个条目是 uint32_t（函数地址或栈顶地址）
 * GCC 支持将函数指针转换为 uint32_t，但会触发 -Wpedantic 警告
 * 此处使用 uint32_t 类型以消除警告
 */
__attribute__((section(".isr_vector"), used))
const uint32_t g_vector_table[48] = {
    /* [0]: 栈顶指针 */
    (uint32_t)&_estack,

    /* [1..15]: 系统异常 (Exception #0 ~ #14) */
    (uint32_t)Reset_Handler,      /* [1]: Exception #0 — 复位 */
    (uint32_t)NMI_Handler,        /* [2]: Exception #2 — NMI */
    (uint32_t)HardFault_Handler,  /* [3]: Exception #3 — Hard Fault */
    (uint32_t)MemManage_Handler,  /* [4]: Exception #4 — MemManage */
    (uint32_t)BusFault_Handler,   /* [5]: Exception #5 — Bus Fault */
    (uint32_t)UsageFault_Handler, /* [6]: Exception #6 — Usage Fault */
    0u,                           /* [7]: Exception #7 — 保留 */
    0u,                           /* [8]: Exception #8 — 保留 */
    0u,                           /* [9]: Exception #9 — 保留 */
    0u,                           /* [10]: Exception #10 — 保留 */
    (uint32_t)SVC_Handler,        /* [11]: Exception #11 — SVCall */
    (uint32_t)DebugMon_Handler,   /* [12]: Exception #12 — Debug Monitor */
    0u,                           /* [13]: Exception #13 — 保留 */
    (uint32_t)PendSV_Handler,     /* [14]: Exception #14 — PendSV */
    (uint32_t)SysTick_Handler,    /* [15]: Exception #15 — SysTick */

    /* [16..46]: 外设中断 IRQ #0 ~ #30 — 未使用，设为 0 */
    0u, 0u, 0u, 0u, 0u,  /* IRQ #0 ~ #4   */
    0u, 0u, 0u, 0u, 0u,  /* IRQ #5 ~ #9   */
    0u, 0u, 0u, 0u, 0u,  /* IRQ #10 ~ #14 */
    0u, 0u, 0u, 0u, 0u,  /* IRQ #15 ~ #19 */
    0u, 0u, 0u, 0u, 0u,  /* IRQ #20 ~ #24 */
    0u, 0u, 0u, 0u, 0u,  /* IRQ #25 ~ #29 */
    0u,                   /* IRQ #30       */

    /* [47]: 外设中断 IRQ #31 — PORTC (按键引脚中断) */
    (uint32_t)PORTC_IRQHandler,
};

/*
 * Reset_Handler 不能使用 __attribute__((naked)) + C 代码，
 * 因为编译器可能省略 prologue/epilogue 导致 .data 复制/ .bss 清零
 * 时栈指针未正确配置。使用标准 C 实现（栈指针已在硬件复位时从
 * 向量表 [0] 加载，因此 SP 可用）。
 */

/* ========== SCG 系统时钟初始化 ========== */
/* S32K1xx RM §39: System Clock Generator (SCG) */
#define SCG_BASE           (0x40064000u)
#define SCG_VERID          (*(volatile uint32_t *)(SCG_BASE + 0x00u))
#define SCG_PARAM          (*(volatile uint32_t *)(SCG_BASE + 0x04u))
#define SCG_CSR            (*(volatile uint32_t *)(SCG_BASE + 0x10u))
#define SCG_RCCR           (*(volatile uint32_t *)(SCG_BASE + 0x14u))
#define SCG_VCCR           (*(volatile uint32_t *)(SCG_BASE + 0x18u))
#define SCG_CCR            (*(volatile uint32_t *)(SCG_BASE + 0x1Cu))
#define SCG_FIRCCSR        (*(volatile uint32_t *)(SCG_BASE + 0x30u))
#define SCG_FIRCDIV        (*(volatile uint32_t *)(SCG_BASE + 0x34u))
#define SCG_SOSCCSR        (*(volatile uint32_t *)(SCG_BASE + 0x20u))
/* SCG_CSR 位域 */
#define SCG_CSR_SCS_FIRC   (1u)      /* SCS=01: 系统时钟源 = FIRC */
/* SCG_RCCR / VCCR 位域 */
#define SCG_RCCR_DIVCORE(n) ((n) << 24) /* Core 时钟分频: 0=÷1 */
#define SCG_RCCR_DIVBUS(n)  ((n) << 20) /* BUS 时钟分频: 0=÷1 */
#define SCG_RCCR_DIVSLOW(n) ((n) << 16) /* SLOW 时钟分频: 2=÷4 */
/* SCG_FIRCCSR 位域 */
#define SCG_FIRCCSR_FIRCEN  (1u << 30)  /* FIRC 使能 */
#define SCG_FIRCCSR_FIRCREGOFF (1u << 29) /* FIRC 稳压器关闭 */
#define SCG_FIRCCSR_LK      (1u << 23)  /* 锁定位 */
#define SCG_FIRCCSR_CKSEL   (1u << 0)   /* FIRC 时钟选择 */

/**
 * @brief SCG 系统时钟初始化
 *        配置 FIRC 48MHz 作为系统时钟源，设置总线时钟分频器
 *
 * @note S32K144 上电后默认使用 FIRC 48MHz（RM §39.5.1），
 *       但 SCG_RCCR 和 SCG_VCCR 中的时钟分频器需要明确配置
 *       以确保 Core/BUS/SLOW 时钟按需求分配。
 *
 *       标准配置（对应 NXP SDK CLOCK_SYS_Init）：
 *       - 时钟源: FIRC 48MHz
 *       - DIVCORE = 0 (÷1) → Core clock = 48MHz
 *       - DIVBUS  = 0 (÷1) → BUS clock  = 48MHz
 *       - DIVSLOW = 2 (÷4) → Slow clock = 12MHz
 */
static void init_scg(void)
{
    uint32_t reg_val;

    /* 1. 确认 FIRC 已使能且稳定 */
    if ((SCG_FIRCCSR & SCG_FIRCCSR_FIRCEN) == 0u)
    {
        /* 使能 FIRC */
        SCG_FIRCCSR |= SCG_FIRCCSR_FIRCEN;
        /* 等待 FIRC 稳定（RM §39.5.6.1: FIRC Valid 位需要时间）*/
        while ((SCG_FIRCCSR & (1u << 1)) == 0u)
        {
            /* 等待 FIRC 输出有效 */
        }
    }

    /* 2. 配置 RCCR（Reset Control Register）：复位后的时钟配置 */
    reg_val = (uint32_t)SCG_CSR_SCS_FIRC                      /* SCS=01: 使用 FIRC       */
             | SCG_RCCR_DIVCORE(0u)                            /* DIVCORE=0: ÷1            */
             | SCG_RCCR_DIVBUS(0u)                             /* DIVBUS=0: ÷1             */
             | SCG_RCCR_DIVSLOW(2u);                           /* DIVSLOW=2: ÷4            */
    SCG_RCCR = reg_val;

    /* 3. 配置 VCCR（Valid Control Register）：正常工作时钟配置 */
    SCG_VCCR = reg_val;

    /* 4. 等待时钟源切换完成 */
    while ((SCG_CSR & 0x0Fu) != SCG_CSR_SCS_FIRC)
    {
        /* 等待 SCS 确认切换为 FIRC */
    }
}

/* ========== WDOG 寄存器定义 ========== */
/* S32K1xx RM §42.5: WDOG memory map @ 0x40052000 */
#define WDOG_BASE      (0x40052000u)
#define WDOG_CS        (*(volatile uint32_t *)(WDOG_BASE + 0x00u))
#define WDOG_CNT       (*(volatile uint32_t *)(WDOG_BASE + 0x04u))
/* WDOG_CS 位域定义 */
#define WDOG_CS_UPDATE  (1u << 31)   /* 更新使能 */
#define WDOG_CS_UNLOCK  (0x55u << 24)/* 解锁码 */
#define WDOG_CS_EN      (1u << 7)    /* 模块使能 */
#define WDOG_CS_CLK     (1u << 0)    /* 时钟源选择 */

/**
 * @brief WDOG 看门狗禁用
 *        S32K144 上电后 WDOG 默认启用，超时约 512ms。
 *        必须在启动流程的第一步禁用，否则 MCU 会在 .data/.bss
 *        初始化或进入 main() 前被看门狗复位。
 *
 * @note 解锁序列（RM §42.5.2）：
 *       1. 向 CNT 写 0xD928C520（输入解锁密钥 1）
 *       2. 向 CNT 写 0x4A2B（输入解锁密钥 2，仅低 16 位有效）
 *       3. 向 CS 写 0x00002140（EN=0, UPDATE=1, UNLOCK=0x55, CLK=0）
 *
 *       使用纯 C 语言 + 寄存器地址宏实现，不依赖内联汇编。
 *       原因：之前使用 __attribute__((naked)) + 内联汇编时，
 *       "ldr r0, =0x..." 伪指令依赖编译器文字池（literal pool），
 *       在 naked 函数中文字池可能无法正确生成，导致看门狗
 *       实际未被禁用，芯片约 512ms 后被 WDOG 复位。
 */
static void disable_wdog(void)
{
    /* 解锁密钥 1：向 CNT 写 0xD928C520 */
    WDOG_CNT = 0xD928C520u;
    /* 解锁密钥 2：向 CNT 写 0x4A2B（仅低 16 位有效）*/
    WDOG_CNT = 0x4A2Bu;
    /* 配置 CS：EN=0, UPDATE=1, UNLOCK=0x55, CLK=0 */
    WDOG_CS  = 0x00002140u;
}

/**
 * @brief 复位处理函数（上电/复位后第一条指令）
 *        1. 禁用 WDOG 看门狗（否则约 512ms 后复位）
 *        2. 复制 .data 段从 Flash 到 SRAM
 *        3. 清零 .bss 段
 *        4. 调用 main()
 *
 * @note 必须放在 .text.Reset_Handler 段，确保不被 gc-sections 丢弃
 *        WDOG 禁用必须在最前面，不依赖任何变量初始化
 */
__attribute__((section(".text.Reset_Handler")))
void Reset_Handler(void)
{
    uint32_t *pSrc;
    uint32_t *pDest;
    uint32_t  len;

    /* 第 0 步：立刻禁用 WDOG，防止看门狗在后续初始化中复位 */
    disable_wdog();

    /* 第 1 步：初始化 SCG 系统时钟
     * 配置 FIRC 48MHz，设置 BUS/SLOW 时钟分频器
     * 必须在操作任何外设寄存器前完成 */
    init_scg();

    /* 复制 .data 段：从 Flash(_sidata) 到 SRAM(_sdata ~ _edata) */
    pSrc  = &_sidata;
    pDest = &_sdata;
    len   = (uint32_t)&_edata - (uint32_t)&_sdata;
    for (uint32_t i = 0u; i < (len / sizeof(uint32_t)); i++)
    {
        pDest[i] = pSrc[i];
    }

    /* 清零 .bss 段：_sbss ~ _ebss */
    pDest = &_sbss;
    len   = (uint32_t)&_ebss - (uint32_t)&_sbss;
    for (uint32_t i = 0u; i < (len / sizeof(uint32_t)); i++)
    {
        pDest[i] = 0u;
    }

    /* 跳转到 main */
    main();

    /* main 不应返回，若返回则死循环 */
    while (1u)
    {
        /* 空 */
    }
}

/* ========== 默认异常处理 ========== */

/**
 * @brief 默认异常处理函数
 *        所有未实现的中断都会跳转到此函数（弱符号）
 *        进入死循环，便于调试时定位未处理的异常
 */
static void Default_Handler(void)
{
    while (1u)
    {
        /* 空循环，等待调试器介入 */
    }
}
