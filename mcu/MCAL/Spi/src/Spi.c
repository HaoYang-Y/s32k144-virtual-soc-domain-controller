/**
 * @file    Spi.c
 * @brief   AUTOSAR MCAL Spi 驱动实现 — S32K144 LPSPI1 Slave 模式
 *
 * @note    MCAL = SDK 薄封装: 底层复用 SDK lpspi_slave_driver 中断路径
 *          (ISR 喂 FIFO + OSIF 信号量同步), 本模块只做 AUTOSAR
 *          Channel/Job/Sequence 语义与状态机
 *
 *          ISR 链 (SDK 提供, 本模块不自写 ISR):
 *            LPSPI1_IRQHandler → LPSPI_DRV_IRQHandler(1U)
 *            → 按 CFGR1.MASTER 位分发 → LPSPI_DRV_SlaveIRQHandler
 *
 *          Slave 引脚 (ALT3):
 *            PTB14 = LPSPI1_SCK
 *            PTB15 = LPSPI1_SIN  ← CH347T MO
 *            PTB16 = LPSPI1_SOUT → CH347T MI
 *            PTB17 = LPSPI1_PCS3 ← CH347T CS0
 *
 *          ⚠ 协议约束: SDK 从机完成判定是纯软件字节计数
 *            (txCount/rxCount 归零), 主机必须恰好发 SpiLength 字节,
 *            发不足 → 阻塞超时/异步永不完成; 发多了 → TEF 错误
 */

#include "Spi.h"
#include "Spi_Cfg.h"
#include "lpspi_slave_driver.h"
#include "lpspi_shared_function.h"
#include <string.h>

/* ===================================================================
 *  模块内部状态
 * =================================================================== */

/** 当前配置指针 (UNINIT 时为 NULL) */
static const Spi_ConfigType *Spi_ConfigPtr = NULL_PTR;

/** 模块状态 */
static Spi_StatusType        Spi_Status    = SPI_UNINIT;

/** 各作业结果 (索引 = JobId) */
static Spi_JobResultType     Spi_JobResults[SPI_JOB_COUNT];

/** 各序列结果 (索引 = SequenceId) */
static Spi_SeqResultType     Spi_SequenceResults[SPI_SEQUENCE_COUNT];

/** 当前异步机制模式 */
static Spi_AsyncModeType     Spi_AsyncMode = SPI_POLLING_MODE;

/** 进行中的异步序列 ID */
static Spi_SequenceType      Spi_ActiveSequence = 0U;

/** 是否有异步传输进行中 */
static bool                  Spi_AsyncInProgress = false;

/** 异步传输超时计数 (MainFunction 调用次数, 近似 1ms/次) */
static uint32_t              Spi_AsyncTimeoutCnt = 0U;

/** SDK 从机状态句柄 (本模块私有, 不暴露给上层) */
static lpspi_state_t         Spi_SlaveState;

/* ===================================================================
 *  内部辅助函数
 * =================================================================== */

/**
 * @brief   按 ID 查通道配置
 * @param   ChannelId 通道 ID
 * @return  配置指针; 越界时返回 NULL
 */
static const Spi_ChannelConfigType *Spi_GetChannel(Spi_ChannelType ChannelId)
{
    if ((Spi_ConfigPtr == NULL_PTR) || (ChannelId > Spi_ConfigPtr->SpiMaxChannel)) {
        return NULL_PTR;
    }
    return &Spi_ConfigPtr->SpiChannelConfigData[ChannelId];
}

/**
 * @brief   按 ID 查作业配置
 * @param   JobId 作业 ID
 * @return  配置指针; 越界时返回 NULL
 */
static const Spi_JobConfigType *Spi_GetJob(Spi_JobType JobId)
{
    if ((Spi_ConfigPtr == NULL_PTR) || (JobId > Spi_ConfigPtr->SpiMaxJob)) {
        return NULL_PTR;
    }
    return &Spi_ConfigPtr->SpiJobConfigData[JobId];
}

/**
 * @brief   按 ID 查序列配置
 * @param   SequenceId 序列 ID
 * @return  配置指针; 越界时返回 NULL
 */
