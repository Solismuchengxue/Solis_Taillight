# Solis 露营车尾灯固件

基于 **ESP32-C3 SuperMini** 控制 **WS2812B** 灯条的遥控露营车尾灯固件。自研轻量方案（替代过重的 WLED），带 Web 控制台、WiFi 配网、OTA 升级，以及与 RC 接收机联动的转向/刹车灯。

## 功能

- **灯效引擎**：7 种常规灯效（纯色/呼吸/流动彩虹/扫描/闪烁/随机闪/彗星），颜色、速度、亮度可调
- **预设列表 + 上电轮换**：可存多个预设，开启轮换后每次上电切换下一个；关闭则用固定默认预设
- **Web 控制台**：实时预览灯效、管理预设、改灯条参数、配 WiFi/AP、RC 标定，单页应用存于 LittleFS
- **WiFi 配网**：上电自动连已存 WiFi；连不上回退 AP 热点配网（AP 名 / 回退行为可配）
- **OTA 升级**：浏览器上传 `.bin`，或 PlatformIO 无线烧录（espota）
- **灯条引脚 / 数量可配**：Web 即可改，无需重编译
- **RC 转向/刹车联动**（同一灯条复用，优先级高于常规灯效）：
  - **刹车**：复用油门通道，电平实时判定（踩刹车亮满红）
  - **转向**：两个独立通道，遥控器点动模式，按一下转向灯效保持设定时长（默认 3 秒）后自停

## 硬件接线

主控：ESP32-C3 SuperMini（4MB flash，原生 USB-CDC），COM6 连电脑烧录。

| 信号 | 引脚 | 说明 |
|------|------|------|
| WS2812B 数据 | **GPIO3** | 6 颗 5050，颜色顺序 GRB |
| RC 刹车（油门通道） | **GPIO4** | 串 1kΩ 进脚 |
| RC 左转（独立通道） | **GPIO5** | 串 1kΩ 进脚 |
| RC 右转（独立通道） | **GPIO6** | 串 1kΩ 进脚 |

避开引脚：GPIO2 / GPIO8 / GPIO9（strapping / 板载 LED / BOOT）。

**供电与共地**：锂电池 → LM2596HV（非隔离 buck）→ 5V，给 ESP32-C3、灯条、接收机供电。
LM2596HV 输出 GND = 输入 GND = 电池/主板 GND，接收机、主板、ESP32 全部共地。
> 接收机 VCC 只接一路 5V，勿同时接主板 5V 与 LM2596HV（会倒灌）。

**RC 信号电平**：HOTRC F-06A 接收机，50Hz 模拟模式，脉宽 1000–2000μs。
实测信号峰值约 3.5V（3.3V 级，用 BAT85+1μF 峰值检波测得），可直连 C3，串 1kΩ 作保险。
> 注意：万用表 DC 档读到的是脉冲**平均值**，不是峰值；测峰值须用峰值检波或示波器。

## 编译与烧录

需要 [PlatformIO](https://platformio.org/)（本机 Core 6.1.19，位于 `D:\PlatformIO_Core`）。

```bash
# 编译
pio run

# USB 烧录固件（COM6）
pio run -t upload

# 烧录 Web 页面到 LittleFS（改了 data/ 后需要）
pio run -t uploadfs

# 无线烧录固件（设备已连局域网时；需防火墙放行 espota）
pio run -e ota -t upload
```

浏览器 OTA：控制台「系统」页选 `.bin` 上传即可（局域网或 AP 模式均可）。

## 使用

1. 首次上电无 WiFi → 开 AP 热点 **`Solis-Taillight`**，浏览器进 **http://192.168.0.1**
2. 「系统」页填 WiFi → 保存重启 → 连入路由后用 **http://solis-taillight.local** 或其局域网 IP 访问
3. 「灯效」页调灯效并保存为预设；「预设」页管理列表与上电轮换
4. 「转向/刹车」页配置并标定 RC（见下）

### RC 标定

1. 接好 3 路信号（各串 1kΩ），「转向/刹车」页勾**启用** → 保存
2. 看「实时脉宽监视」的油门/左/右三个 μs 值：
   - 踩油门，看刹车通道掉到多少 → 据此设**刹车阈值**（刹车判定为「中位 − 刹车阈值」）
   - 按左/右转键，看脉宽跳到多少 → **触发阈值**设得比按下值略低
3. 调**保持时长**（默认 3000ms）。直连方案**不要勾**「信号反相」
4. 保存后实测：踩油门→满红；点左/右键→对应转向灯亮设定时长

## REST API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET  | `/api/state` | 全部状态与配置 |
| POST | `/api/led` | 灯条引脚/数量/总亮度 |
| POST | `/api/presets` | 整表替换预设列表 |
| POST | `/api/apply` | 实时预览某灯效（不保存） |
| POST | `/api/rotate` | 上电轮换开关 + 默认预设 |
| POST | `/api/rc` | RC 联动配置 |
| GET  | `/api/rc/live` | RC 实时脉宽（标定用） |
| POST | `/api/ap` | AP 热点名 + 回退开关 |
| POST | `/api/wifi` | WiFi 凭据（保存后重启） |
| POST | `/update` | OTA 固件上传 |
| POST | `/api/reboot` `/api/factory` | 重启 / 恢复出厂 |

## 工程结构

```
Solis_ESP32C3LED/
├─ platformio.ini       # 构建配置（含 ota 无线烧录环境）
├─ src/
│  ├─ main.cpp          # 启动编排 + 主循环
│  ├─ config.h/.cpp     # 配置模型 + NVS 持久化
│  ├─ led_engine.h/.cpp # 灯效引擎（常规灯效 + RC 叠加）
│  ├─ wifi_mgr.h/.cpp   # STA 连接 + AP 回退 + mDNS
│  ├─ web.h/.cpp        # Web 服务 + REST API + OTA
│  └─ rc_input.h/.cpp   # RC 脉宽采集 + 刹车/转向判定
├─ data/                # LittleFS：index.html / app.js / style.css
└─ doc/                 # 主控资料
```
