# 🌡️ Smart Environment Monitor — ESP32-S3

基于 **ESP32-S3 + ESP-IDF + FreeRTOS** 的智能环境监测系统，集成多传感器采集、MQTT 云端上报、OLED 本地显示及低功耗管理。

## ✨ 功能特性

| 功能 | 说明 |
|------|------|
| 🌡️ 温湿度采集 | DHT11 单总线协议，实时读取温度/湿度 |
| 💡 光照强度 | BH1750FVI I2C 数字光照传感器，1-65535 lux |
| 🌫️ 空气质量 | MQ-135 模拟传感器 ADC 采集，0-100% AQI |
| 📡 MQTT 上报 | 通过 MQTT 协议接入阿里云 IoT 平台，JSON 格式上报 |
| 📺 OLED 显示 | SSD1306 128×64 I2C 显示屏，实时显示传感器数据 |
| 🔋 低功耗 | ESP32 Light-sleep 模式，待机功耗从 ~80mA 降至 ~0.8mA |
| 🎮 远程控制 | 支持阿里云下发指令：修改采集间隔、进入休眠、重启等 |
| 🧵 多任务调度 | FreeRTOS 优先级抢占 + 双核绑定 + 队列通信 |

## 🏗️ 系统架构

```
┌──────────────────┐   sensor_queue   ┌───────────────────┐
│   sensor_task    │ ───────────────> │   upload_task      │
│   (Core 1, Pri5) │                  │   (Core 0, Pri4)   │
│   DHT11 / BH1750 │   cmd_queue      │   MQTT → 阿里云    │
│   MQ135          │ <─────────────── │   解析远程指令      │
└──────────────────┘                  └───────────────────┘
        │
        │ sensor_queue (共享)
        ▼
┌──────────────────┐                  ┌───────────────────┐
│   display_task   │                  │   power_task       │
│   (Core 1, Pri3) │                  │   (Core 0, Pri2)   │
│   SSD1306 OLED   │                  │   Light-sleep 管理  │
└──────────────────┘                  └───────────────────┘

互斥锁: g_i2c_bus_mutex → 保护 BH1750 + SSD1306 共享 I2C 总线
```

## 📁 项目结构

```
esp32-smart-env-monitor/
├── CMakeLists.txt                    # 顶层 CMake（注册 components）
├── sdkconfig.defaults                # 默认 SDK 配置
├── partitions.csv                    # 自定义分区表
├── README.md
├── main/
│   ├── CMakeLists.txt
│   ├── main.c                        # 入口：初始化 → 启动任务
│   ├── smart_env_config.h            # 全局配置（引脚、凭据、间隔）
│   ├── task_comm.c/h                 # 队列 + 互斥锁定义
│   ├── wifi_manager.c/h              # Wi-Fi STA 连接管理
│   ├── aliyun_mqtt_client.c/h        # 阿里云 MQTT 客户端
│   ├── sensor_task.c/h               # 传感器采集任务
│   ├── upload_task.c/h               # 网络上传任务
│   ├── display_task.c/h              # OLED 显示任务
│   └── power_manager.c/h             # 低功耗管理
├── components/
│   ├── dht11/                        # DHT11 驱动（GPIO 单总线）
│   ├── bh1750/                       # BH1750 驱动（I2C）
│   ├── mq135/                        # MQ135 驱动（ADC）
│   └── ssd1306/                      # SSD1306 OLED 驱动（I2C）
└── docs/
    └── wiring.md                     # 接线说明
```

## 🔌 硬件接线

### I2C 总线（BH1750 + SSD1306 共享）

| ESP32-S3 Pin | BH1750 | SSD1306 OLED |
|:---:|:---:|:---:|
| GPIO21 (SDA) | SDA | SDA |
| GPIO22 (SCL) | SCL | SCL |
| 3.3V | VCC | VCC |
| GND | GND | GND |

> BH1750 I2C 地址: `0x23`（ADDR 引脚接 GND）
> SSD1306 I2C 地址: `0x3C`

### DHT11（GPIO 单总线）

| ESP32-S3 Pin | DHT11 |
|:---:|:---:|
| GPIO4 | DATA |
| 3.3V | VCC |
| GND | GND |

> DHT11 DATA 引脚需要 4.7KΩ 上拉电阻到 3.3V

### MQ-135（ADC）

| ESP32-S3 Pin | MQ-135 |
|:---:|:---:|
| GPIO1 (ADC1_CH0) | AOUT |
| 5V | VCC |
| GND | GND |

> MQ-135 需要预热 24-48 小时才能获得准确读数

### 可选：OLED 显示

| ESP32-S3 Pin | SSD1306 |
|:---:|:---:|
| GPIO21 | SDA |
| GPIO22 | SCL |
| 3.3V | VCC |
| GND | GND |

