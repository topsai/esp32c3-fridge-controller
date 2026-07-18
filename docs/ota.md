# ArduinoOTA 无线升级

项目使用 ArduinoOTA 在同一局域网内升级 ESP32-C3。没有配置凭据时，OTA 保持 `disabled`，冰箱控制逻辑照常运行。OTA 不是网页上传服务，也不会开放 HTTP 管理页面。

## 安全模型

- OTA 必须配置密码；不要使用示例密码。
- Wi-Fi 名称、Wi-Fi 密码和 OTA 密码只保存在本机 `include/ota_config.local.h`。
- 该文件已被 Git 忽略，不应复制到问题、日志或截图中。
- 仅在可信局域网使用 OTA；ArduinoOTA 的密码认证不能替代网络隔离。
- 升级开始时固件通过现有压缩机/风扇状态机关闭压缩机，风扇继续执行已有的 5 分钟散热流程。
- 写入期间温控器不能重新启动压缩机，主循环、传感器、按钮、HIL 和风扇计时仍继续执行。

## 首次配置

复制示例文件：

```powershell
Copy-Item include/ota_config.example.h include/ota_config.local.h
```

编辑 `include/ota_config.local.h`：

```cpp
#pragma once
#define FRIDGE_WIFI_SSID "实际的 Wi-Fi 名称"
#define FRIDGE_WIFI_PASSWORD "实际的 Wi-Fi 密码"
#define FRIDGE_OTA_HOSTNAME "fridge-controller"
#define FRIDGE_OTA_PASSWORD "单独设置的高强度 OTA 密码"
```

OTA 主机名和 OTA 密码必须非空。Wi-Fi 名称和密码仅作为旧配置迁移入口；新设备可通过开机配网页面把网络凭据保存到 NVS。
主机名只写名称，不加 `.local`。建议 OTA 密码与 Wi-Fi 密码不同。

## 首次 USB 烧录

现有设备尚未运行 OTA 服务，因此第一次必须通过 USB 安装带本地配置的固件：

```powershell
pio run -e esp32-c3-devkitm-1 -t upload
```

确认设备正常控制、Wi-Fi 已连接后，后续才使用无线升级。默认分区表包含 `otadata`、`app0` 和 `app1`，允许双 OTA 应用分区写入。

## 无线升级

电脑和 ESP32-C3 必须在可互通的同一局域网。执行：

```powershell
$env:FRIDGE_OTA_PASSWORD="与 ota_config.local.h 相同的 OTA 密码"
pio run -e esp32-c3-ota -t upload --upload-port fridge-controller.local
```

`FRIDGE_OTA_PASSWORD` 只存在于当前 PowerShell 进程，PlatformIO 通过 `--auth` 传给 `espota`；不要把密码写进 `platformio.ini`。上传完成后可执行
`Remove-Item Env:FRIDGE_OTA_PASSWORD` 清除变量。若 mDNS 主机名无法解析，从路由器或 HIL `STATUS` 的 `ota_ip` 找到设备 IP：

```powershell
pio run -e esp32-c3-ota -t upload --upload-port 192.168.1.123
```

不要把设备地址永久写进仓库，因为 DHCP 地址可能变化。

## 设备状态

OTA 状态包括：

- `disabled`：本地配置不存在、不完整或含空值；
- `connecting`：正在非阻塞连接 Wi-Fi；
- `ready`：Wi-Fi 和 ArduinoOTA 服务可用；
- `updating`：正在接收并写入固件；OLED 显示百分比；
- `error`：连接或升级失败，随后自动恢复服务或按 30 秒间隔重连。

HIL `STATUS` 和 Python 仪表板显示 `ota_state`、`ota_progress`、`wifi_connected`、`ota_ip`，不会输出任何密码。

## 故障排查

- `ota_state=disabled`：检查本地文件名、四个宏名和空值，然后重新用 USB 烧录。
- 长时间 `connecting/error`：检查 2.4 GHz Wi-Fi、SSID/密码、信号、客户端隔离和 DHCP。
- `.local` 无法解析：直接使用设备 IP；Windows 网络或路由器可能不提供可用的 mDNS。
- `Authentication Failed`：上传密码与设备编译时的 `FRIDGE_OTA_PASSWORD` 不一致。
- 上传中断：设备保持在原有效应用分区；恢复网络后重试。若设备无法启动或 OTA 服务不可用，使用 USB 恢复。
- 固件过大：查看 PlatformIO 的 Flash 使用量；单个应用必须放入 OTA 应用分区。

## 验证边界

自动化测试会验证状态转换、秘密文件策略、三个构建环境、HIL 字段和固件尺寸。只有连接真实 ESP32-C3、Wi-Fi 和安全的低压测试负载后，才能验证 mDNS、认证、无线传输、重启和真实输出行为。