static const Spi_SequenceConfigType *Spi_GetSequence(Spi_SequenceType SequenceId)
{
    if ((Spi_ConfigPtr == NULL_PTR) || (SequenceId > Spi_ConfigPtr->SpiMaxSequence)) {
        return NULL_PTR;
    }
    return &Spi_ConfigPtr->SpiSequenceConfigData[SequenceId];
}

/**
 * @brief   将通道配置映射为 SDK 从机配置
 * @param   Ch 通道配置指针
 * @param   SdkCfg SDK 配置输出指针
 * @return  无
 */
static void Spi_MapChannelToSdk(const Spi_ChannelConfigType *Ch,
                                lpspi_slave_config_t *SdkCfg)
{
    LPSPI_DRV_SlaveGetDefaultConfig(SdkCfg);

    /* 片选引脚: WhichPcs (0-3) → LPSPI_PCS0-3, 非法值回退 PCS3 */
    SdkCfg->whichPcs = (Ch->WhichPcs == 0U) ? LPSPI_PCS0 :
                       (Ch->WhichPcs == 1U) ? LPSPI_PCS1 :
                       (Ch->WhichPcs == 2U) ? LPSPI_PCS2 : LPSPI_PCS3;

    /* CPOL: MODE_2/3 (CPOL=1) → SCK 空闲为高 */
    SdkCfg->clkPolarity = ((Ch->SpiMode == SPI_MODE_2) || (Ch->SpiMode == SPI_MODE_3))
                          ? LPSPI_SCK_ACTIVE_LOW : LPSPI_SCK_ACTIVE_HIGH;

    /* CPHA: MODE_1/3 (CPHA=1) → 第 2 个边沿采样 */
    SdkCfg->clkPhase = ((Ch->SpiMode == SPI_MODE_1) || (Ch->SpiMode == SPI_MODE_3))
                       ? LPSPI_CLOCK_PHASE_2ND_EDGE : LPSPI_CLOCK_PHASE_1ST_EDGE;

    SdkCfg->pcsPolarity  = LPSPI_ACTIVE_LOW;
    SdkCfg->bitcount     = (uint16_t)Ch->SpiDataWidth;
    SdkCfg->lsbFirst     = (Ch->SpiTransferStart == SPI_LSB_FIRST);
    SdkCfg->transferType = LPSPI_USING_INTERRUPTS;
    SdkCfg->callback     = NULL_PTR;   /* 收尾由本模块轮询, 不用 SDK 回调 */
    SdkCfg->callbackParam = NULL_PTR;
}

/**
 * @brief   异步传输统一收尾 (置结果 → 通知 → 恢复 IDLE)
 * @param   Result 作业结果 (SPI_JOB_OK / SPI_JOB_FAILED)
 * @return  无
 * @note    通知回调必须在状态恢复 IDLE 之前调用
 */
static void Spi_FinalizeAsync(Spi_JobResultType Result)
{
    const Spi_SequenceConfigType *seq = Spi_GetSequence(Spi_ActiveSequence);
    uint8 i;

    if (seq != NULL_PTR) {
        for (i = 0U; i < seq->SpiNumberOfJobs; i++) {
            Spi_JobResults[seq->SpiJobList[i]] = Result;
        }
    }
    Spi_SequenceResults[Spi_ActiveSequence] =
        (Result == SPI_JOB_OK) ? SPI_SEQ_OK : SPI_SEQ_FAILED;

    if (Spi_ConfigPtr->SpiJobEndNotification != NULL_PTR) {
        Spi_ConfigPtr->SpiJobEndNotification();
    }
    if (Spi_ConfigPtr->SpiSequenceEndNotification != NULL_PTR) {
        Spi_ConfigPtr->SpiSequenceEndNotification();
    }

    Spi_AsyncInProgress = false;
    Spi_Status          = SPI_IDLE;
}

/* ===================================================================
 *  API 实现 (SWS_Spi)
 * =================================================================== */

/**
 * @brief   初始化 SPI 驱动 (LPSPI1 从机 + 中断机制)
 * @param   ConfigPtr 配置指针; NULL 时使用预编译默认配置 Spi_Config
 * @return  无; 失败时保持 SPI_UNINIT, 由 Spi_GetStatus 查询
 */
