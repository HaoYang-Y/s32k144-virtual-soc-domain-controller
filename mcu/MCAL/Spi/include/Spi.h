/**
 * @file    Spi.h
 * @brief   AUTOSAR MCAL Spi 驱动头文件
 *
 * @note    对应 AUTOSAR CP SWS_Spi (SPI Handler/Driver) 规范
 *          硬件: S32K144 LPSPI1 Slave 模式 (MCU 为从机, SOC/CH347T 为主机)
 *          底层: SDK lpspi_slave_driver (中断驱动, ISR 喂 FIFO + 信号量同步)
 *          本文件不暴露任何 SDK 类型, 上层只依赖 AUTOSAR 类型
 *
 *          Slave 引脚 (ALT3):
 *            PTB14 = LPSPI1_SCK
 *            PTB15 = LPSPI1_SIN  ← CH347T MO
 *            PTB16 = LPSPI1_SOUT → CH347T MI
 *            PTB17 = LPSPI1_PCS3 ← CH347T CS0
 */

#ifndef MCAL_SPI_H
#define MCAL_SPI_H

#include "Std_Types.h"
#include "Platform_Types.h"   /* boolean / NULL_PTR */

/* ===================================================================
 *  AUTOSAR 标准类型 (SWS_Spi)
 * =================================================================== */

/** @brief SPI 通道 ID */
typedef uint8  Spi_ChannelType;

/** @brief SPI 作业 ID */
typedef uint8  Spi_JobType;

/** @brief SPI 序列 ID */
typedef uint8  Spi_SequenceType;

/** @brief SPI 硬件单元 ID */
typedef uint8  Spi_HWUnitType;

/** @brief SPI 数据缓冲元素类型 (8bit 帧下为字节) */
typedef uint8  Spi_DataBufferType;

/** @brief SPI 数据数量类型 */
typedef uint16 Spi_NumberOfDataType;

/** @brief 最大通道数类型 */
typedef uint8  Spi_MaxChannelType;

/** @brief 最大作业数类型 */
typedef uint8  Spi_MaxJobType;

/** @brief 最大序列数类型 */
typedef uint8  Spi_MaxSequenceType;

/** @brief 序列内作业数类型 */
typedef uint8  Spi_NumberOfJobsType;

/** @brief SPI 通道类型 (IB/PB/EB) */
typedef enum {
    SPI_IB = 0U,   /* Internal Buffer: 驱动内部缓冲 */
    SPI_PB = 1U,   /* Provided Buffer: 上层提供缓冲 */
    SPI_EB = 2U    /* External Buffer: 与通道绑定的外部缓冲 */
} Spi_ChannelClassType;

/** @brief SPI 模块状态 */
typedef enum {
    SPI_UNINIT = 0U,   /* 未初始化 */
    SPI_IDLE   = 1U,   /* 已初始化, 空闲 */
    SPI_BUSY   = 2U    /* 传输进行中 */
} Spi_StatusType;

/** @brief SPI 作业结果 */
typedef enum {
    SPI_JOB_OK      = 0U,   /* 作业成功完成 */
    SPI_JOB_PENDING = 1U,   /* 作业排队等待/进行中 */
    SPI_JOB_FAILED  = 2U,   /* 作业失败 */
    SPI_JOB_QUEUED  = 3U    /* 作业已入队 (本工程单序列, 暂不使用) */
} Spi_JobResultType;

/** @brief SPI 序列结果 */
typedef enum {
    SPI_SEQ_OK       = 0U,   /* 序列成功完成 */
    SPI_SEQ_PENDING  = 1U,   /* 序列进行中 */
    SPI_SEQ_FAILED   = 2U,   /* 序列失败 */
    SPI_SEQ_CANCELED = 3U    /* 序列被取消 */
} Spi_SeqResultType;

/** @brief SPI 异步机制模式 */
typedef enum {
    SPI_POLLING_MODE    = 0U,   /* 同步模式 */
    SPI_INTERRUPT_MODE  = 1U    /* 中断(异步)模式 */
} Spi_AsyncModeType;

/** @brief SPI 数据宽度 (bit/帧) */
typedef enum {
    SPI_DATAWIDTH_UNCONFIGURED = 0U,
    SPI_DATAWIDTH_8BIT         = 8U,
    SPI_DATAWIDTH_16BIT        = 16U,
    SPI_DATAWIDTH_32BIT        = 32U
} Spi_DataWidthType;

