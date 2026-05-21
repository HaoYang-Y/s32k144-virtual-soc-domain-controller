# Timer 模块 — 对应书籍第7章

## 学习目标

- [ ] 理解 SysTick 内核定时器工作原理
- [ ] 掌握 SysTick 寄存器配置（CSR/RVR/CVR）
- [ ] 掌握 SysTick 中断处理流程
- [ ] 实现基于 SysTick 的精确延时
- [ ] (进阶) 理解 LPIT 外设定时器

## 书籍对应页码

| 小节 | 内容 | 页数 |
|------|------|------|
| §7.1 | 定时器基本概念 | p133~136 |
| §7.2 | 系统时钟 SysTick | p136~139 |
| §7.3 | SysTick 驱动构件封装 | p140~150 |
| §7.4 | LPIT 定时器 | p151~160 |

## 要实现的函数清单

在 `include/timer_driver.h` 和 `src/timer_driver.c` 中实现：

1. `systick_init()` — 配置 SysTick 以 1ms 周期产生中断
2. `systick_delay_ms()` — 基于 SysTick 的精确延时
3. `systick_get_tick()` — 获取系统心跳
4. `SysTick_Handler()` — SysTick 中断服务函数

(进阶) LPIT 部分：
5. `lpit_init()` — 配置 LPIT 通道周期
6. `lpit_start()` — 启动定时器
7. `lpit_is_timeout()` — 检查超时

## 编译运行

```bash
cd mcu/timer
make all        # 编译
make flash      # 烧录
```

## 验证标准

- [ ] `make all` 编译无报错
- [ ] LED 以精确的 500ms 周期闪烁（比手动循环更准确）
