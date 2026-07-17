# Serial HIL Testing System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 通过 ESP32-C3 自身 USB 串口提供隔离、安全、实时的 HIL 指令测试，并由 Python 工具执行场景、显示状态和生成报告。

**Architecture:** 固件侧使用无动态分配的 ASCII 解析器和仅在 `FRIDGE_HIL` 环境启用的注入/遥测层；正式温控路径消费注入后的输入。Python 侧分离协议、串口、场景运行器与 Tk 仪表板，共用同一测试核心。

**Tech Stack:** PlatformIO、Arduino ESP32、C++11、Python 3、pyserial、tkinter、unittest。

## Global Constraints

- 正式环境不得接受任何 HIL 指令。
- HIL 上电、复位和心跳超时均必须锁定 GPIO 3/10。
- 输出解锁必须是显式命令；状态快照同时报告期望与实际电平。
- HIL 注入不得绕过现有温控、故障、状态机或显示业务路径。
- 不连接第二块控制芯片。

---

### Task 1: 协议解析器与 HIL 构建环境

**Files:**
- Modify: `platformio.ini`
- Create: `include/hil_protocol.h`
- Create: `src/hil_protocol.cpp`
- Create: `src/hil_protocol_contract.cpp`

**Interfaces:**
- Produces: `HilCommandType`、`HilCommand`、`HilParseResult`、`parseHilCommand()`。

- [ ] **Step 1:** 先写编译期/构建契约并运行 `pio run -e esp32-c3-hil`，确认缺少解析器实现时失败。
- [ ] **Step 2:** 实现固定缓冲、无动态分配的逐行解析器和稳定错误码。
- [ ] **Step 3:** 分别运行正式环境和 HIL 环境构建，要求均成功。

### Task 2: 固件注入、遥测与输出安全锁

**Files:**
- Modify: `src/main.cpp`
- Create: `include/hil_runtime.h`
- Create: `src/hil_runtime.cpp`

**Interfaces:**
- Produces: NTC/DS 输入模式、输出锁、10 秒心跳看门狗、串口轮询、JSON ACK/STATUS。

- [ ] **Step 1:** 增加 HIL 运行状态和默认锁定测试，先使集成契约失败。
- [ ] **Step 2:** 将 NTC/DS 注入接入现有业务输入点，按键指令调用现有业务回调。
- [ ] **Step 3:** 在 `applyOutputState()` 应用安全锁；锁定时实际输出低但保留期望状态。
- [ ] **Step 4:** 实现完整命令分派、状态 JSON、复位、NVS 清除、重启和心跳锁定。
- [ ] **Step 5:** 构建两个环境并确认正式固件不包含 `HIL PING` 命令文本。

### Task 3: Python 协议、传输与场景运行器

**Files:**
- Create: `hil/protocol.py`
- Create: `hil/transport.py`
- Create: `hil/runner.py`
- Create: `hil/tests/test_protocol.py`
- Create: `hil/tests/test_runner.py`
- Create: `hil/requirements.txt`

**Interfaces:**
- Produces: `HilProtocolClient`、`SerialTransport`、`ScenarioRunner` 和 JSON/HTML 报告。

- [ ] **Step 1:** 为请求序号、JSON 校验、超时、断言和清理写失败测试。
- [ ] **Step 2:** 实现最小协议、串口和场景核心使测试通过。
- [ ] **Step 3:** 运行 `python -m unittest discover -s hil/tests -v`。

### Task 4: 场景、仪表板与文档

**Files:**
- Create: `hil/dashboard.py`
- Create: `hil/scenarios/smoke.json`
- Create: `hil/scenarios/thermostat.json`
- Create: `hil/scenarios/fan_cooldown.json`
- Create: `hil/README.md`
- Create: `docs/hil-testing.md`
- Modify: `README.md`
- Modify: `.gitignore`

**Interfaces:**
- Produces: 串口选择、状态轮询、手动安全控制、场景运行、实时日志及报告入口。

- [ ] **Step 1:** 实现 tkinter 仪表板，UI 只调用测试核心，不复制协议逻辑。
- [ ] **Step 2:** 添加握手、安全锁、温控和真实 5 分钟风扇冷却场景。
- [ ] **Step 3:** 完成刷写、连接、指令、场景、安全和限制文档。

### Task 5: 完整验证与发布

- [ ] **Step 1:** 运行全部 Python 测试和现有回归测试。
- [ ] **Step 2:** 对正式与 HIL 环境分别执行 clean build。
- [ ] **Step 3:** 检查正式二进制不含 HIL 命令标识、工作区和文档一致性。
- [ ] **Step 4:** 提交 `feat: add serial HIL testing system`，推送并核对远端哈希。