/** @brief SPI 位传输顺序 */
typedef enum {
    SPI_MSB_FIRST = 0U,   /* 高位先行 */
    SPI_LSB_FIRST = 1U    /* 低位先行 */
} Spi_TransferStartType;

/** @brief SPI 时钟模式 (CPOL/CPHA 组合) — 平台扩展 */
typedef enum {
    SPI_MODE_0 = 0U,   /* CPOL=0, CPHA=0 */
    SPI_MODE_1 = 1U,   /* CPOL=0, CPHA=1 */
    SPI_MODE_2 = 2U,   /* CPOL=1, CPHA=0 */
    SPI_MODE_3 = 3U    /* CPOL=1, CPHA=1 */
} Spi_ModeType;

/** @brief 作业结束通知回调 (无参) */
typedef void (*Spi_JobEndNotificationType)(void);

/** @brief 序列结束通知回调 (无参) */
typedef void (*Spi_SequenceEndNotificationType)(void);

/* ===================================================================
 *  配置结构 (SWS_Spi; 字段带 ★ 的为工程扩展)
 * =================================================================== */

/** @brief SPI 通道配置 */
typedef struct {
    Spi_ChannelType       SpiChannelId;     /* 通道 ID */
    Spi_ChannelClassType  SpiChannelType;   /* 通道类型: SPI_IB/PB/EB */
    Spi_DataWidthType     SpiDataWidth;     /* 数据宽度 (bit/帧) */
    Spi_TransferStartType SpiTransferStart; /* 位顺序: MSB/LSB first */
    Spi_NumberOfDataType  SpiLength;        /* 传输数据长度 (字节) */
    Spi_NumberOfDataType  SpiIbNBuffers;    /* IB 缓冲个数 (全双工=2) */
    Spi_DataBufferType * const *SpiIbNBufPtr; /* IB 缓冲指针数组 {TX, RX} */
    /* --- ★ 工程扩展: 硬件参数 (完整 AUTOSAR 中属 ExternalDevice/HWUnit 配置) --- */
    Spi_ModeType          SpiMode;          /* ★ 时钟模式 (CPOL/CPHA) */
    uint8                 WhichPcs;         /* ★ 片选引脚索引 (0-3 → PCS0-3) */
    uint8                 DefaultData;      /* ★ WriteIB(NULL) 时的默认发送字节 */
} Spi_ChannelConfigType;

/** @brief SPI 作业配置 */
typedef struct {
    Spi_JobType     SpiJobId;            /* 作业 ID */
    Spi_JobType     SpiJobPriority;      /* 优先级 (单作业场景无竞争) */
    Spi_ChannelType SpiDeviceAssignment; /* 作业绑定的通道 ID */
} Spi_JobConfigType;

/** @brief SPI 序列配置 */
typedef struct {
    Spi_SequenceType      SpiSequenceId;            /* 序列 ID */
    Spi_AsyncModeType     SpiTransferMode;          /* 传输机制: 中断/轮询 */
    Spi_NumberOfJobsType  SpiNumberOfJobs;          /* 序列内作业数 */
    const Spi_JobType    *SpiJobList;               /* 作业 ID 列表 */
    boolean               SpiInterruptibleSequence; /* 是否可中断 (本场景 FALSE) */
    uint32                TimeoutMs;                /* ★ 传输超时 (ms), AUTOSAR 无此字段 */
} Spi_SequenceConfigType;

/** @brief SPI 配置容器 */
typedef struct {
    Spi_MaxChannelType          SpiMaxChannel;          /* 最大通道 ID */
    Spi_MaxJobType              SpiMaxJob;              /* 最大作业 ID */
    Spi_MaxSequenceType         SpiMaxSequence;         /* 最大序列 ID */
    const Spi_ChannelConfigType  *SpiChannelConfigData; /* 通道配置数组 */
    const Spi_JobConfigType      *SpiJobConfigData;     /* 作业配置数组 */
    const Spi_SequenceConfigType *SpiSequenceConfigData;/* 序列配置数组 */
    Spi_JobEndNotificationType      SpiJobEndNotification;      /* 作业结束通知 */
    Spi_SequenceEndNotificationType SpiSequenceEndNotification; /* 序列结束通知 */
} Spi_ConfigType;

/* ===================================================================
 *  API 声明 (SWS_Spi)
 * =================================================================== */

/**
 * @brief   初始化 SPI 驱动
 * @param   ConfigPtr 配置指针; NULL 时使用预编译默认配置 (Spi_Config)
 * @return  无 (void); 失败时保持 SPI_UNINIT
 */
