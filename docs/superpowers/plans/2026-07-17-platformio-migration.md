# ESP32-C3 Fridge Controller PlatformIO Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将当前可运行的 `Fridge1.ino` 保守迁移为可重复构建、文档完整并发布到新公开 GitHub 仓库的 ESP32-C3 PlatformIO 工程。

**Architecture:** 固件继续保存在单一 `src/main.cpp` 中，保持原 Arduino 草图的控制流和全局状态结构。PlatformIO 仅负责目标板、Arduino framework、依赖解析和构建；专题文档独立描述硬件、操作及迁移差异。

**Tech Stack:** ESP32-C3、Arduino framework、PlatformIO 6.1.19、OneWire、DallasTemperature、OneButton、Adafruit GFX、Adafruit SSD1306、Git、GitHub CLI。

## Global Constraints

- 源基准固定为 `C:\Users\John\Desktop\Arduino_Projects\Fridge1\Fridge1.ino` 的当前工作区版本。
- 不主动修改温控、引脚、定时、继电器、风扇、按键、OLED、NVS 或传感器逻辑。
- Board 固定为 `esp32-c3-devkitm-1`，framework 固定为 `arduino`。
- 新 GitHub 仓库固定为公开仓库 `esp32c3-fridge-controller`。
- 不继承或修改旧仓库 `topsai/Fridge` 的 Git 历史和远程内容。
- 编译验证与实机验证必须分开报告。

---

### Task 1: 固化迁移基线并建立 PlatformIO 工程

**Files:**
- Create: `.gitignore`
- Create: `platformio.ini`
- Create: `src/main.cpp`
- Create: `docs/migration.md`

**Interfaces:**
- Consumes: 原始 `Fridge1.ino` 当前字节内容。
- Produces: PlatformIO 可解析工程、固件源文件、可审计的来源哈希。

- [ ] **Step 1: 记录原文件哈希和 Git 状态**

Run:

```powershell
Get-FileHash 'C:\Users\John\Desktop\Arduino_Projects\Fridge1\Fridge1.ino' -Algorithm SHA256
git -C 'C:\Users\John\Desktop\Arduino_Projects\Fridge1' status -sb
```

Expected: 输出 SHA-256；状态显示 `Fridge1.ino` 为未提交修改，证明使用的是当前工作区版本。

- [ ] **Step 2: 创建工程配置和忽略规则**

`platformio.ini` 必须声明：

```ini
[env:esp32-c3-devkitm-1]
platform = espressif32
board = esp32-c3-devkitm-1
framework = arduino
monitor_speed = 115200
lib_deps =
  paulstoffregen/OneWire
  milesburton/DallasTemperature
  mathertel/OneButton
  adafruit/Adafruit GFX Library
  adafruit/Adafruit SSD1306
```

`.gitignore` 必须排除 `.pio/`、`.vscode/`、`.idea/`、编辑器临时文件和系统元数据。

- [ ] **Step 3: 逐字节复制固件源文件**

先复制原文件为 `src/main.cpp`，不做格式化；若 `pio run` 报告只有 Arduino 自动原型差异，则补充最少量函数声明，并将差异写入 `docs/migration.md`。

- [ ] **Step 4: 建立迁移差异证据**

Run:

```powershell
git diff --no-index -- 'C:\Users\John\Desktop\Arduino_Projects\Fridge1\Fridge1.ino' 'src\main.cpp'
```

Expected: 初始复制无差异；任何后续差异只能是编译必需声明，并在迁移文档中逐项对应。

- [ ] **Step 5: 首次构建并修正机械性问题**

Run:

```powershell
pio run
```

Expected: `SUCCESS`。若失败，只修复 C++ 编译形态或依赖声明，不改变业务行为，并重新运行直至得到明确结果。

- [ ] **Step 6: 提交工程基线**

```powershell
git add .gitignore platformio.ini src/main.cpp docs/migration.md
git commit -m "build: migrate firmware to PlatformIO"
```

### Task 2: 编写完整项目文档与许可证

