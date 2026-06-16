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
