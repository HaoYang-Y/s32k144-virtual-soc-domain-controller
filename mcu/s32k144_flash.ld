/*
 * S32K144 链接脚本 — 最小实现
 * 内存布局:
 *   Flash:  0x00000000 - 0x0007FFFF (512KB)
 *   SRAM:   0x1FFF8000 - 0x20003FFF  (48KB)
 *   SRAM_L: 0x1FFF8000 - 0x1FFFFFFF (32KB)
 *   SRAM_U: 0x20000000 - 0x20003FFF (16KB)
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 512K
    SRAM  (rwx) : ORIGIN = 0x1FFF8000, LENGTH = 48K
}

/* 栈大小 */
__stack_size = 0x0800; /* 2KB */

SECTIONS
{
    /* 中断向量表 — 位于 Flash 起始 */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* 代码段 */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.glue_7)
        *(.glue_7t)
        *(.gnu.linkonce.t.*)
        . = ALIGN(4);
    } > FLASH

    /* 只读数据 */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
        . = ALIGN(4);
    } > FLASH

    /* 初始化全局变量 (.data) */
    . = ALIGN(4);
    _sidata = .;
    .data : AT(_sidata)
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM

    /* 未初始化全局变量 (.bss) */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > SRAM

    /* 栈 */
    .stack :
    {
        . = ALIGN(8);
        . = . + __stack_size;
        _estack = .;
    } > SRAM

    /* 堆 (可选) */
    .heap :
    {
        . = ALIGN(8);
        . = . + 0x0400; /* 1KB 堆 */
        _eheap = .;
    } > SRAM

    /* 丢弃不需要的段 */
    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.attributes*)
        *(.comment)
        *(.debug*)
    }
}