void Spi_Init(const Spi_ConfigType *ConfigPtr)
{
    const Spi_ChannelConfigType *ch;
    lpspi_slave_config_t          sdk_cfg;
    status_t                      st;

    if (Spi_Status != SPI_UNINIT) {
        return;   /* 重复初始化 no-op */
    }

    if (ConfigPtr == NULL_PTR) {
        ConfigPtr = &Spi_Config;
    }
#if (SPI_DEV_ERROR_DETECT == STD_ON)
    if ((ConfigPtr->SpiChannelConfigData == NULL_PTR) ||
        (ConfigPtr->SpiJobConfigData == NULL_PTR) ||
        (ConfigPtr->SpiSequenceConfigData == NULL_PTR)) {
        return;
    }
#endif
    Spi_ConfigPtr = ConfigPtr;

    /* 本工程单硬件单元: 通道 0 的硬件参数即为 LPSPI1 从机配置 */
    ch = Spi_GetChannel(0U);
    if (ch == NULL_PTR) {
        Spi_ConfigPtr = NULL_PTR;
        return;
    }
    Spi_MapChannelToSdk(ch, &sdk_cfg);

    st = LPSPI_DRV_SlaveInit(SPI_INSTANCE_ID, &Spi_SlaveState, &sdk_cfg);
    if (st != STATUS_SUCCESS) {
        Spi_ConfigPtr = NULL_PTR;
        return;
    }

    /* 结果数组清零 */
    for (uint8 i = 0U; i < SPI_JOB_COUNT; i++) {
        Spi_JobResults[i] = SPI_JOB_OK;
    }
    for (uint8 i = 0U; i < SPI_SEQUENCE_COUNT; i++) {
        Spi_SequenceResults[i] = SPI_SEQ_OK;
    }

    Spi_Status = SPI_IDLE;
}

/**
 * @brief   反初始化 SPI 驱动
 * @return  E_OK: 反初始化成功; E_NOT_OK: 未初始化
 */
Std_ReturnType Spi_DeInit(void)
{
    if (Spi_Status == SPI_UNINIT) {
        return E_NOT_OK;
    }
    if (Spi_AsyncInProgress) {
        /* 先中止传输, 否则 SDK Deinit 内部断言失败 */
        (void)LPSPI_DRV_SlaveAbortTransfer(SPI_INSTANCE_ID);
        Spi_AsyncInProgress = false;
    }
    (void)LPSPI_DRV_SlaveDeinit(SPI_INSTANCE_ID);
    Spi_Status    = SPI_UNINIT;
    Spi_ConfigPtr = NULL_PTR;
    return E_OK;
}

/**
 * @brief   将数据写入 IB 通道的内部发送缓冲
 * @param   Channel      通道 ID
 * @param   DataBufferPtr 源数据; NULL 时用通道 DefaultData 填充
 * @return  E_OK: 成功; E_NOT_OK: 未初始化/通道非法/非 IB 通道
 */
Std_ReturnType Spi_WriteIB(Spi_ChannelType Channel, const Spi_DataBufferType *DataBufferPtr)
{
    const Spi_ChannelConfigType *ch;

    if (Spi_Status == SPI_UNINIT) {
        return E_NOT_OK;
    }
    ch = Spi_GetChannel(Channel);
    if ((ch == NULL_PTR) || (ch->SpiChannelType != SPI_IB)) {
        return E_NOT_OK;
    }

    if (DataBufferPtr == NULL_PTR) {
        /* SWS_Spi_00023: NULL 时使用通道默认发送值 */
        (void)memset(ch->SpiIbNBufPtr[0], ch->DefaultData, ch->SpiLength);
    } else {
        (void)memcpy(ch->SpiIbNBufPtr[0], DataBufferPtr, ch->SpiLength);
    }
    return E_OK;
}

/**
 * @brief   从 IB 通道的内部接收缓冲读取数据
 * @param   Channel           通道 ID
 * @param   DataBufferPointer 目的数据指针
 * @return  E_OK: 成功; E_NOT_OK: 未初始化/通道非法/非 IB 通道/指针为空
 */
Std_ReturnType Spi_ReadIB(Spi_ChannelType Channel, Spi_DataBufferType *DataBufferPointer)
{
    const Spi_ChannelConfigType *ch;

    if (Spi_Status == SPI_UNINIT) {
        return E_NOT_OK;
    }
    ch = Spi_GetChannel(Channel);
    if ((ch == NULL_PTR) || (ch->SpiChannelType != SPI_IB)) {
        return E_NOT_OK;
    }
    if (DataBufferPointer == NULL_PTR) {
        return E_NOT_OK;
    }

    (void)memcpy(DataBufferPointer, ch->SpiIbNBufPtr[1], ch->SpiLength);
    return E_OK;
}

