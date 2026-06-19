/**
 * @file    ExecClient.cpp
 * @brief   [SKELETON] ara::exec::ExecClient 实现
 */

#include "ExecClient.h"

namespace ara {
namespace exec {

void ExecClient::ReportExecutionState(ApplicationState state) {
    (void)state;
    /* TODO: 通过 IPC 向 Execution Management 报告 */
}

std::string ExecClient::GetMachineState() const {
    /* TODO: 查询机器状态 */
    return "Running";
}

bool ExecClient::WaitForTermination() {
    /* TODO: 阻塞等待终止信号 */
    return false;
}

} /* namespace exec */
} /* namespace ara */
