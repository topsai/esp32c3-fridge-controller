# ArduinoOTA 安全升级设计

## 目标

为 ESP32-C3 冰箱控制器增加局域网 ArduinoOTA 升级能力。OTA 不得阻塞温控主循环，Wi-Fi 不可用时不得影响
传感器、按键、OLED、压缩机或风扇运行。真实 Wi-Fi 和 OTA 密码不得进入 Git。

## 架构

新增独立 OtaManager，状态为 Disabled、Connecting、Ready、Updating 和 Error。
setup() 只启动一次非阻塞连接，loop() 每轮调用 poll()；连接和重试均通过 millis() 推进，不使用长时间等待。

OTA 管理器通过回调通知主固件升级开始、进度、结束和错误。它不直接控制温控业务对象。

## 本地配置

仓库提交 include/ota_config.example.h，用户复制为 include/ota_config.local.h 并填写：

- Wi-Fi SSID；
- Wi-Fi 密码；
- OTA 主机名；
- OTA 密码。

ota_config.local.h 加入 .gitignore。文件不存在或配置为空时，OTA 状态为 Disabled，正式与 HIL 固件仍可完整构建和运行。

## 安全行为

- OTA 必须配置非空密码。
- 只支持设备所在局域网内的 ArduinoOTA，不开放互联网服务。
- OTA 开始时调用现有 setRelayPhysical(false)，压缩机立即关闭，风扇进入现有 5 分钟 Cooldown。
- OTA 更新期间禁止温控逻辑重新启动压缩机，风扇计时和 OLED/状态更新继续运行。
- OTA 成功后由 ArduinoOTA 完成重启。
- OTA 失败时解除升级锁，固件继续从当前有效镜像运行；温控在下一轮重新评估。
- Flash 使用默认双 OTA 分区；不得切换到 no_ota 分区。

## Wi-Fi 状态机

- 启动后发起一次 STA 连接，不阻塞等待。
- 连接成功后启动 ArduinoOTA 服务。
- 连接超时后进入退避，随后重新尝试。
- 运行中掉线后回到连接状态。
- 不在循环中打印高频日志。
- Wi-Fi 重连不改变压缩机或风扇状态。

## 用户反馈

OLED 在现有三行布局内只在 OTA 更新期间显示进度，避免长期改变主界面：

- 更新开始：显示 OTA Update；
- 更新过程：显示百分比；
- 成功：显示完成并即将重启；
- 失败：显示错误码，随后恢复常规界面。

HIL STATUS 增加 OTA 状态、Wi-Fi 连接状态、IP 地址和更新进度；不返回 SSID 密码或 OTA 密码。

## PlatformIO 使用

新增 esp32-c3-ota 环境，继承正式环境并设置 upload_protocol = espota。上传目标通过命令行传入：

    pio run -e esp32-c3-ota -t upload --upload-port fridge-controller.local

首次启用 OTA 必须使用 USB 烧录。后续只有设备已连接 Wi-Fi 且 OTA 服务为 Ready 时才能无线更新。

## 测试

- 纯状态转换测试覆盖禁用、连接、超时退避、就绪、掉线、更新和错误恢复。
- 构建无本地配置的正式、HIL 和 OTA 环境。
- 构建使用非敏感测试配置的 OTA 环境，证明代码路径可编译。
- 检查 Git 不跟踪本地配置。
- 回归现有状态机、HIL 和 Python 测试。
- 实机验证包括首次 USB 烧录、局域网发现、密码拒绝、成功更新、断网运行和更新失败恢复。

## 非目标

- 不提供浏览器上传页面。
- 不提供公网 OTA、云端版本检查或自动下载。
- 不提交真实网络凭据。
- 不声称编译成功等同于实机无线升级通过。