/**
 * @brief   同步发送序列 (阻塞直到完成/超时, 单序列不支持并发)
 * @param   Sequence 序列 ID
 * @return  E_OK: 序列全部作业成功; E_NOT_OK: 拒绝请求或任一作业失败
 *
 * @note    从机侧每次传输均为全双工交换: TX IB 经 WriteIB 预填,
 *          RX IB 由 SDK ISR 填充, 之后经 Spi_ReadIB 读出
 */
Std_ReturnType Spi_SyncTransmit(Spi_SequenceType Sequence)
{
    const Spi_SequenceConfigType *seq;
    boolean                        seq_ok = TRUE;
    uint8                          i;

    if ((Spi_Status == SPI_UNINIT) || Spi_AsyncInProgress) {
        return E_NOT_OK;
    }
    seq = Spi_GetSequence(Sequence);
    if (seq == NULL_PTR) {
        return E_NOT_OK;
    }

    Spi_Status = SPI_BUSY;
    Spi_SequenceResults[Sequence] = SPI_SEQ_PENDING;

    for (i = 0U; i < seq->SpiNumberOfJobs; i++) {
        const Spi_JobConfigType     *job = Spi_GetJob(seq->SpiJobList[i]);
        const Spi_ChannelConfigType *ch;
        status_t                      st;
        Spi_JobResultType             res;

        if (job == NULL_PTR) {
            seq_ok = FALSE;
            break;
        }
        ch = Spi_GetChannel(job->SpiDeviceAssignment);
        if (ch == NULL_PTR) {
            seq_ok = FALSE;
            break;
        }

        Spi_JobResults[job->SpiJobId] = SPI_JOB_PENDING;
        st = LPSPI_DRV_SlaveTransferBlocking(SPI_INSTANCE_ID,
                                             ch->SpiIbNBufPtr[0], ch->SpiIbNBufPtr[1],
                                             ch->SpiLength, seq->TimeoutMs);
        if (st == STATUS_SUCCESS) {
            /* ⚠ 关键: SDK ISR 在 TEF/REF 帧错误时也会 Post 信号量,
             *        阻塞调用以 SUCCESS 返回, 必须查状态句柄才能发现帧错误 */
            res = (Spi_SlaveState.status == LPSPI_TRANSFER_OK)
                  ? SPI_JOB_OK : SPI_JOB_FAILED;
        } else {
            res = SPI_JOB_FAILED;   /* STATUS_TIMEOUT / STATUS_ERROR / STATUS_BUSY */
        }

        Spi_JobResults[job->SpiJobId] = res;
        if (res != SPI_JOB_OK) {
            seq_ok = FALSE;
        }
        if (Spi_ConfigPtr->SpiJobEndNotification != NULL_PTR) {
            Spi_ConfigPtr->SpiJobEndNotification();
        }
    }

    Spi_SequenceResults[Sequence] = seq_ok ? SPI_SEQ_OK : SPI_SEQ_FAILED;
    if (Spi_ConfigPtr->SpiSequenceEndNotification != NULL_PTR) {
        Spi_ConfigPtr->SpiSequenceEndNotification();
    }
    Spi_Status = SPI_IDLE;

    return seq_ok ? E_OK : E_NOT_OK;
}

/**
 * @brief   异步发送序列 (立即返回, 由 Spi_MainFunction_Handling 收尾)
 * @param   Sequence 序列 ID
 * @return  E_OK: 请求接受并已启动; E_NOT_OK: 拒绝请求
 *
 * @note    仅 SPI_INTERRUPT_MODE 下可用 (SWS 要求);
 *          完成/错误/超时经 Spi_MainFunction_Handling 三态收尾
 */
