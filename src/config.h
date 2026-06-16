// 配置数据模型 + NVS 持久化
// 所有可在 Web 页面修改的参数都集中在这里, 重启后由 NVS 恢复
#pragma once
#include <Arduino.h>

// 灯效类型 (常规灯效, 不含 RC 叠加灯效)
enum EffectId : uint8_t {
  FX_SOLID = 0,   // 纯色常亮
  FX_BREATHE,     // 呼吸
  FX_RAINBOW,     // 流动彩虹
  FX_SCANNER,     // 来回扫描 (KITT)
  FX_BLINK,       // 闪烁
  FX_SPARKLE,     // 随机闪烁
  FX_COMET,       // 彗星拖尾
  FX_COUNT        // 灯效总数 (放最后)
};

// 一个预设 = 一种灯效及其参数
struct Preset {
  char     name[16];
  uint8_t  effect;          // EffectId
  uint8_t  r, g, b;         // 主色
  uint8_t  speed;           // 0-255, 灯效快慢
  uint8_t  brightness;      // 0-255, 该预设的亮度
};

#define MAX_PRESETS 12
#define MAX_LEDS    300       // 灯珠数量上限 (静态缓冲), 实际用 ledCount

// RC 转向/刹车联动配置
// 刹车: 复用油门通道(电平实时判定)。转向: 两个独立通道, 点动触发后保持 turnHoldMs。
struct RcConfig {
  bool     enabled;         // 是否启用 RC 联动
  bool     invert;          // 信号是否反相 (直连=false; 反相缓冲=true)
  uint8_t  brakePin;        // 刹车: 油门通道输入脚
  uint16_t centerUs;        // 油门中位脉宽, 通常 1500
  uint16_t brakeUs;         // 油门脉宽低于(中位-此值)判为刹车
  uint8_t  leftPin;         // 左转: 独立通道输入脚 (点动)
  uint8_t  rightPin;        // 右转: 独立通道输入脚 (点动)
  uint16_t turnTriggerUs;   // 脉宽高于此值视为"按下"触发
  uint16_t turnHoldMs;      // 触发后转向灯效保持时长 (默认 3000)
};

// 全局配置
struct Config {
  uint8_t  ledPin;          // WS2812 数据脚
  uint16_t ledCount;        // 灯珠数量
  uint8_t  brightness;      // 全局总亮度上限 0-255

  Preset   presets[MAX_PRESETS];
  uint8_t  presetCount;
  uint8_t  defaultPreset;   // 不轮换时使用的预设索引
  bool     rotateOnBoot;    // 上电轮换开关
  uint8_t  currentPreset;   // 当前(上次)预设索引, 轮换时上电+1

  RcConfig rc;

  char     wifiSsid[33];
  char     wifiPass[65];

  char     apSsid[33];      // AP 配网热点名称
  bool     apFallback;      // 已存WiFi但连不上时, 是否回退开AP
};

extern Config cfg;

namespace ConfigStore {
  void begin();        // 挂载 NVS, 读出配置; 若空则写入出厂默认
  void load();         // 从 NVS 读
  void save();         // 写回 NVS
  void factoryReset(); // 恢复出厂默认并保存
}
