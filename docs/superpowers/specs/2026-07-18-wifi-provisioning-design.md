# Wi-Fi 配网功能设计

## 目标

为 ESP32-C3 冰箱控制器增加无需重新编译 Wi-Fi 名称和密码的手机配网能力，同时保持现有温控、传感器、按键、OLED、
压缩机/风扇状态机、HIL 和 ArduinoOTA 行为。配网 Web 服务必须非阻塞，任何网络故障都不得阻塞或重置制冷控制。

## 明确触发条件

只允许以下两种情况进入配网：

1. 设备启动时没有任何已保存的 Wi-Fi 凭据；
2. 用户同时按住升温键和降温键至少 5 秒。

已有凭据时，无论连接失败多久、失败多少次或运行中掉线，都只在后台重连，绝不自动开启配网热点。双键按住期间暂停两个
`OneButton::tick()`，避免长按回调错误调整设定温度；达到 5 秒后只触发一次，松开双键才允许下一次触发。

## 技术方案

使用固定版本 `tzapu/WiFiManager @ 2.0.17`。WiFiManager 提供 ESP32 captive portal、网络扫描和非阻塞
`process()`；项目不调用会在连接失败时自动进入 portal 的 `autoConnect()`，只显式启动配置 portal。

新增独立 `WifiProvisioningManager`，负责：

- 判断是否存在已保存的 Wi-Fi 凭据；
- 启动普通 STA 连接及非阻塞重连；
- 启动、轮询和关闭非阻塞配置 portal；
- 处理双键请求的凭据清除；
- 向 OTA、OLED 和 HIL 暴露只读状态。

`OtaManager` 不再直接拥有 SSID/密码或决定 STA 重连。它消费 Wi-Fi 管理器提供的连接状态：联网后启动 ArduinoOTA，掉线时
暂停 OTA 服务，重新联网后恢复。OTA 主机名和 OTA 密码仍由本地 OTA 配置提供，配网页面不显示也不修改 OTA 密码。

## 状态与转换

Wi-Fi 配网状态：

- `NoCredentials`：没有凭据，启动 portal；
- `UnconfiguredIdle`：本次开机的 60 秒配网窗口已结束，保持未配置并返回正常界面；
- `Connecting`：使用已保存或兼容配置连接 STA；
- `Connected`：STA 已获得 IP；
- `RetryWait`：连接失败后的非阻塞退避；
- `Portal`：用户允许的配置热点正在运行。

转换规则：

- 启动无凭据：`NoCredentials -> Portal`；
- 启动有凭据：`Connecting -> Connected`；
- 连接超时或掉线：`Connecting/Connected -> RetryWait -> Connecting`；
- 连接失败和 `RetryWait` 不得转换为 `Portal`；
- 双键 5 秒：任意非 OTA 更新状态清除旧 Wi-Fi，随后进入 `Portal`；
- portal 保存有效凭据并联网：`Portal -> Connected`；
- portal 保存失败：保持 `Portal`，允许用户修正；
- portal 开启满 60 秒仍未保存成功：停止热点并进入 `UnconfiguredIdle`，本次开机不再自动开启；
- `UnconfiguredIdle` 只在设备重启或再次完成双键 5 秒手势后离开；下次开机因仍无凭据而再次 `NoCredentials -> Portal`；
- 正在 OTA 更新时忽略双键配网请求，防止网络切换中断写入。

所有计时使用无符号 `millis()` 差值，支持计时器回绕；不使用等待网络的 `delay()`。

## 凭据与兼容性

Wi-Fi SSID 和密码由 ESP32 Wi-Fi/NVS 持久化，不写入项目文件、日志、OLED、HIL 或 Git。

保留现有 `FRIDGE_WIFI_SSID` 和 `FRIDGE_WIFI_PASSWORD` 作为迁移兼容入口：当 NVS 没有凭据而两个编译期值均非空时，先用它们
发起 STA 连接并允许 ESP32 持久化；只有 NVS 与兼容配置都不存在时才属于首次无凭据并自动进入 portal。后续文档推荐使用 portal，
不再要求在本地头文件填写 Wi-Fi 凭据。

OTA 主机名默认可使用 `fridge-controller`，OTA 密码必须保持非空才能启用 OTA。Wi-Fi 已配好但 OTA 密码为空时，联网功能正常，
OTA 状态为 `disabled`。

## 配网热点与页面

热点名称为 `Fridge-Setup-XXXXXX`，其中后缀来自设备唯一标识。每次打开 portal 时用 ESP32 随机源生成新的临时 WPA2 密码，
长度至少 10 个字符。密码只显示在本机 OLED，不进入串口状态或持久化存储。

