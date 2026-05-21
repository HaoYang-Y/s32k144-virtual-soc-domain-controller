# UART 模块 — 对应书籍第6章

## 学习目标

- [ ] 理解 UART 异步串行通信协议
- [ ] 掌握 LPUART 寄存器结构（BAUD/STAT/CTRL/DATA）
- [ ] 掌握波特率计算公式
- [ ] 实现 UART 驱动构件封装
- [ ] 完成串口回显实验

## 书籍对应页码

| 小节 | 内容 | 页数 |
|------|------|------|
| §6.1 | 异步串行通信基础知识 | p103~107 |
| §6.2 | LPUART 模块编程结构 | p107~120 |
| §6.3 | UART 驱动构件封装 | p120~126 |
| §6.4 | UART 应用实验 | p126~132 |

## 要实现的函数清单

在 `include/uart_driver.h` 和 `src/uart_driver.c` 中实现：

1. `uart_init()` — 初始化（时钟 + 波特率 + 数据格式 + 使能收发器）
2. `uart_putchar()` — 阻塞发送单个字符
3. `uart_getchar()` — 阻塞接收单个字符
4. `uart_puts()` — 发送字符串

然后在 `src/main.c` 中实现串口回显。

## 编译运行

```bash
cd mcu/uart
make all        # 编译
make flash      # 烧录
```

PC 端用 `screen` 或 `minicom` 连接串口：

```bash
screen /dev/ttyACM0 115200
```

## 验证标准

- [ ] `make all` 编译无报错
- [ ] 烧录后串口打印 "UART Demo Ready"
- [ ] PC 端输入的字符能被 MCU 回显
