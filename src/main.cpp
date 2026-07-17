#include <OneWire.h>
#include <DallasTemperature.h>
// #include <Encoder.h>
#include <OneButton.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <Preferences.h>
bool LOGON = false;  // 日志状态（ true=开） false=关


// ---------------- 引脚定义 ----------------
#define ONE_WIRE_BUS 2  // DS18B20 数据引脚
#define NTC_PIN 4       // NTC 模拟输入引脚
#define RELAY_PIN 3     // 压缩机继电器，低电平有效
#define FAN_PIN 10      // 风扇控制，高电平有效
#define SWA 1           // 编码器 A 相
#define EC_DT 0         // 编码器 B 相
#define SWB 7           // 编码器按键，低电平有效
#define I2C_SDA 5       // OLED I2C SDA
#define I2C_SCL 6       // OLED I2C SCL

// ---------------- OLED 设置 ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- 传感器对象 ----------------
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
// Encoder encoder(EC_CLK, EC_DT);
// 创建Button对象
// OneButton button(EC_SW, true);
OneButton button1(SWA, true);
OneButton button2(SWB, true);
// ---------------- NTC 参数 ----------------
// 使用 Steinhart-Hart 模型计算温度
const float VCC = 3.3f;           // 系统电压
const int ADC_MAX = 4095;         // ESP32 ADC 分辨率
const float NTC_R25 = 10000.0f;   // 25℃时电阻为 10kΩ
const float NTC_BETA = 3950.0f;   // NTC 热敏电阻 B 值
const float R_SERIES = 10000.0f;  // 分压电阻，15kΩ 接在 VCC 侧

// ---------------- 数据持久化 ----------------
// 创建 Preferences 对象
Preferences prefs;

// ---------------- 控制参数 ----------------
const int MIN_TEMP = -30;                  // 最低设定温度
const int MAX_TEMP = 20;                   // 最高设定温度
const int DEFAULT_TEMP = -10;              // 默认设定温度
int setpoint_x2 = DEFAULT_TEMP * 2;        // 设定值，放大 2 倍用于支持 0.5°C 精度
int lastSavedSetpoint_x2;                  // 上次写入 NVS 的值
volatile bool needSave = false;            // 标志位，表示需要保存
const int HYSTERESIS_X2 = 4;               // 回差 (2°C)，放大 2 倍
const unsigned long MIN_OFF_MS = 30000UL;  // 压缩机最小关机时间 300s
const unsigned long MIN_ON_MS = 12000UL;   // 压缩机最小开机时间 120s
const unsigned long LOGON_MS = 30000UL;    // 日志打印间隔时间 30s
bool needOff = 0;
bool needOn = 1;
// ---------------- 状态变量 ----------------
bool compressorOn = true;             // 压缩机状态（true=开）
unsigned long relayChangeMillis = 0;  // 最近一次切换时间
float ntcOffset = 0.0f;               // NTC 偏移校准值

// 屏幕省电相关
unsigned long lastInteraction = 0;
const unsigned long SCREEN_TIMEOUT = 60000UL;  // 60s 无操作自动熄屏
bool screenOn = true;

// 校准定时（NTC 与 DS18B20 偏移校准）
unsigned long lastCalibMillis = 0;
const unsigned long CALIB_INTERVAL = 60000UL;  // 每 60s 校准一次

// DS18B20 非阻塞读取相关
unsigned long lastDSRequest = 0;
float lastDSTemp = NAN;
float lastNTCTemp = NAN;
bool dsRequestPending = false;

// 故障检测相关
bool sensorFault = false;                  // 当前是否故障
const unsigned long FAULT_DELAY = 5000UL;  // 传感器连续失效 5s 判定故障
unsigned long faultStartMillis = 0;
int NTC_ERR_COUNT = 0;  // NTC 错误统计
// ---------------- 辅助函数 ----------------

