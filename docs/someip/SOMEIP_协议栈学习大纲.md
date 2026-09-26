# SOME/IP 协议栈学习大纲（面向 com / routing / someip 中间件开发）

> 学习样板：COVESA vsomeip（开源、可读可跑）。用它把"协议栈分层"读透，再映射到工作里的
> svlcom / svlsomeip / svlrouting 及那部分 Vector SOME/IP。**跳过协议理论**（SD / method /
> event / field 已掌握），直接看分层与实现。

## Context（为什么这样安排）
- 方向是协议栈中间件：COM 层 / internal routing / SOME/IP 层；做系统内 + 跨系统 / 跨 ECU 通信。
- 工作栈里 **Vector SOME/IP 负责 SOME/IP 协议栈那部分（mentor 确认：SD 流程由它完成）**；
  它与 svl* 的其余分工 / 边界还在工作中厘清 → 见「开放问题」。
- 日志流（据此推断分层，仅供对照，以工作中实际为准）：
  - Android：app → svlcom → svlsomeip → 210(pcap)
  - QNX：210 → svlsomeip → svlcom → svlrouting → vector someip → vlan10(pcap)
  - → QNX 节点像在做跨网段(210↔vlan10)的路由 / 网关（域控的活）；各层确切职责以实际为准。
- 学法：以 vsomeip 为样板，先把分层读透（确定、可核实）；再把工作栈边学边往上映射。
- 先要"分层 + 层间接口"的骨架认知 → 第一步搞清分层，不急着写代码。

## 核心 · 分层对照（概念 ↔ vsomeip 代码 ↔ 工作栈）
| 层 | 职责 | vsomeip（目录已核实存在） | 工作栈（按日志流推断，以实际为准）|
|---|---|---|---|
| 应用 / COM | 面向服务 API、method/event/field 的 payload 序列化 | `interface/vsomeip/` + `implementation/runtime/` | svlcom |
| SOME/IP | 报文头封装、SD、端点收发 | `implementation/message/` + `service_discovery/` + `endpoints/` | svlsomeip + **Vector SOME/IP**（SD 由 Vector，mentor 确认）|
| 内部路由 | 本地 app 间(UDS) 与远端(UDP/TCP) 间路由、offer/subscribe 分发 | `implementation/routing/` | svlrouting |
| 配置 | 服务 / 实例 / 端点 / 路由的 JSON 配置 | `implementation/configuration/` | 你栈的配置 |
| 上线 / 跨网段 | 端点把 SOME/IP + SD 发到网络（线格式标准化、跨栈可互通）| endpoints 的 UDP/TCP + SD 多播 | → 210 / vlan10（vlan10 侧 SOME/IP + SD 由 Vector）|

## 开放问题（ramp-up：去工作里问 / 查，答案只有你能填）
- **已确认（mentor）**：Vector SOME/IP 就是 **SOME/IP 协议栈**那部分，且 **SD 流程由它完成**；
  另观察到它会拦截 SecOC、过滤不合理报文（偏"安全网关 / 过滤"）。
- 待确认：报文头封装 / 端点收发是 Vector 还是 svlsomeip 做？两者在 SOME/IP 层如何分工？
- 待确认：svlcom / svlrouting 与 vsomeip 的 COM / routing 层是否一一对应？哪些链路 / ECU / VLAN 走 Vector？

## 分层精读顺序（vsomeip，由外到内、先接口后实现）
1. `interface/vsomeip/` — 公共 API（application / runtime / message / payload）。先看清 COM 层给 app 的接口面。
2. `implementation/routing/` ← **核心（做 routing 的）**：routing manager 的 Host / Client 两种角色；
   本地走 `/tmp/vsomeip-*` UDS、远端走网络；offer / request / event / subscribe 如何被路由。
   （已核实：#947 日志有 "routing manager [Host]"、"Routing root @ /tmp/vsomeip-0"、"local_server @ /tmp/vsomeip-1347"。）
3. `implementation/endpoints/` — local(UDS) 与 tcp/udp 端点；消息在节点内 / 跨节点怎么走；#947 多播路由 bug 也在这层。
4. `implementation/service_discovery/` — `service_discovery_impl.cpp` 状态机 + `*entry_impl` / `ip*_option_impl` 序列化。
5. `implementation/message/` — `serializer.cpp` / `deserializer.cpp` / `message_header_impl.cpp` / `payload_impl.cpp`（跨栈互通靠它对齐线格式）。
6. `implementation/configuration/` — JSON 如何定义服务 / 端点 / 路由（网关 / 域控最吃这块）。
7. `security/` / `e2e_protection/` / `tracing/` — 暂不读，以后有需要再看。

## 跑起来看分层（贴合 log + pcap 的习惯）
- 编译 vsomeip 3.7.6（C++20 / Boost ≥ 1.75 / CMake 3.13+），跑官方 examples（request-response / subscribe-notify / hello_world）。
- 开 vsomeip debug 日志，把每层日志（routing / endpoint / SD 的 log 行）对上——就像工作里 svlcom / svlsomeip / svlrouting 的 log。
- Wireshark 抓 SOME/IP + SD，把"日志里某一层的动作"和"线上的一个包"对应起来。
- 单机多节点：Docker 多容器（bridge、每容器独立 IP、**非 --net=host**，见 #656）当多 ECU；
  3.5.7+ 记得容器内 `ip route add 224.0.0.0/4 dev eth0`（#947 多播路由回归，含最新 3.7.6）。

## 关联本仓库（可选 sandbox，后续动手用）
- `soc/communication/ara/com/` 骨架对应上面分层：Proxy/Skeleton(COM API) / SomeIpBinding(报文) / ServiceDiscovery(SD)；
  MCU 侧 PduR 是"路由"的另一形态。想动手时可在此复刻 vsomeip 的分层。

## 版本与环境结论（已用 GitHub API + CMakeLists 实测）
- vsomeip 最新 = 3.7.6；依赖 C++20 / Boost ≥ 1.75 / CMake 3.13–3.28。
- ⚠️ 工程规则是 C++17，而 vsomeip 库需 C++20 编译；其公共头能否在 C++17 下 include 未验证，集成时确认。
- 备选 3.5.6（docker SD 开箱即用、回归前最后一版），但代码落后约两年。

## 参考
- vsomeip 仓库：https://github.com/COVESA/vsomeip （重点目录：routing/、endpoints/）
- Docker SD bug #947：https://github.com/COVESA/vsomeip/issues/947
- 根因 commit：https://github.com/COVESA/vsomeip/commit/55c5ed66c3436efe2369cfc4908f98712c276a66
- 多容器 #656：https://github.com/COVESA/vsomeip/issues/656

## 下一步（学习方向）
- A. 精读 routing manager（Host vs Client + 本地/远端路由）——最贴 svlrouting。
- B. 过一遍 COM API 面（`interface/vsomeip/` + `runtime/`）。