**Files:**
- Create: `README.md`
- Create: `docs/hardware.md`
- Create: `docs/operation.md`
- Create: `LICENSE`
- Modify: `docs/migration.md`

**Interfaces:**
- Consumes: `src/main.cpp` 的实际常量、引脚和行为，以及 Task 1 的构建结果。
- Produces: 面向开发、接线、操作、维护和审计的完整文档集。

- [ ] **Step 1: 从代码复核文档事实**

Run:

```powershell
rg -n "#define|const .*TEMP|HYSTERESIS|MIN_.*_MS|SCREEN_TIMEOUT|CALIB_INTERVAL|prefs\.|0x3C|attach" src/main.cpp
```

Expected: 得到所有需要写入文档的引脚、参数、NVS、OLED 和按键行为，不从注释猜测实现。

- [ ] **Step 2: 编写 README**

README 必须包含项目状态、功能、硬件/软件要求、快速开始、接线摘要、操作摘要、配置参数、目录结构、构建/烧录/串口命令、安全警告、验证边界、贡献和许可证。

- [ ] **Step 3: 编写专题文档**

`docs/hardware.md` 必须包含完整引脚表、电平语义、传感器/OLED 接线、供电和市电隔离警告；`docs/operation.md` 必须包含启动、温控、采样、显示、按键、NVS、故障、日志及实机测试清单；`docs/migration.md` 必须包含来源哈希、文件映射、全部必要差异和构建环境。

- [ ] **Step 4: 添加 MIT License**

版权行为 `Copyright (c) 2026 Frank`，正文使用标准 MIT License 全文。

- [ ] **Step 5: 检查文档一致性和链接**

Run:

```powershell
$patterns = @('T' + 'BD', 'T' + 'ODO', '待' + '补充', '占' + '位')
Get-ChildItem README.md, docs, LICENSE -Recurse -File | Select-String -Pattern $patterns
rg -n "GPIO|0x3C|setpoint_x2|pio run|pio run -t upload|pio device monitor" README.md docs
```

Expected: 无占位项；关键事实与命令均能在文档中定位。

- [ ] **Step 6: 提交文档**

```powershell
git add README.md docs/hardware.md docs/operation.md docs/migration.md LICENSE
git commit -m "docs: add hardware and operation guides"
```

### Task 3: 完整验证并发布 GitHub

**Files:**
- Modify: 仅在验证发现事实错误时修改对应工程或文档文件。

**Interfaces:**
- Consumes: 完整本地工程、可用 GitHub 认证。
- Produces: 干净的本地 `main`、公开 GitHub 仓库及可追踪的远程提交。

- [ ] **Step 1: 执行全新构建**

Run:

```powershell
pio run -t clean
pio run
```

Expected: 构建结束为 `SUCCESS`，退出码 0，并输出 RAM/Flash 使用量。

- [ ] **Step 2: 审查仓库内容**

Run:

```powershell
git status -sb
git diff --check
git ls-files
git log --oneline --decorate
```

Expected: 无未提交文件、无空白错误；仅包含计划内文件，提交历史清晰。

- [ ] **Step 3: 安装并验证 GitHub CLI**

若 `gh` 尚未安装，按 GitHub 发布流程由用户完成安装/授权后运行：

```powershell
gh --version
gh auth status
```

Expected: CLI 可用且已登录目标 GitHub 账户。

- [ ] **Step 4: 创建公开仓库并推送**

Run:

```powershell
gh repo create esp32c3-fridge-controller --public --source . --remote origin --push
```

Expected: 创建新公开仓库、添加 `origin`、推送本地 `main` 并设置上游。

- [ ] **Step 5: 验证远程一致性**

Run:

```powershell
gh repo view --json nameWithOwner,visibility,defaultBranchRef,url
git remote -v
git ls-remote origin refs/heads/main
git rev-parse HEAD
```

Expected: 仓库名为 `esp32c3-fridge-controller`、可见性为 `PUBLIC`、默认分支为 `main`，远端与本地提交哈希一致。