// 控制继电器和风扇物理引脚
// bool compressorOn = false;           // 压缩机状态
bool fanOn = false;                  // 风扇状态
unsigned long fanOffDelayStart = 0;  // 记录风扇延时关闭开始时间
void updateFanLogic() {
  if (digitalRead(RELAY_PIN)) {
    digitalWrite(FAN_PIN, HIGH);
    fanOn = true;
  } else {
    digitalWrite(FAN_PIN, LOW);
    fanOn = false;
  }
}
// 继电器控制函数
void setRelayPhysical(bool state) {
  if (state) {
    // 开启压缩机
    digitalWrite(RELAY_PIN, HIGH);
    compressorOn = true;
  } else {
    // 关闭压缩机
    digitalWrite(RELAY_PIN, LOW);
    compressorOn = false;
  }
}
// void setRelayPhysical(bool status) {
//   digitalWrite(RELAY_PIN, status ? LOW : HIGH);  // 压缩机低电平有效
//   digitalWrite(FAN_PIN, status ? HIGH : LOW);    // 风扇高电平有效
//   compressorOn = status;                         // 更新状态变量
//   relayChangeMillis = millis();                  // 记录切换时间
// }

// EWMA 滤波参数
const float NTC_ALPHA = 0.05f;  // 平滑系数 0~1，越小越平滑
float filteredNTCTemp = NAN;    // EWMA 滤波后的温度
// 读取 NTC 温度（带 Steinhart-Hart 公式和偏移校准）
float readNTCTempC() {
  int raw = analogRead(NTC_PIN);               // ADC 读取
  if (raw <= 0 || raw >= ADC_MAX) return NAN;  // 越界则无效
  float v = raw * VCC / ADC_MAX;               // 电压
  float rntc = R_SERIES * v / (VCC - v);       // NTC 电阻值
  if (rntc <= 0) return NAN;

  // Steinhart-Hart 近似公式
  float steinhart = 1.0f / ((1.0f / (25.0f + 273.15)) + (1.0f / NTC_BETA) * log(rntc / NTC_R25));
  float newTemp = steinhart - 273.15f + ntcOffset;  // 转换为摄氏度并加校准值
  if (LOGON) Serial.print("Raw-NTC:");
  if (LOGON) Serial.println(newTemp);

  // 阈值抑制
  // if (!isnan(filteredNTCTemp) && fabs(newTemp - filteredNTCTemp) > 1.5f) {
  //   // 超过 1.5°C 的瞬时跳变，直接忽略
  //   newTemp = filteredNTCTemp;
  // }

  // EWMA 滤波
  if (isnan(filteredNTCTemp)) filteredNTCTemp = newTemp;  // 初始化
  filteredNTCTemp = NTC_ALPHA * newTemp + (1.0f - NTC_ALPHA) * filteredNTCTemp;

  return filteredNTCTemp;
}

// 判断温度值是否有效
bool isFloatValid(float v) {
  return !isnan(v) && v > -60.0f && v < 60.0f;
}

// 强制点亮 OLED
void turnDisplayOnNow() {
  // 延长 OLED
  lastInteraction = millis();

  if (!screenOn) {
    if (LOGON) Serial.println("OLED On");
    display.ssd1306_command(SSD1306_DISPLAYON);
    screenOn = true;
  }
}

// 强制关闭 OLED
void turnDisplayOffNow() {
  if (!screenOn) return;
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  if (LOGON) Serial.println("OLED Off");
  screenOn = false;
}
bool oledDirty = false;  // OLED 更新标志
String lastLines[8];     // 最多 8 行
// 根据需要定义行索引对应的 Y 坐标
const uint8_t lineY[3] = { 0, 8, 16 };  // 每行Y坐标
// 每行高度 8 或 16 像素，根据你的字体
const uint8_t lineHeight = 8;  // 你的 setTextSize(1) 对应8像素高
// 你写了 lineHeight = 8，但 setTextSize(1) 时 Adafruit_GFX 的字体是 8px 高 + 1px 行间距，实际大约 9~10 像素。
// 👉 建议 lineHeight = 10，否则文字底部可能被后面的行覆盖。
// 只更新变动的行（不刷新屏幕）
void drawLineNoRefresh(uint8_t y, uint8_t idx, const String& text) {
  if (lastLines[idx] == text) return;  // 没变化就跳过

  lastLines[idx] = text;

  // 擦除该行背景
  display.fillRect(0, y, SCREEN_WIDTH, lineHeight, SSD1306_BLACK);

  // 写新内容
  display.setCursor(0, y);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.print(text);
  oledDirty = true;  // 有更新才需要刷新
}

