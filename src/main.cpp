// Solis ESP32-C3 LED 露营车尾灯固件
// 阶段3: + WiFi 配网 + Web 控制台 + OTA
// 后续叠加: RC 联动

#include <Arduino.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "led_engine.h"
#include "wifi_mgr.h"
#include "web.h"
#include "rc_input.h"

// 可选: 本地 WiFi 种子凭据 (src/secrets.h, 已 git 忽略, 不入库)
#if defined(__has_include)
#  if __has_include("secrets.h")
#    include "secrets.h"
#  endif
#endif

static void selectBootPreset() {
  if (cfg.presetCount == 0) return;
  uint8_t idx;
  if (cfg.rotateOnBoot) {
    idx = (cfg.currentPreset + 1) % cfg.presetCount;
    cfg.currentPreset = idx;
    ConfigStore::save();
  } else {
    idx = min<uint8_t>(cfg.defaultPreset, cfg.presetCount - 1);
  }
  LedEngine::applyPreset(cfg.presets[idx]);
  Serial.printf("[Solis] boot preset #%u \"%s\" (rotate=%d)\n",
                idx, cfg.presets[idx].name, cfg.rotateOnBoot);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n[Solis] boot: stage3 wifi+web+ota"));

  ConfigStore::begin();

#ifdef SEED_WIFI_SSID
  // 仅当尚未配网时, 用本地 secrets.h 的凭据种入 NVS (凭据不在固件源码/git 中)
  if (cfg.wifiSsid[0] == 0) {
    strncpy(cfg.wifiSsid, SEED_WIFI_SSID, sizeof(cfg.wifiSsid) - 1);
    strncpy(cfg.wifiPass, SEED_WIFI_PASS, sizeof(cfg.wifiPass) - 1);
    ConfigStore::save();
    Serial.println("[WiFi] seeded credentials from secrets.h");
  }
#endif

  LedEngine::begin();
  selectBootPreset();

  WifiMgr::begin();
  Web::begin();
  RcInput::begin();

  // 开发端无线烧录 (PlatformIO: upload_protocol=espota, upload_port=solis.local)
  ArduinoOTA.setHostname("Solis-Taillight");
  ArduinoOTA.begin();
  Serial.println("[OTA] ArduinoOTA ready (solis.local)");
}

void loop() {
  ArduinoOTA.handle();
  RcInput::loop();
  LedEngine::loop();
  WifiMgr::loop();
  Web::loop();
}
