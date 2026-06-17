# Solis 露营车尾灯固件

基于 **ESP32-C3 SuperMini** 控制 **WS2812B** 灯条的遥控露营车尾灯固件。自研轻量方案（替代过重的 WLED），带中英文 Web 控制台、WiFi 配网、OTA 升级，以及与 RC 接收机联动的转向/刹车/双闪灯。

## 功能

- **灯效引擎**：8 种常规灯效（纯色 / 呼吸 / 流动彩虹 / 扫描 / 闪烁 / 随机闪 / 彗星→ / 彗星←），颜色、速度、亮度可调
- **预设列表 + 上电轮换**：表格式管理最多 12 个预设；开启轮换则每次上电切下一个，关闭用固定默认预设
- **多灯带**：装饰 / 左转 / 右转 / 刹车，最多 4 条灯带，各自可配数据脚与灯珠数量；数据脚相同则共用一条
- **RC 联动**（优先级 刹车 > 双闪 > 转向 > 装饰，均为交通标准配色）：
  - **刹车**：复用油门通道，电平实时；**红色常亮**；判定方向可反向（适配杆量正/反）
  - **转向**：左/右各自独立灯带与触发通道；**琥珀色流水**；仅点动（按一下保持设定时长后自停）；流水方向各自可反向
  - **双闪**：锁存式（触发进入并保持，再次触发取消）；触发方式可选「左右同时」/「双击任意转向」/「关闭」
- **Web 控制台**：中英文切换、实时预览、预设表、配置页一键保存、RC 实时脉宽标定，单页应用存于 LittleFS
- **WiFi 配网**：上电自动连已存 WiFi；连不上回退 AP 热点（AP 名 / 回退可配，AP 固定 192.168.0.1）
- **OTA 升级**：浏览器上传 `.bin`，或 PlatformIO 无线烧录（espota）

## 硬件接线

主控：ESP32-C3 SuperMini（4MB flash，原生 USB-CDC），COM6 连电脑烧录。
接线图见 [doc/wiring.svg](doc/wiring.svg)。

| 信号 | 默认引脚 | 说明 |
|------|------|------|
| WS2812B 数据（装饰/左/右/刹车） | **GPIO3** | 默认共用一条 6 颗；可在 Web 各改独立 GPIO/数量 |
| RC 刹车（油门通道） | **GPIO5** | 串 1kΩ 进脚 |
| RC 左转触发通道 | **GPIO6** | 串 1kΩ 进脚 |
| RC 右转触发通道 | **GPIO7** | 串 1kΩ 进脚 |

避开引脚：GPIO2 / GPIO8 / GPIO9（strapping / 板载 LED / BOOT）。

**供电与共地**：锂电池 → LM2596HV（非隔离 buck）→ 5V，给 ESP32-C3、灯条、接收机供电。
LM2596HV 输出 GND = 输入 GND = 电池/主板 GND，接收机、主板、ESP32 **全部共地**。
> 接收机 VCC 只接一路 5V，勿同时接主板 5V 与 LM2596HV（会倒灌）。

**RC 信号电平**：HOTRC F-06A 接收机，50Hz 模拟模式，脉宽 1000–2000μs。
实测信号峰值约 3.5V（3.3V 级，用 BAT85+1μF 峰值检波测得），可直连 C3，串 1kΩ 作保险。
RC 信号同时送往 hoverboard 主板（GD32）驱动电机，本固件并联读取。
> 注意：万用表 DC 档读到的是脉冲**平均值**，不是峰值；测峰值须用峰值检波或示波器。

## 编译与烧录

需要 [PlatformIO](https://platformio.org/)（本机 Core 6.1.19，位于 `D:\PlatformIO_Core`）。

```bash
pio run                      # 编译
pio run -t upload            # USB 烧录固件 (COM6)
pio run -t uploadfs          # 烧录 Web 页面到 LittleFS (改了 data/ 后)
pio run -e ota -t upload     # 无线烧录固件 (设备在局域网; 需防火墙放行 espota)
```

浏览器 OTA：控制台「系统」页选 `.bin` 上传（局域网或 AP 模式均可）。

> 本地 WiFi 种子：可选放 `src/secrets.h`（已 git 忽略）写 `SEED_WIFI_SSID`/`SEED_WIFI_PASS`，设备未配网时自动种入 NVS；凭据不入源码/固件。

## 使用

1. 首次上电无 WiFi → 开 AP 热点 **`Solis-Taillight`**，浏览器进 **http://192.168.0.1**
2. 「系统」页填 WiFi → 保存重启 → 连入路由后用 **http://solis-taillight.local** 或局域网 IP 访问
   （路由器中设备名显示为 `SolisTaillight-<MAC后3字节>`）
3. 「灯效」页：表格里编辑预设（名称/灯效/颜色/速度/亮度），点该行「预览」实时看，点「保存预设列表」写入
4. 「配置」页：装饰灯 / 左转 / 右转 / 双闪 / 刹车，改完点底部唯一的「保存配置」（重启生效）；最下方是实时脉宽监视
5. 右上角按钮切换中英文

### RC 标定

1. 接好 3 路信号（各串 1kΩ）；「配置」页启用对应功能
2. 看页面最下「实时脉宽监视」的油门/左/右三个 μs 值：
   - 踩油门看刹车通道值 → 设**中位 / 刹车阈值**（刹车在中位某一侧，方向不对就勾「刹车方向反向」）
   - 按左/右转键看脉宽 → **触发阈值**设得比按下值略低
3. 转向流水方向不对就勾对应的「灯效反向」；选好双闪触发方式
4. 点「保存配置」，实拨遥控器验证

## REST API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET  | `/api/state` | 全部状态与配置 |
| POST | `/api/led` | 装饰灯数据脚 / 数量 |
| POST | `/api/presets` | 整表替换预设列表 |
| POST | `/api/apply` | 实时预览某灯效（不保存） |
| POST | `/api/rotate` | 上电轮换开关 + 默认预设 |
| POST | `/api/rc` | RC 联动配置（刹车 + 双闪 + left/right 嵌套对象） |
| GET  | `/api/rc/live` | RC 实时脉宽（标定用） |
| POST | `/api/ap` | AP 热点名 + 回退开关 |
| POST | `/api/wifi` | WiFi 凭据（保存后重启） |
| POST | `/update` | OTA 固件上传 |
| POST | `/api/reboot` `/api/factory` | 重启 / 恢复出厂 |

## 工程结构

```
Solis_Taillight/
├─ platformio.ini       # 构建配置 (含 ota 无线烧录环境)
├─ src/
│  ├─ main.cpp          # 启动编排 + 主循环 (含可选 secrets.h 种子)
│  ├─ config.h/.cpp     # 配置模型 + NVS 持久化
│  ├─ led_engine.h/.cpp # 多灯带灯效引擎 (常规灯效 + 刹车/转向/双闪)
│  ├─ wifi_mgr.h/.cpp   # STA 连接 + AP 回退 + mDNS + DHCP 主机名
│  ├─ web.h/.cpp        # Web 服务 + REST API + OTA + 延迟重启
│  └─ rc_input.h/.cpp   # RC 脉宽中断采集 + 刹车/转向/双闪判定
├─ data/                # LittleFS: index.html / app.js / style.css / favicon.svg
└─ doc/                 # 主控资料 + 接线图 wiring.svg
```