Std_ReturnType Spi_AsyncTransmit(Spi_SequenceType Sequence)
{
    const Spi_SequenceConfigType *seq;
    const Spi_JobConfigType      *job;
    const Spi_ChannelConfigType  *ch;
    status_t                      st;
    uint8                          i;

    if ((Spi_Status == SPI_UNINIT) || Spi_AsyncInProgress) {
        return E_NOT_OK;
    }
    if (Spi_AsyncMode != SPI_INTERRUPT_MODE) {
        return E_NOT_OK;
    }
    seq = Spi_GetSequence(Sequence);
    if (seq == NULL_PTR) {
        return E_NOT_OK;
    }

    /* 校验全部作业与通道 (单作业场景为 1 个) */
    for (i = 0U; i < seq->SpiNumberOfJobs; i++) {
        job = Spi_GetJob(seq->SpiJobList[i]);
        if ((job == NULL_PTR) || (Spi_GetChannel(job->SpiDeviceAssignment) == NULL_PTR)) {
            return E_NOT_OK;
        }
    }
    job = Spi_GetJob(seq->SpiJobList[0]);
    ch  = Spi_GetChannel(job->SpiDeviceAssignment);

    Spi_Status                = SPI_BUSY;
    Spi_ActiveSequence        = Sequence;
    Spi_AsyncTimeoutCnt       = seq->TimeoutMs;
    Spi_SequenceResults[Sequence] = SPI_SEQ_PENDING;
    for (i = 0U; i < seq->SpiNumberOfJobs; i++) {
        Spi_JobResults[seq->SpiJobList[i]] = SPI_JOB_PENDING;
    }

    st = LPSPI_DRV_SlaveTransfer(SPI_INSTANCE_ID,
                                 ch->SpiIbNBufPtr[0], ch->SpiIbNBufPtr[1],
                                 ch->SpiLength);
    if (st != STATUS_SUCCESS) {
        /* 启动失败: 回滚状态 */
        Spi_Status          = SPI_IDLE;
        Spi_SequenceResults[Sequence] = SPI_SEQ_FAILED;
        return E_NOT_OK;
    }
    Spi_AsyncInProgress = true;
    return E_OK;
}

/**
 * @brief   查询 SPI 模块状态
 * @return  模块状态: SPI_UNINIT / SPI_IDLE / SPI_BUSY
 */
Spi_StatusType Spi_GetStatus(void)
{
    return Spi_Status;
}

/**
 * @brief   查询指定硬件单元状态
 * @param   HWUnit 硬件单元 ID
 * @return  SPI_UNINIT: 未初始化或 HWUnit 非法; SPI_BUSY: 传输中; SPI_IDLE: 空闲
 */
Spi_StatusType Spi_GetHWUnitStatus(Spi_HWUnitType HWUnit)
{
    if ((Spi_Status == SPI_UNINIT) || (HWUnit > 0U)) {
        return SPI_UNINIT;   /* 本工程仅硬件单元 0 (LPSPI1) */
    }
    if (Spi_SlaveState.isTransferInProgress) {
        return SPI_BUSY;
    }
    return Spi_Status;
}

/**
 * @brief   查询指定作业结果
 * @param   Job 作业 ID
 * @return  作业结果; Job 越界时返回 SPI_JOB_FAILED
 */
Spi_JobResultType Spi_GetJobResult(Spi_JobType Job)
{
    if ((Spi_ConfigPtr == NULL_PTR) || (Job > Spi_ConfigPtr->SpiMaxJob)) {
        return SPI_JOB_FAILED;
    }
    return Spi_JobResults[Job];
}

/**
 * @brief   查询指定序列结果
 * @param   Sequence 序列 ID
 * @return  序列结果; Sequence 越界时返回 SPI_SEQ_FAILED
 */
Spi_SeqResultType Spi_GetSequenceResult(Spi_SequenceType Sequence)
{
    if ((Spi_ConfigPtr == NULL_PTR) || (Sequence > Spi_ConfigPtr->SpiMaxSequence)) {
        return SPI_SEQ_FAILED;
    }
    return Spi_SequenceResults[Sequence];
}

/**
 * @brief   设置异步机制模式
 * @param   Mode SPI_POLLING_MODE / SPI_INTERRUPT_MODE
 * @return  E_OK: 设置成功; E_NOT_OK: 模式非法或异步传输进行中
 *
 * @note    学习工程简化: Spi_SyncTransmit 在两种模式下均走 SDK 中断
 *          阻塞路径; Spi_AsyncTransmit 仅在 SPI_INTERRUPT_MODE 下可用
 */
