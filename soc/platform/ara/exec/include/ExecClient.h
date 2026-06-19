/**
 * @file    ExecClient.h
 * @brief   [SKELETON] ara::exec::ExecClient — AUTOSAR AP 执行管理
 *
 * @note    对应 AUTOSAR AP ara::exec 规范
 *          负责应用生命周期管理和状态上报
 */

#ifndef ARA_EXEC_EXECCLIENT_H
#define ARA_EXEC_EXECCLIENT_H

#include <string>

namespace ara {
namespace exec {

/** @brief 应用状态 */
enum class ApplicationState {
    kRunning,
    kTerminating,
    kPreparingShutdown,
};

class ExecClient {
public:
    ExecClient() = default;
    ~ExecClient() = default;

    /** @brief 报告应用状态给执行管理 */
    void ReportExecutionState(ApplicationState state);

    /** @brief 获取机器状态 */
    std::string GetMachineState() const;

    /** @brief 阻塞等待执行管理终止请求 */
    bool WaitForTermination();

private:
    /* TODO: 实现与 ara::exec 的 IPC 通信 */
};

} /* namespace exec */
} /* namespace ara */

#endif /* ARA_EXEC_EXECCLIENT_H */