OLED 配网页面常亮并循环显示：

- `WiFi Setup`；
- 热点名称；
- 临时热点密码；
- `192.168.4.1`。

WiFiManager 页面只开放选择/输入 Wi-Fi 和退出所需菜单，不提供 Web OTA、设备重启、擦除或其他管理入口。手机未自动弹出
captive portal 时，用户手动访问 `http://192.168.4.1/`。

portal 最多运行 60 秒，保存成功或用户退出时也会提前关闭。超过 60 秒仍未完成配网时，热点关闭，OLED 立即恢复正常温度控制界面，
本次开机不再自动开启 portal。整个 60 秒窗口内配网服务保持非阻塞，冰箱控制照常运行。若没有保存凭据，下次重新开机仍会再次进入
60 秒配网窗口。双键手动开启的 portal 使用相同的 60 秒窗口。

## 控制安全

- 配网开始、连接、失败、重连或成功均不主动改变压缩机和风扇请求；
- 主温控循环和 5 分钟风扇冷却计时在 portal 期间继续执行；
- HIL 输出锁继续拥有最高物理输出限制；
- OTA 更新期间压缩机保持现有 OTA 安全停机逻辑，且禁止进入 portal；
- 双键配网手势只影响 Wi-Fi 凭据，不清除温度设定值或其他 NVS 命名空间；
- OLED 从配网页面返回常规页面时强制完整重绘，避免缓存造成空白或残影。

## HIL 与可观测性

HIL `STATUS` 增加：

- `wifi_state`：`no_credentials/connecting/connected/retry_wait/portal`；
- `wifi_state` 的未配置等待值为 `unconfigured_idle`；
- `wifi_provisioning`：是否正在配网；
- `wifi_ap_ssid`：仅 portal 期间返回热点名称；
- `wifi_connected` 和 `ota_ip` 继续表示 STA 状态和地址。

不返回家庭 Wi-Fi SSID、家庭 Wi-Fi 密码、热点临时密码或 OTA 密码。Python 仪表板在未连接串口时仍显示所有新增字段并标记为不可用。

## 测试与验收

### 自动化

- 纯 C++ 状态契约验证首次无凭据进入 portal；
- 验证有凭据的连接超时、重复失败和掉线只能进入退避/重连，不能进入 portal；
- 验证无凭据 portal 在 60 秒关闭并进入 `unconfigured_idle`，同一次开机不重复启动，模拟重启后再次启动；
- 验证双键不足 5 秒不触发、达到 5 秒只触发一次、松开后重新布防；
- 验证 OTA 更新期间双键请求被拒绝；
- Python 测试验证 HIL 仪表板新增字段在离线状态也存在；
- 原有按钮熄屏、压缩机/风扇状态机、HIL 协议和场景测试全部回归；
- clean build `esp32-c3-devkitm-1`、`esp32-c3-hil`、`esp32-c3-ota`；
- 每个固件镜像必须小于默认 OTA 应用分区 `1310720` 字节，并保留合理余量；
- 检查 Git 不跟踪本地 OTA 配置或任何真实凭据。

### 实机

- 擦除 Wi-Fi 凭据后重启，确认自动出现热点及 OLED 临时密码；
- 不提交配网并等待超过 60 秒，确认热点关闭、OLED 恢复温度界面、制冷控制持续运行，且下次重启再次出现热点；
- 输入错误家庭 Wi-Fi 密码，确认 portal 保持开放且制冷控制继续；
- 保存正确凭据，确认热点关闭、STA 获得 IP、OTA 进入 ready；
- 关闭路由器超过 2 分钟，确认热点始终不出现，路由器恢复后自动重连；
- 双键长按不足/超过 5 秒，确认设定温度不变且只有后者进入 portal；
- OTA 更新期间双键长按，确认不会开启 portal 或中断更新；
- 检查 OLED、风扇散热时间、继电器有效电平和真实压缩机行为。

自动测试和编译通过不能替代上述实机验证。

## 文档

更新 README、OTA 文档和 HIL 文档，说明首次配网、换路由器、双键手势、热点密码、连接失败行为、恢复方法和安全边界。

## 非目标

- 不因连接失败自动进入配网；
- 不提供公网配网、云端账户、蓝牙配网或手机 App；
- 不提供浏览器固件上传；
- 不允许配网页面修改温控参数或输出状态；
- 不把真实凭据写入仓库；
- 不声称未连接硬件的测试已经验证无线电、captive portal 或实体按键。
