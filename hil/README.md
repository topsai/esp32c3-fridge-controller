# 串口 HIL 工具

此目录包含冰箱控制器 HIL 固件的电脑端工具。HIL 运行真实 `main.cpp`，但允许通过 USB 串口注入传感器值和按键事件，并默认锁定真实输出。

## 安装

```powershell
python -m pip install -r hil/requirements.txt
```

## 烧录 HIL 固件

```powershell
pio run -e esp32-c3-hil -t upload
```

不要在连接压缩机或市电负载时进行无人值守测试。

## 仪表板

```powershell
python -m hil.dashboard
```

仪表板页面始终可以打开，是否连接串口只决定数据来源。未连接时完整保留状态字段、曲线区域和全部控件，实时字段显示为不可用，不生成模拟数据。可在页面顶部选择端口并连接，也可以用 `--port COM5` 启动后自动尝试连接。

连接后每秒查询一次真实状态。真实输出默认锁定；解锁按钮需要人工确认，红色横幅表示实际 GPIO 可能动作。

## 命令行场景

```powershell
python -m hil.run_scenario --port COM5 hil/scenarios/smoke.json
python -m hil.run_scenario --port COM5 hil/scenarios/thermostat.json
python -m hil.run_scenario --port COM5 hil/scenarios/fan_cooldown.json
```

报告写入 `hil/reports/`，包括 JSON 和 HTML。默认场景不会解锁真实输出。

## 运行电脑端测试

```powershell
python -m unittest discover -s hil/tests -v
```