void Spi_Init(const Spi_ConfigType *ConfigPtr);

/**
 * @brief   反初始化 SPI 驱动, 关闭硬件单元
 * @return  E_OK: 反初始化成功; E_NOT_OK: 未初始化或失败
 */
Std_ReturnType Spi_DeInit(void);

/**
 * @brief   将数据写入指定 IB 通道的内部发送缓冲
 * @param   Channel      通道 ID
 * @param   DataBufferPtr 源数据指针; NULL 时用通道 DefaultData 填充
 * @return  E_OK: 写入成功; E_NOT_OK: 未初始化/通道非法/非 IB 通道
 */
Std_ReturnType Spi_WriteIB(Spi_ChannelType Channel,
                           const Spi_DataBufferType *DataBufferPtr);

/**
 * @brief   从指定 IB 通道的内部接收缓冲读取数据
 * @param   Channel           通道 ID
 * @param   DataBufferPointer 目的数据指针 (写入方向)
 * @return  E_OK: 读取成功; E_NOT_OK: 未初始化/通道非法/非 IB 通道
 */
Std_ReturnType Spi_ReadIB(Spi_ChannelType Channel,
                          Spi_DataBufferType *DataBufferPointer);

/**
 * @brief   同步发送指定序列 (阻塞直到序列完成或超时)
 * @param   Sequence 序列 ID
 * @return  E_OK: 序列全部作业成功; E_NOT_OK: 拒绝请求或任一作业失败
 *
 * @note    Slave 模式每次传输均为全双工交换 (主时钟双向移位),
 *          结果经 Spi_GetSequenceResult 查询
 */
Std_ReturnType Spi_SyncTransmit(Spi_SequenceType Sequence);

/**
 * @brief   异步发送指定序列 (立即返回, 由 Spi_MainFunction_Handling 收尾)
 * @param   Sequence 序列 ID
 * @return  E_OK: 请求接受并已启动; E_NOT_OK: 拒绝请求
 *
 * @note    仅 SPI_INTERRUPT_MODE 下可用; 完成时触发通知回调
 */
Std_ReturnType Spi_AsyncTransmit(Spi_SequenceType Sequence);

/**
 * @brief   查询 SPI 模块状态
 * @return  模块状态: SPI_UNINIT / SPI_IDLE / SPI_BUSY
 */
Spi_StatusType Spi_GetStatus(void);

/**
 * @brief   查询指定硬件单元状态
 * @param   HWUnit 硬件单元 ID
 * @return  SPI_UNINIT: 未初始化或 HWUnit 非法; SPI_BUSY: 传输中; SPI_IDLE: 空闲
 */
Spi_StatusType Spi_GetHWUnitStatus(Spi_HWUnitType HWUnit);

/**
 * @brief   查询指定作业结果
 * @param   Job 作业 ID
 * @return  作业结果; Job 越界时返回 SPI_JOB_FAILED
 */
Spi_JobResultType Spi_GetJobResult(Spi_JobType Job);

/**
 * @brief   查询指定序列结果
 * @param   Sequence 序列 ID
 * @return  序列结果; Sequence 越界时返回 SPI_SEQ_FAILED
 */
Spi_SeqResultType Spi_GetSequenceResult(Spi_SequenceType Sequence);

/**
 * @brief   设置异步机制模式
 * @param   Mode SPI_POLLING_MODE / SPI_INTERRUPT_MODE
 * @return  E_OK: 设置成功; E_NOT_OK: 异步传输进行中或模式非法
 */
Std_ReturnType Spi_SetAsyncMode(Spi_AsyncModeType Mode);

/**
 * @brief   取消指定序列的异步传输
 * @param   Sequence 序列 ID
 * @return  无 (void); 无进行中的序列时为 no-op
 */
void Spi_Cancel(Spi_SequenceType Sequence);

/**
 * @brief   SPI 驱动周期处理函数 (异步传输收尾: 完成/错误/超时三态)
 * @return  无 (void); 无异步传输时立即返回
 */
void Spi_MainFunction_Handling(void);

/**
 * @brief   获取 SPI 模块版本信息
 * @param   VersionInfoPtr 版本信息输出指针; NULL 时直接返回
 * @return  无 (void)
 */
void Spi_GetVersionInfo(Std_VersionInfoType *VersionInfoPtr);

#endif /* MCAL_SPI_H */
