/*
 * @brief 域控制器主程序入口 (SOC 端) — TODO 骨架
 *        Ubuntu 或 Linux 主机上运行，与 S32K144 MCU 通过 CAN 通信
 *
 * @note 对应书籍：后续各章节集成练习
 *
 * === 学习主线：CAN → SOME/IP → AUTOSAR CP/AP ===
 * 第一阶段(学习时实现)：CAN 通信
 *    main()
 *     ├─ ConfigManager  ← 加载 config/domain_config.yaml
 *     ├─ CanBus         ← SocketCAN 收发 (can_manager)
 *     └─ SignalFusion   ← CAN 信号解析 & 融合 (signal_manager)
 *
 * 第二阶段(学习时实现)：SOME/IP 服务通信
 *     ├─ SomeIpServer   ← SOME/IP 服务 (someip_service/)
 *     ├─ ara::com 服务模型映射
 *     └─ ara::sd  服务发现
 *
 * 第三阶段(后续)：AUTOSAR CP/AP 概念深入
 *     └─ UdsServer      ← UDS 诊断协议 (uds_server/)
 *
 * 检查项目根目录 docs/MCU零基础学习计划.md 获得完整学习路线
 */
#include <iostream>

int main(void) {
    std::cout << "========================================" << std::endl;
    std::cout << "  域控制器 SOC 端" << std::endl;
    std::cout << "  学习路线: CAN → SOME/IP → AUTOSAR" << std::endl;
    std::cout << "  参考 docs/MCU零基础学习计划.md" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
