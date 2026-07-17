# ESP32-C3 冰箱控制器

基于 ESP32-C3 和 Arduino framework 的独立冰箱温度控制器固件，使用 PlatformIO 构建。项目从一个已在实际设备上运行的 Arduino 草图保守迁移而来，迁移过程中没有改动固件逻辑。

## 功能

- NTC 热敏电阻作为主要温控输入，并进行 EWMA 平滑。
- DS18B20 作为第二路数字温度显示，采用非阻塞读取。
- 128×32 SSD1306 OLED 显示设定值、温度和输出状态。
- 两个按键以 0.5 °C 步进调整设定温度，支持长按连续调整。
- 设定值存储在 ESP32 NVS 中，断电后保留。
- NTC 连续异常时关闭压缩机输出。
- 压缩机停机后风扇继续运行 5 分钟，再自动关闭。
- OLED 无操作 60 秒后自动熄屏。

> [!WARNING]
> 冰箱压缩机和市电具有触电、火灾及设备损坏风险。ESP32 只能连接到具有足够电气隔离、额定电压和额定电流裕量的驱动模块。接线、检修和首次通电必须由具备相应资质与经验的人员完成。

## 当前状态

- PlatformIO 编译：已通过。
- 固件迁移基线：初始迁移时 `src/main.cpp` 与源 `.ino` 的 SHA-256 相同；后续修复均记录在迁移文档中。
- 实机回归：原草图由用户确认可正常运行；迁移后的 PlatformIO 镜像仍需在目标硬件上烧录复核。

## 硬件

- ESP32-C3 开发板（PlatformIO 目标：`esp32-c3-devkitm-1`）
- DS18B20 温度传感器及合适的 OneWire 上拉电阻
- 10 kΩ、B3950 NTC 热敏电阻与 10 kΩ 分压电阻
- 128×32、I2C、地址 `0x3C` 的 SSD1306 OLED
- 两个按键
- 与压缩机和风扇匹配、带隔离和保护的驱动模块

接线摘要：

| 功能 | GPIO |
| --- | ---: |
| DS18B20 OneWire | 2 |
| NTC ADC | 4 |
| 压缩机继电器 | 3 |
| 风扇 | 10 |
| 升温按键 | 1 |
| 降温按键 | 7 |
| OLED SDA | 5 |
| OLED SCL | 6 |

完整电气语义与注意事项见 [`docs/hardware.md`](docs/hardware.md)。

## 快速开始

### 环境要求

- PlatformIO Core 或带 PlatformIO IDE 扩展的 Visual Studio Code
- 支持数据传输的 USB 线
- ESP32-C3 串口/USB 驱动（如操作系统需要）

### 构建

```sh
pio run
```

### 烧录

```sh
pio run -t upload
```

如果自动识别端口失败，可在 `platformio.ini` 中临时设置 `upload_port`，或使用 `pio device list` 查找端口。

### 串口监视

```sh
pio device monitor
```

串口配置为 115200 baud。固件默认 `LOGON = false`，此时不会初始化串口；需要日志时可在本地改为 `true` 后重新构建，但不要把调试改动误认为迁移基线。

## 串口 HIL 测试

项目提供独立的 `esp32-c3-hil` 构建环境，可在同一块 ESP32-C3 上运行完整主程序，同时通过 USB 串口
注入温度、触发按键、查看内部状态并运行自动场景。HIL 默认锁定压缩机和风扇实际输出。

```powershell
pio run -e esp32-c3-hil -t upload
python -m pip install -r hil/requirements.txt
python -m hil.dashboard --port COM5
```

详细协议、安全限制和场景说明见 [`docs/hil-testing.md`](docs/hil-testing.md)。

## 使用说明

- 默认设定温度为 -10 °C，可调范围为 -30 °C 至 20 °C。
- 短按任一按键以 0.5 °C 调整；长按约每 200 ms 连续调整 0.5 °C。
- 熄屏状态下第一次按键只唤醒屏幕，不改变设定值。
- NTC 温度高于“设定值 + 2 °C”时开启压缩机；低于“设定值 - 2 °C”时关闭。
- NTC 连续 10 次无效（采样周期约 500 ms）后关闭压缩机。

更完整的状态、采样和测试说明见 [`docs/operation.md`](docs/operation.md)。

## 重要实现说明

本项目优先保护现有设备行为，因此保留了源代码中的现状，包括注释与实现可能不一致的地方：

- 实际代码以 `HIGH` 开启 GPIO 3 的压缩机输出、以 `LOW` 关闭；接线必须依据实际驱动板验证，不能只依据旧注释。
- 风扇由显式状态机控制：压缩机运行时同步运行，停机后进入 5 分钟散热状态。
- `MIN_OFF_MS`、`MIN_ON_MS`、`CALIB_INTERVAL` 等常量目前已声明但没有参与实际控制。

这些内容没有在迁移中“顺手修复”，以避免改变已运行设备的逻辑。详见 [`docs/migration.md`](docs/migration.md)。

## 目录结构

```text
├── platformio.ini       # PlatformIO 环境与依赖
├── src/main.cpp         # 完整固件逻辑
├── docs/hardware.md     # 接线与电气安全
├── docs/operation.md    # 操作、控制与实机检查
├── docs/migration.md    # 来源、哈希与迁移差异
└── LICENSE              # MIT License
```

## 贡献

修改控制逻辑前，请先记录原硬件行为并进行台架测试。涉及压缩机保护、输出有效电平、传感器故障策略或引脚的修改，应单独提交并明确说明兼容性影响。

## 许可证

本项目使用 [MIT License](LICENSE)。许可证仅适用于软件，不构成对硬件安全性、适用性或制冷设备合规性的保证。
