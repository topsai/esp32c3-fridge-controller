# 串口 HIL 实时测试

## 测试边界

HIL 环境完整运行正式固件的 `setup()`、`loop()`、温控、故障、OLED、NVS 及压缩机/风扇状态机。
串口接口只替换选定的输入来源并提供状态观测，不另写一套温控模拟程序。

自动测试能够验证业务计算、状态转换、期望 GPIO 和 ESP32 引脚读回值。它不能自动证明实体按键、传感器接线、
OLED 像素、继电器触点、风扇叶片或压缩机真正动作。

## 安全要求

1. 首次测试时断开压缩机、风扇和所有市电负载。
2. 烧录 `esp32-c3-hil` 环境，而不是正式环境。
3. HIL 上电默认将 GPIO 3 和 GPIO 10 强制为低电平。
4. 只有确认低压测试接线后才能发送 `OUTPUTS UNLOCK`。
5. 解锁后电脑必须持续发送命令；10 秒没有命令会自动重新锁定。
6. 测试结束、串口异常或程序退出后，确认两个输出均回到低电平。

## 构建与连接

```powershell
pio run -e esp32-c3-hil
pio run -e esp32-c3-hil -t upload
pio device list
python -m pip install -r hil/requirements.txt
python -m hil.dashboard
```

未连接硬件时仪表板仍完整显示全部状态字段、曲线区域和控件，实时字段显示为不可用且不生成模拟数据。连接真实硬件时可从顶部选择端口，
也可使用 `python -m hil.dashboard --port COM5` 自动尝试连接。波特率固定为 115200。

## 协议

请求格式为 `HIL <序号> <命令> [参数]`，每行一个请求。响应为单行 JSON，携带相同序号。

常用示例：

```text
HIL 1 PING
HIL 2 STATUS
HIL 3 NTC VALUE -5.5
HIL 4 NTC FAULT
HIL 5 NTC PHYSICAL
HIL 6 DS VALUE -7.0
HIL 7 BUTTON UP CLICK
HIL 8 BUTTON DOWN LONG 2000
HIL 9 DISPLAY WAKE
HIL 10 OUTPUTS LOCK
```

仪表板提供 `温度 −0.5°C`、`温度 +0.5°C` 两个按钮，调用上述 `BUTTON DOWN CLICK` 和
`BUTTON UP CLICK` 指令。设定范围、熄屏唤醒和 NVS 保存均继续由主固件处理。

支持的完整命令见设计规格。温度注入进入原有 NTC 滤波、故障累计和温控路径；DS 注入进入显示状态路径。

## 输出锁

锁定只覆盖真实 GPIO，不停止内部状态机。因此 `STATUS` 同时提供：

- `expected_relay_level`、`expected_fan_level`：状态机希望输出的电平；
- `actual_relay_level`、`actual_fan_level`：安全锁之后 ESP32 引脚的实际读回电平；
- `outputs_unlocked`：是否允许真实输出跟随期望状态。
- `ota_state`、`ota_progress`：OTA 服务状态和升级百分比；
- `wifi_connected`、`ota_ip`：Wi-Fi 连接状态和局域网地址，不包含任何凭据。

这样可以在不带动负载的情况下验证完整温控逻辑。

## 自动场景

- `smoke.json`：握手、复位、默认锁定和实际低电平。
- `thermostat.json`：注入高温并验证压缩机与风扇期望状态。
- `fan_cooldown.json`：启动压缩机、注入 NTC 故障、验证 5 分钟冷却和到期关闭；需约 307 秒。

场景运行期间会定期发送 `PING` 作为心跳。JSON/HTML 报告默认写入 Git 忽略的 `hil/reports/`。

## 人工检查

- 实体按键短按、长按和去抖；
- 真实 NTC 与 DS18B20 温度和断线；
- OLED 是否点亮且文字正确；
- 隔离驱动输入与继电器触点；
- 风扇实际转动及停机后运行时间；
- 压缩机启停、电流、温升和保护。
