# 迁移说明

## 来源

本工程以 `C:\Users\John\Desktop\Arduino_Projects\Fridge1\Fridge1.ino` 的当前工作区版本为唯一固件基准。
迁移时源文件在旧仓库中处于已修改、尚未提交状态，因此采用磁盘上的当前版本，而不是旧仓库 `HEAD` 中的版本。

源文件和首次复制后的 `src/main.cpp` 具有相同的 SHA-256：

```text
CD6798A846E9CEFEC89841E054A729D8D94593F0D4EE9E611E2773E11E6F59B0
```

这证明迁移起点没有改变任何固件字节。

## 文件映射

| Arduino 项目 | PlatformIO 项目 | 说明 |
| --- | --- | --- |
| `Fridge1.ino` | `src/main.cpp` | 固件的唯一实现文件 |
| Arduino IDE 自动库解析 | `platformio.ini` 的 `lib_deps` | 显式依赖解析 |

## 迁移原则与实际差异

- 不调整业务逻辑、引脚、常量、控制时序或启动行为。
- 不格式化、不拆分模块、不清理注释或重命名符号。
- 普通 C++ 编译没有要求补充函数声明。
- 初始迁移时 `src/main.cpp` 与源 `.ino` 逐字节一致。
- 新增内容仅为 PlatformIO 配置、忽略规则和项目文档。

## 迁移后的缺陷修复

### 长按调温时 OLED 意外熄灭

`loop()` 在按键回调之前缓存 `now`，长按回调随后把 `lastInteraction` 更新为更晚的 `millis()`；
同一轮循环末尾使用 `now - lastInteraction` 进行无符号减法时可能下溢，从而被误判为已经超过 60 秒。

修复只将 OLED 超时判断的当前时间改为回调之后现场读取的 `millis()`。按键、温控、显示内容和超时时长均未改变。

## 已验证的构建环境

- PlatformIO Core 6.1.19
- Espressif 32 platform 7.0.1
- Arduino-ESP32 framework 2.0.17
- Board：`esp32-c3-devkitm-1`
- 串口监视速率：115200 baud；只有 `LOGON` 为 `true` 时固件才初始化串口。

首次 `pio run` 已成功完成依赖解析、编译、链接和 ESP32-C3 镜像生成。OneWire 2.3.8 在其库源码中产生两条
`#undef` 语法警告，但没有造成构建失败，也不来自本项目固件。

## 验证边界

PlatformIO 构建不能证明传感器、OLED、按键、继电器、风扇或压缩机已在真实硬件上通过测试。
详细实机检查项目见 [`operation.md`](operation.md)。