## 🚀 快速开始

### 1. 环境准备

```bash
# 安装 ESP-IDF v5.x (如未安装)
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/

# 克隆项目
git clone https://github.com/YOUR_USERNAME/esp32-smart-env-monitor.git
cd esp32-smart-env-monitor
```

### 2. 配置设备参数

编辑 `main/smart_env_config.h`：

```c
/* Wi-Fi 配置 */
#define WIFI_SSID           "你的WiFi名称"
#define WIFI_PASSWORD       "你的WiFi密码"

/* 阿里云 IoT 三元组 */
#define ALIYUN_PRODUCT_KEY    "你的ProductKey"
#define ALIYUN_DEVICE_NAME    "你的DeviceName"
#define ALIYUN_DEVICE_SECRET  "你的DeviceSecret"
```

### 3. 编译烧录

```bash
# 设置目标芯片
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录（连接 USB，确认串口号）
idf.py -p /dev/ttyUSB0 flash

# 监视串口输出
idf.py -p /dev/ttyUSB0 monitor
```

### 4. 阿里云 IoT 平台配置

1. 登录 [阿里云 IoT 控制台](https://iot.console.aliyun.com/)
2. 创建产品 → 添加设备 → 获取三元组
3. 定义物模型属性：`temperature`, `humidity`, `light`, `airQuality`
4. 在设备详情页查看数据上报

## 📊 数据上报格式 (MQTT JSON)

```json
{
  "id": "esp32_A1B2C3D4",
  "version": "1.0",
  "method": "thing.event.property.post",
  "params": {
    "temperature": 25.6,
    "humidity": 62.3,
    "light": 456.7,
    "airQuality": 35,
    "timestamp": 12345
  }
}
```

## 🎮 远程指令

通过阿里云 IoT 下发指令：

| 指令 | 格式 | 说明 |
|------|------|------|
| 设置采集间隔 | `{"setInterval": {"value": 5000}}` | 修改采集间隔(ms) |
| 进入休眠 | `{"enterSleep": {}}` | 立即进入 Light-sleep |
| 设备重启 | `{"reset": {}}` | 软件复位 |
| LED 开/关 | `{"ledOn": {}}` / `{"ledOff": {}}` | 控制 LED |

## ⚡ FreeRTOS 任务分配

| 任务 | 核心 | 优先级 | 栈大小 | 说明 |
|:---:|:---:|:---:|:---:|------|
| sensor_task | Core 1 | 5 (最高) | 4KB | 传感器采集，实时性要求高 |
| upload_task | Core 0 | 4 | 8KB | MQTT 上传，依赖网络栈 |
| display_task | Core 1 | 3 | 4KB | OLED 刷新，共享 I2C |
| power_task | Core 0 | 2 (最低) | 2KB | 空闲监控，Light-sleep 管理 |

### 任务间通信

- **Queue（队列）**: `sensor_task → upload_task / display_task`
  - 传感器数据通过队列传递，满时丢弃最旧数据
- **Queue（队列）**: `upload_task → sensor_task`
  - 远程指令通过队列传递给传感器任务
- **Mutex（互斥锁）**: 保护 I2C 总线
  - BH1750 和 SSD1306 共享 I2C 总线，互斥锁防止竞争

## 🔋 低功耗管理

```
正常运行 (~80mA) → 空闲 30s → Light-sleep (~0.8mA)
                                ├── Timer 唤醒 (30s)
                                └── GPIO 唤醒 (BOOT 按键)
```

启用 `CONFIG_PM_ENABLE` 后，ESP-IDF 自动管理 CPU 频率：
- 活跃时: 240 MHz
- 空闲时: 40 MHz + Light-sleep

## 🔧 自定义配置

所有可配置项在 `main/smart_env_config.h` 中：

- 引脚分配
- Wi-Fi / MQTT 凭据
- 采集间隔
- 任务优先级和栈大小
- 队列深度
- 低功耗超时时间

## 📚 技术要点

1. **双核调度**: sensor_task 绑定 Core 1（实时采集），upload_task 绑定 Core 0（Wi-Fi 栈）
2. **互斥锁**: I2C 总线共享资源通过 `xSemaphoreTake/Give` 保护
3. **队列通信**: 任务间零耦合，通过 `xQueueSend/Receive` 传递数据
4. **HMAC-SHA256**: 阿里云 MQTT 认证密码计算
5. **ESP Timer**: DHT11 微秒级时序控制
6. **ADC OneShot**: MQ135 单次采样模式
7. **Light-sleep**: RTC 外设保持 + 定时器/GPIO 唤醒

## 📄 License

MIT License