Std_ReturnType Spi_SetAsyncMode(Spi_AsyncModeType Mode)
{
    if ((Mode != SPI_POLLING_MODE) && (Mode != SPI_INTERRUPT_MODE)) {
        return E_NOT_OK;
    }
    if (Spi_AsyncInProgress) {
        return E_NOT_OK;
    }
    Spi_AsyncMode = Mode;
    return E_OK;
}

/**
 * @brief   取消指定序列的异步传输
 * @param   Sequence 序列 ID
 * @return  无; 非当前异步序列或无线程传输时为 no-op
 */
void Spi_Cancel(Spi_SequenceType Sequence)
{
    const Spi_SequenceConfigType *seq;
    uint8                          i;

    if (!Spi_AsyncInProgress || (Sequence != Spi_ActiveSequence)) {
        return;
    }

    (void)LPSPI_DRV_SlaveAbortTransfer(SPI_INSTANCE_ID);

    seq = Spi_GetSequence(Spi_ActiveSequence);
    if (seq != NULL_PTR) {
        for (i = 0U; i < seq->SpiNumberOfJobs; i++) {
            Spi_JobResults[seq->SpiJobList[i]] = SPI_JOB_FAILED;
        }
    }
    Spi_SequenceResults[Spi_ActiveSequence] = SPI_SEQ_CANCELED;
    if (Spi_ConfigPtr->SpiJobEndNotification != NULL_PTR) {
        Spi_ConfigPtr->SpiJobEndNotification();
    }
    if (Spi_ConfigPtr->SpiSequenceEndNotification != NULL_PTR) {
        Spi_ConfigPtr->SpiSequenceEndNotification();
    }
    Spi_AsyncInProgress = false;
    Spi_Status          = SPI_IDLE;
}

/**
 * @brief   SPI 驱动周期处理 (异步传输三态收尾: 完成/错误/超时)
 * @return  无; 无异步传输时立即返回
 *
 * @note    超时计数按 MainFunction 调用次数近似 1ms/次 (主循环周期
 *          约 1ms); 如需精确计时可改用 OSIF SysTick tick 数
 */
void Spi_MainFunction_Handling(void)
{
    uint32_t  bytes_remained = 0U;
    status_t  st;

    if (!Spi_AsyncInProgress) {
        return;
    }

    st = LPSPI_DRV_SlaveGetTransferStatus(SPI_INSTANCE_ID, &bytes_remained);
    if (st == STATUS_SUCCESS) {
        Spi_FinalizeAsync(SPI_JOB_OK);
    } else if (st == STATUS_ERROR) {
        Spi_FinalizeAsync(SPI_JOB_FAILED);
    } else {   /* STATUS_BUSY: 传输进行中 */
        if (Spi_AsyncTimeoutCnt == 0U) {
            (void)LPSPI_DRV_SlaveAbortTransfer(SPI_INSTANCE_ID);
            Spi_FinalizeAsync(SPI_JOB_FAILED);
        } else {
            Spi_AsyncTimeoutCnt--;
        }
    }
}

/**
 * @brief   获取 SPI 模块版本信息
 * @param   VersionInfoPtr 版本信息输出指针; NULL 时直接返回
 * @return  无
 */
void Spi_GetVersionInfo(Std_VersionInfoType *VersionInfoPtr)
{
    if (VersionInfoPtr == NULL_PTR) {
        return;
    }
    VersionInfoPtr->vendorID         = SPI_VENDOR_ID;
    VersionInfoPtr->moduleID         = SPI_MODULE_ID;
    VersionInfoPtr->sw_major_version = SPI_SW_MAJOR_VERSION;
    VersionInfoPtr->sw_minor_version = SPI_SW_MINOR_VERSION;
    VersionInfoPtr->sw_patch_version = SPI_SW_PATCH_VERSION;
}

/* ===================================================================
 *  SDK 依赖桩函数
 * =================================================================== */

/**
 * @brief   LPSPI Master IRQ Handler 桩 — 本工程仅 Slave 模式
 *
 * @note    lpspi_shared_function.c 的 LPSPI_DRV_IRQHandler 会按
 *          CFGR1.MASTER 位分发; Slave 模式下 Master 中断不会触发,
 *          提供空桩满足链接
 */
void LPSPI_DRV_MasterIRQHandler(uint32_t instance)
{
    (void)instance;
}