// 更新 OLED 显示
void updateOLED(float dsTemp, float ntcTemp) {
  if (!screenOn) return;
  // 计算运行时间
  unsigned long ms = millis();
  unsigned long totalMinutes = ms / 60000;
  unsigned long hours = totalMinutes / 60;
  unsigned long minutes = totalMinutes % 60;

  char buf[32];

  // 第一行：设定温度 & 时间
  snprintf(buf, sizeof(buf), "Set: %.1fC %luh:%lum",
           (float)setpoint_x2 / 2.0f, hours, minutes);
  drawLineNoRefresh(lineY[0], 0, buf);

  // // 第二行：DS18B20 温度
  // if (isFloatValid(dsTemp))
  //   snprintf(buf, sizeof(buf), "DS: %.1f C", dsTemp);
  // else
  //   snprintf(buf, sizeof(buf), "DS: -- C");
  // drawLineNoRefresh(lineY[1], 1, buf);

  // // 第三行：NTC 温度
  // if (isFloatValid(ntcTemp))
  //   snprintf(buf, sizeof(buf), "NTC: %.1f C", ntcTemp);
  // else
  //   snprintf(buf, sizeof(buf), "NTC: -- C");
  // drawLineNoRefresh(lineY[2], 2, buf);
  // 第二行：合并 DS18B20 与 NTC 温度
  char dsBuf[16];
  char ntcBuf[16];

  if (isFloatValid(dsTemp))
    snprintf(dsBuf, sizeof(dsBuf), "DS: %.1fC", dsTemp);
  else
    snprintf(dsBuf, sizeof(dsBuf), "DS: --C");

  if (isFloatValid(ntcTemp))
    snprintf(ntcBuf, sizeof(ntcBuf), " NTC: %.1fC", ntcTemp);  // 注意前面有个空格
  else
    snprintf(ntcBuf, sizeof(ntcBuf), " NTC: --C");

  // 合并成一行
  snprintf(buf, sizeof(buf), "%s%s", dsBuf, ntcBuf);
  drawLineNoRefresh(lineY[1], 1, buf);


  // 第四行：压缩机 & 风扇状态
  snprintf(buf, sizeof(buf), "Comp:%s Fan:%s",
           compressorOn ? "ON " : "OFF",
           fanOn ? "ON " : "OFF");
  drawLineNoRefresh(lineY[2], 3, buf);

  // 第五行：故障状态
  snprintf(buf, sizeof(buf), "Fault:%s", sensorFault ? "YES" : "NO");
  drawLineNoRefresh(lineY[3], 4, buf);

  // 🔥最后统一刷新一次屏幕
  if (oledDirty) {
    display.display();
    oledDirty = false;
  }
}

// // 处理编码器旋钮 & 按键
// void handleEncoderAndButton() {
//   // 旋钮旋转检测
//   long enc = encoder.read();
//   long diff = enc - lastEncPos;
//   if (diff >= 4 || diff <= -4) {  // 每 4 个脉冲算 1 步
//     int steps = diff / 4;
//     setpoint_x2 += steps;
//     // 限制在范围内
//     if (setpoint_x2 < MIN_TEMP * 2) setpoint_x2 = MIN_TEMP * 2;
//     if (setpoint_x2 > MAX_TEMP * 2) setpoint_x2 = MAX_TEMP * 2;
//     lastEncPos += steps * 4;
//     turnDisplayOnNow();
//     // 保存到 Flash
//     prefs.putInt("setpoint_x2", setpoint_x2);
//   }
// }

// 实际写入 flash
void saveSetpoint() {
  if (prefs.begin("fridge", false)) {  // 打开命名空间
    prefs.putInt("setpoint_x2", setpoint_x2);
    prefs.end();
    lastSavedSetpoint_x2 = setpoint_x2;  // 更新已保存值
    if (LOGON) Serial.print("保存温度到 NVS: ");
    if (LOGON) Serial.println(setpoint_x2);
  }
}
// ---------------- 按键事件 ----------------
void buttonClickUp() {
  if (LOGON) Serial.println("Up单击");
  if (screenOn) {
    setpoint_x2++;
    // 限制在范围内
    if (setpoint_x2 < MIN_TEMP * 2) setpoint_x2 = MIN_TEMP * 2;
    if (setpoint_x2 > MAX_TEMP * 2) setpoint_x2 = MAX_TEMP * 2;
    // 保存到 Flash
    needSave = true;  // 标记需要保存
  }
  turnDisplayOnNow();
}
void buttonClickDown() {
  if (LOGON) Serial.println("Downp单击");
  if (screenOn) {
    setpoint_x2--;
    // 限制在范围内
    if (setpoint_x2 < MIN_TEMP * 2) setpoint_x2 = MIN_TEMP * 2;
    if (setpoint_x2 > MAX_TEMP * 2) setpoint_x2 = MAX_TEMP * 2;
    // 保存到 Flash
    needSave = true;  // 标记需要保存
  }
  turnDisplayOnNow();
}

