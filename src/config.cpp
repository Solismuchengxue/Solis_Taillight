#include "config.h"
#include <Preferences.h>

Config cfg;
static Preferences prefs;

static const char* NS = "solis";   // NVS namespace
static const char* KEY = "cfg";    // 单 blob 存整个 Config

// ---- 出厂默认 ----
static void setDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.ledPin   = 3;
  cfg.ledCount = 6;
  cfg.brightness = 128;

  // 几个开箱即用的预设
  auto mk = [](Preset& p, const char* n, EffectId fx,
               uint8_t r, uint8_t g, uint8_t b, uint8_t spd, uint8_t br) {
    strncpy(p.name, n, sizeof(p.name) - 1);
    p.name[sizeof(p.name) - 1] = 0;
    p.effect = fx; p.r = r; p.g = g; p.b = b; p.speed = spd; p.brightness = br;
  };
  mk(cfg.presets[0], "Red Solid",  FX_SOLID,   255, 0,   0,   0,   200);
  mk(cfg.presets[1], "Breathe",    FX_BREATHE, 255, 40,  0,   80,  200);
  mk(cfg.presets[2], "Rainbow",    FX_RAINBOW, 0,   0,   0,   100, 180);
  mk(cfg.presets[3], "Scanner",    FX_SCANNER, 255, 0,   0,   120, 200);
  mk(cfg.presets[4], "Sparkle",    FX_SPARKLE, 255, 255, 255, 90,  180);
  cfg.presetCount   = 5;
  cfg.defaultPreset = 0;
  cfg.rotateOnBoot  = false;
  cfg.currentPreset = 0;

  // RC 默认 (停用, 引脚预置为规划值; 直连不反相)
  cfg.rc.brakeEnabled  = false;
  cfg.rc.brakeReverse  = false;   // 默认油门低于中位判刹车
  cfg.rc.brakePin      = 4;       // 油门通道(刹车)
  cfg.rc.centerUs      = 1500;
  cfg.rc.brakeUs       = 250;     // 油门低于 1250us 判刹车
  cfg.rc.brakeLedPin   = 3;       // 默认与装饰灯共用 GPIO3
  cfg.rc.brakeLedCount = 6;
  cfg.rc.brakePreset   = 0;
  cfg.rc.turnEnabled   = false;
  cfg.rc.turnReverse   = false;
  cfg.rc.turnMode      = TURN_MOMENTARY;
  cfg.rc.leftPin       = 5;       // 左转独立通道
  cfg.rc.rightPin      = 6;       // 右转独立通道
  cfg.rc.turnTriggerUs = 1700;    // 脉宽 >1700us 视为按下
  cfg.rc.turnHoldMs    = 3000;    // 点动触发后保持 3 秒
  cfg.rc.turnLedPin    = 3;       // 默认与装饰灯共用 GPIO3
  cfg.rc.turnLedCount  = 6;
  cfg.rc.turnPreset    = 0;

  cfg.wifiSsid[0] = 0;
  cfg.wifiPass[0] = 0;

  strncpy(cfg.apSsid, "Solis-Taillight", sizeof(cfg.apSsid) - 1);
  cfg.apFallback = true;
}

void ConfigStore::load() {
  prefs.begin(NS, true);                  // 只读
  size_t n = prefs.getBytesLength(KEY);
  if (n == sizeof(Config)) {
    prefs.getBytes(KEY, &cfg, sizeof(Config));
  } else {
    prefs.end();
    setDefaults();
    save();
    return;
  }
  prefs.end();
}

void ConfigStore::save() {
  prefs.begin(NS, false);                 // 读写
  prefs.putBytes(KEY, &cfg, sizeof(Config));
  prefs.end();
}

void ConfigStore::factoryReset() {
  setDefaults();
  save();
}

void ConfigStore::begin() {
  load();
}
