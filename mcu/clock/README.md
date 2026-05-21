# Clock 模块 — 对应书籍第5章

## 学习目标

- [ ] 理解 S32K144 时钟树结构（SOSC/SIRC/FIRC/SPLL）
- [ ] 掌握 SCG 寄存器配置（CSR/SOSCCSR/SPLLCSR）
- [ ] 实现外部晶振 (SOSC) 初始化
- [ ] 实现 SPLL 锁相环配置（160MHz）
- [ ] 掌握系统时钟源切换流程
- [ ] 掌握外设时钟分频配置

## 书籍对应页码

| 小节 | 内容 | 页数 |
|------|------|------|
| §5.1 | 时钟系统基础 | p87~92 |
| §5.2 | 时钟系统编程结构 | p92~105 |
| §5.3 | 时钟驱动构件封装 | p105~115 |
| §5.4 | 时钟应用实验 | p115~120 |

## 要实现的函数清单

在 `include/clock_driver.h` 和 `src/clock_driver.c` 中实现：

1. `clock_init_sosc()` — 初始化外部 8MHz 晶振
2. `clock_init_spll()` — 初始化 SPLL 到 160MHz
3. `clock_set_sysclk()` — 选择系统时钟源
4. `clock_get_freq()` — 获取当前系统时钟频率
5. `clock_set_periph_div()` — 配置外设时钟分频

## 编译运行

```bash
cd mcu/clock
make all        # 编译
make flash      # 烧录
```

## 验证标准

- [ ] `make all` 编译无报错
- [ ] 串口打印正确显示各时钟源切换后的频率值