// 长按快速+1°C
void buttonUpLongPress() {
  static unsigned long lastRepeat = 0;
  if (millis() - lastRepeat > 200) {  // 每100ms+1°C
    turnDisplayOnNow();
    setpoint_x2++;
    // 限制在范围内
    if (setpoint_x2 < MIN_TEMP * 2) setpoint_x2 = MIN_TEMP * 2;
    if (setpoint_x2 > MAX_TEMP * 2) setpoint_x2 = MAX_TEMP * 2;
    if (LOGON) Serial.print("设定温度x2: ");
    if (LOGON) Serial.println(setpoint_x2);
    lastRepeat = millis();
  }
}

// 长按快速-1°C
void buttonDownLongPress() {

  static unsigned long lastRepeat = 0;
  if (millis() - lastRepeat > 200) {
    turnDisplayOnNow();
    setpoint_x2--;
    // 限制在范围内
    if (setpoint_x2 < MIN_TEMP * 2) setpoint_x2 = MIN_TEMP * 2;
    if (setpoint_x2 > MAX_TEMP * 2) setpoint_x2 = MAX_TEMP * 2;
    if (LOGON) Serial.print("设定温度x2: ");
    if (LOGON) Serial.println(setpoint_x2);
    lastRepeat = millis();
  }
}

// ---------------- 打印调试 ----------------
void PrintLog(const char* on, const char* reason) {
  if (LOGON) Serial.println(
    "Test\n"
    + String("Tine/ms: ") + millis() + "\n"
    + String("act: ") + on + "\n"
    + String("原因: ") + reason + "\n"
    + String("Temp: ") + ((float)setpoint_x2 / 2.0f) + "C\n"
    + String("NTC: ") + (isnan(lastNTCTemp) ? "Na" : String(lastNTCTemp, 2)) + "\n"
    + String("DS: ") + (isnan(lastDSTemp) ? "Na" : String(lastDSTemp, 2)) + "\n"
    + String("ErrSt: ") + (sensorFault ? "Err" : "Ok") + "\n" + "=");
}

void DS18B20Read(unsigned long now) {
  // ---------------- DS18B20 非阻塞读取 ----------------
  if (!dsRequestPending && now - lastDSRequest >= 5000) {
    sensors.requestTemperatures();  // 发起转换
    // if (LOGON) Serial.println("发起转换");
    dsRequestPending = true;
    lastDSRequest = now;
  }
  if (dsRequestPending && now - lastDSRequest >= 1000) {
    float t = sensors.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C) {
      lastDSTemp = NAN;
      if (LOGON) Serial.println("DS18B20 未连接，尝试恢复...");
      sensors.begin();  // 再扫描一次
    }
    if (t > 80.0f || t < -50.0f) {
      if (LOGON) Serial.println("温度或传感器异常 忽略");
      // 上电时 DS18B20 默认85℃ 忽略
      lastDSTemp = NAN;
    } else lastDSTemp = t;
    dsRequestPending = false;
    if (LOGON) Serial.print("lastDSTemp:");
    if (LOGON) Serial.print(lastDSTemp);
    if (LOGON) Serial.print("-compressorOn:");
    if (LOGON) Serial.print(compressorOn);
    if (LOGON) Serial.print("-needOff:");
    if (LOGON) Serial.print(needOff);
    if (LOGON) Serial.print("-needOn:");
    if (LOGON) Serial.print(needOn);
    if (LOGON) Serial.print("-lastNTCTemp:");
    if (LOGON) Serial.println(lastNTCTemp);
  }
}

