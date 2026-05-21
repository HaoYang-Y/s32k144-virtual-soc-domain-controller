# GPIO 模块 — 对应书籍第4章

## 学习目标

- [ ] 理解 MCU 启动流程（Reset_Handler → main）
- [ ] 掌握 GPIO 寄存器结构（PDOR/PSOR/PCOR/PTOR/PDIR/PDDR）
- [ ] 掌握 PORT 引脚控制寄存器（PCR MUX/上拉配置）
- [ ] 实现 GPIO 驱动构件封装
- [ ] 完成 LED 闪烁实验

## 书籍对应页码

| 小节 | 内容 | 页数 |
|------|------|------|
| §4.1 | I/O 接口基本概念 | p39~41 |
| §4.2 | 端口控制与 GPIO 编程结构 | p41~46 |
| §4.3 | GPIO 驱动构件封装 | p46~60 |
| §4.4 | LED 闪烁实验 | p60~65 |
| §4.5 | 工程框架与启动流程 | p65~75 |

## 要实现的函数清单

在 `include/gpio_driver.h` 和 `src/gpio_driver.c` 中实现：

1. `gpio_init()` — 引脚初始化（时钟使能 + PCR 配置 + 方向配置）
2. `gpio_write()` — 输出高/低电平（PSOR/PCOR）
3. `gpio_read()` — 读取输入电平（PDIR）
4. `gpio_toggle()` — 翻转输出（PTOR）
5. `gpio_delay_ms()` — 自旋延迟

然后在 `src/main.c` 中补全：
1. 中断向量表 + Reset_Handler 启动代码
2. `board_init()` — 初始化 LED 引脚
3. `main()` — LED 闪烁循环

## 编译运行

```bash
cd mcu/gpio
make all        # 编译
make flash      # 烧录（需连接 J-Link）
```

## 验证标准

- [ ] `make all` 编译无报错
- [ ] 烧录后板载 LED 以 500ms 周期闪烁
