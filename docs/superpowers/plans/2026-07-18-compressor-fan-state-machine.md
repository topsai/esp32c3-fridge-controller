# Compressor and Fan State Machine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用显式状态机实现压缩机停机后风扇继续运行 300000 ms，并保持现有温控接口和硬件电平语义。

**Architecture:** 纯 C++ 头文件定义压缩机/风扇状态和无副作用转换函数，编译期契约验证全部转换。`main.cpp` 负责把运行请求与 `millis()` 传入状态机，并将结果统一应用到 GPIO 和 OLED。

**Tech Stack:** C++11、Arduino、ESP32-C3、PlatformIO、Python unittest。

## Global Constraints

- 风扇延时固定为 300000 ms。
- 温控阈值、故障判定、按键、OLED、NVS 和传感器逻辑保持不变。
- 压缩机高电平运行、风扇高电平运行的现有电平语义保持不变。
- 重复停止请求不得刷新冷却计时。
- NTC 故障停机也进入风扇冷却状态。

---

### Task 1: 建立失败的状态转换契约

**Files:**
- Create: `src/state_machine_contract.cpp`

**Interfaces:**
- Consumes: 将要由 `include/fridge_state_machine.h` 提供的 `FridgeOutputs`、`requestCompressor()` 和 `advanceFanTimer()`。
- Produces: 编译期 `static_assert` 契约。

- [ ] **Step 1: 写入状态契约**

契约必须覆盖初始、开机、停机、299999 ms、300000 ms、重复停机、冷却中重启和时间回绕。

- [ ] **Step 2: 验证 RED**

Run: `pio run`

Expected: 因 `fridge_state_machine.h` 尚不存在而编译失败。

### Task 2: 实现状态机并接入固件

**Files:**
- Create: `include/fridge_state_machine.h`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces: `CompressorState`、`FanState`、`FridgeOutputs`、`requestCompressor(FridgeOutputs, bool, uint32_t)`、`advanceFanTimer(FridgeOutputs, uint32_t)`、`FAN_COOLDOWN_MS`。

- [ ] **Step 1: 实现最小纯转换函数**

转换函数必须为 `constexpr` 且不调用 Arduino API，使 `static_assert` 可以直接验证真实生产逻辑。

- [ ] **Step 2: 验证 GREEN**

Run: `pio run`

Expected: 所有编译期契约成立且构建 `SUCCESS`。

- [ ] **Step 3: 接入 GPIO 和 OLED**

`setRelayPhysical()` 发送运行/停止请求；`updateFanLogic()` 推进计时；一个输出应用函数集中写 GPIO；OLED 从状态机派生 ON/OFF。

- [ ] **Step 4: 运行全部回归**

Run: `python -m unittest discover -s tests -v`，然后运行 `pio run`。

Expected: Python 回归通过，PlatformIO 构建成功。

### Task 3: 更新文档并发布

**Files:**
- Modify: `README.md`
- Modify: `docs/hardware.md`
- Modify: `docs/operation.md`
- Modify: `docs/migration.md`

**Interfaces:**
- Consumes: 已验证状态机行为。
- Produces: 与实际固件一致的使用、接线、操作和迁移说明。

- [ ] **Step 1: 更新风扇行为和实机清单**

明确运行、冷却、到期、重启、故障停机和 GPIO 电平行为。

- [ ] **Step 2: 完整验证**

Run: `python -m unittest discover -s tests -v`、`pio run -t clean`、`pio run`、`git diff --check`。

Expected: 所有命令退出码 0。

- [ ] **Step 3: 提交并推送**

```powershell
git add include/fridge_state_machine.h src/state_machine_contract.cpp src/main.cpp tests README.md docs
git commit -m "feat: add compressor fan cooldown state machine"
git push
```

- [ ] **Step 4: 验证远端一致性**

本地 `HEAD` 与 `origin/main` 哈希必须一致，工作区必须干净。