// ---------------- setup ----------------
void setup() {
  if (LOGON) Serial.begin(115200);

  // 开机读取 NVS
  prefs.begin("fridge", false);
  setpoint_x2 = prefs.getInt("setpoint_x2", DEFAULT_TEMP * 2);  // 默认 -10°C
  lastSavedSetpoint_x2 = setpoint_x2;
  prefs.end();

  // 引脚模式配置
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(NTC_PIN, INPUT);
  // pinMode(EC_SW, INPUT_PULLUP);
  pinMode(SWA, INPUT);
  pinMode(SWB, INPUT);

  setRelayPhysical(true);  // 上电默认开启压缩机
  PrintLog("无", "上电默认开启压缩机");

  // 传感器 & OLED 初始化
  sensors.begin();
  sensors.setWaitForConversion(false);  // 关键：不等待
  sensors.requestTemperatures();        // 启动第一次转换
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    while (1) delay(1000);
  display.clearDisplay();
  display.display();

  // lastEncPos = encoder.read();
  lastInteraction = millis();
  screenOn = true;
  display.ssd1306_command(SSD1306_DISPLAYON);
  updateOLED(NAN, NAN);

  lastDSRequest = millis();
  dsRequestPending = false;

  // 初始化按钮引脚
  button1.attachClick([]() {
    buttonClickUp();
  });
  // 初始化按钮引脚
  button2.attachClick([]() {
    buttonClickDown();
  });
  // 长按快速+1°C
  button1.attachDuringLongPress([]() {
    buttonUpLongPress();
  });

  // 长按快速-1°C
  button2.attachDuringLongPress([]() {
    buttonDownLongPress();
  });
  // 长按松开后保存
  button1.attachLongPressStop([]() {
    // 保存到 Flash
    needSave = true;  // 标记需要保存
  });
  button2.attachLongPressStop([]() {
    // 保存到 Flash
    needSave = true;  // 标记需要保存
  });
}

// ---------------- loop ----------------
void loop() {
  static unsigned long lastNTCRead = 0;
  unsigned long now = millis();
  // 更新按钮状态
  button1.tick();
  button2.tick();


  // 只有温度设定真正变化且未保存才写入 NVS
  if (needSave && setpoint_x2 != lastSavedSetpoint_x2) {
    needSave = false;
    saveSetpoint();
  }

  // 更新温度传感器状态
  DS18B20Read(now);

  // ---------------- NTC 读取 ----------------
  // 读取新的温度值
  if (now - lastNTCRead >= 500) {  // 每500ms读一次
    updateFanLogic();              // 控制风扇开关
    float newTemp = readNTCTempC();
    if (!isnan(newTemp)) {
      lastNTCTemp = newTemp;
      NTC_ERR_COUNT = 0;
    } else {
      lastNTCTemp = NAN;
      NTC_ERR_COUNT++;
    }
    lastNTCRead = millis();
  }

  // ---------------- 故障检测 ----------------
  if (NTC_ERR_COUNT >= 10) {
    // NTC 失效
    sensorFault = true;
    setRelayPhysical(false);  // 故障停机
    PrintLog("关机", "传感器失效，故障停机");

  } else {
    // 故障恢复
    if (sensorFault) {
      sensorFault = false;
      PrintLog("开机", "故障恢复，强制开机");
    }
    faultStartMillis = 0;

    // ---------------- 温控逻辑 ----------------

    float sp = (float)setpoint_x2 / 2.0f;  // 设定温度
    // 👉 计算 设定温度（目标温度）。
    // 这里 setpoint_x2 是整型 *2 的值（可能是因为用了整数存储 0.5°C 分辨率）。
    // 除以 2.0 得到实际设定温度。
    float hyst = (float)HYSTERESIS_X2 / 2.0f;  // 回差
    // 计算 温度回差（hysteresis）。
    // 避免温度在临界值时，继电器频繁开关（抖动）。
    if (lastNTCTemp > sp + hyst) {
      setRelayPhysical(true);
      PrintLog("开机", "满足条件 → 开启压缩机");
    }
    if (lastNTCTemp < sp - hyst) {
      setRelayPhysical(false);
      PrintLog("关机", "满足条件 → 关闭压缩机");
    }
  }

  // ---------------- OLED 控制 ----------------
  if (millis() - lastInteraction >= SCREEN_TIMEOUT) turnDisplayOffNow();
  static unsigned long lastOLEDUpdate = 0;
  if (screenOn && now - lastOLEDUpdate >= 500) {
    updateOLED(lastDSTemp, lastNTCTemp);
    lastOLEDUpdate = now;
  }

  // ---------------- 日志 控制 ----------------
  static unsigned long lastLOGUpdate = 0;
  if (LOGON && now - lastLOGUpdate >= LOGON_MS) {
    PrintLog("无", "日志定时打印");
    lastLOGUpdate = now;
  }
}
