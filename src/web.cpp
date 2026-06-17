#include "web.h"
#include "config.h"
#include "led_engine.h"
#include "wifi_mgr.h"
#include "rc_input.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>

static AsyncWebServer server(80);

#define FW_VERSION (__DATE__ " " __TIME__)

// 延迟重启: 先让 HTTP 响应发完再重启, 否则连接被打断客户端收不到结果
static volatile uint32_t g_rebootAt = 0;
static void scheduleReboot() { g_rebootAt = millis() + 800; }

void Web::loop() {
  if (g_rebootAt && (int32_t)(millis() - g_rebootAt) >= 0) {
    g_rebootAt = 0;
    ESP.restart();
  }
}

static const char* effectNames[FX_COUNT] = {
  "Solid", "Breathe", "Rainbow", "Scanner", "Blink", "Sparkle", "Comet→", "Comet←"
};

// ---------- 工具: 整套状态 -> JSON ----------
static void buildState(JsonDocument& d) {
  auto w = d["wifi"].to<JsonObject>();
  w["ap"]         = WifiMgr::isAp();
  w["ip"]         = WifiMgr::ip();
  w["ssid"]       = WifiMgr::ssid();
  w["apSsid"]     = cfg.apSsid;
  w["apFallback"] = cfg.apFallback;

  auto l = d["led"].to<JsonObject>();
  l["pin"]        = cfg.ledPin;
  l["count"]      = cfg.ledCount;
  l["brightness"] = cfg.brightness;

  d["rotateOnBoot"]  = cfg.rotateOnBoot;
  d["defaultPreset"] = cfg.defaultPreset;
  d["currentPreset"] = cfg.currentPreset;

  auto arr = d["presets"].to<JsonArray>();
  for (uint8_t i = 0; i < cfg.presetCount; i++) {
    auto o = arr.add<JsonObject>();
    o["name"]       = cfg.presets[i].name;
    o["effect"]     = cfg.presets[i].effect;
    o["r"]          = cfg.presets[i].r;
    o["g"]          = cfg.presets[i].g;
    o["b"]          = cfg.presets[i].b;
    o["speed"]      = cfg.presets[i].speed;
    o["brightness"] = cfg.presets[i].brightness;
  }

  auto fx = d["effects"].to<JsonArray>();
  for (uint8_t i = 0; i < FX_COUNT; i++) fx.add(effectNames[i]);

  auto rc = d["rc"].to<JsonObject>();
  rc["brakeEnabled"]  = cfg.rc.brakeEnabled;
  rc["brakeReverse"]  = cfg.rc.brakeReverse;
  rc["brakePin"]      = cfg.rc.brakePin;
  rc["brakeLedCount"] = cfg.rc.brakeLedCount;
  rc["centerUs"]      = cfg.rc.centerUs;
  rc["brakeUs"]       = cfg.rc.brakeUs;
  rc["brakeLedPin"]   = cfg.rc.brakeLedPin;
  rc["hazardMode"]    = cfg.rc.hazardMode;

  auto putTurn = [](JsonObject o, const TurnSide& t) {
    o["enabled"]    = t.enabled;
    o["channelPin"] = t.channelPin;
    o["ledPin"]     = t.ledPin;
    o["ledCount"]   = t.ledCount;
    o["mode"]       = t.mode;
    o["triggerUs"]  = t.triggerUs;
    o["holdMs"]     = t.holdMs;
    o["reverse"]    = t.reverse;
  };
  putTurn(rc["left"].to<JsonObject>(),  cfg.rc.left);
  putTurn(rc["right"].to<JsonObject>(), cfg.rc.right);

  d["maxPresets"] = MAX_PRESETS;
  d["maxLeds"]    = MAX_LEDS;
  d["fw"]         = FW_VERSION;
}

static void sendState(AsyncWebServerRequest* req) {
  JsonDocument d;
  buildState(d);
  String out;
  serializeJson(d, out);
  req->send(200, "application/json", out);
}

static void ok(AsyncWebServerRequest* req) {
  req->send(200, "application/json", "{\"ok\":true}");
}

// 解析请求体 JSON 的通用 body handler 生成器
typedef std::function<void(AsyncWebServerRequest*, JsonDocument&)> JsonFn;
static ArBodyHandlerFunction jsonBody(JsonFn fn) {
  return [fn](AsyncWebServerRequest* req, uint8_t* data, size_t len,
              size_t index, size_t total) {
    // 小请求体, 一次到齐即解析
    if (index != 0 || len != total) return;       // (配置项体积都很小)
    JsonDocument d;
    if (deserializeJson(d, data, len)) { req->send(400, "application/json",
        "{\"ok\":false,\"err\":\"bad json\"}"); return; }
    fn(req, d);
  };
}

// ---------- OTA 上传处理 ----------
static void otaUpload(AsyncWebServerRequest* req, const String& filename,
                      size_t index, uint8_t* data, size_t len, bool final) {
  if (index == 0) {
    Serial.printf("[OTA] start: %s\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  }
  if (len) Update.write(data, len);
  if (final) {
    if (Update.end(true)) Serial.printf("[OTA] success %u bytes\n", index + len);
    else                  Update.printError(Serial);
  }
}

void Web::begin() {
  if (!LittleFS.begin(true)) Serial.println("[FS] LittleFS mount failed");

  // --- 状态 ---
  server.on("/api/state", HTTP_GET, sendState);

  // --- 灯条基本设置(引脚/数量/亮度) ---
  server.on("/api/led", HTTP_POST, [](AsyncWebServerRequest* r){}, nullptr,
    jsonBody([](AsyncWebServerRequest* req, JsonDocument& d) {
      if (d["pin"].is<int>())        cfg.ledPin = d["pin"];
      if (d["count"].is<int>())      cfg.ledCount = constrain((int)d["count"], 1, MAX_LEDS);
      if (d["brightness"].is<int>()) cfg.brightness = d["brightness"];
      ConfigStore::save();
      sendState(req);
      scheduleReboot();              // 数据脚/数量变更需重启重建灯带
    }));

  // --- 预设列表(整表替换) ---
  server.on("/api/presets", HTTP_POST, [](AsyncWebServerRequest* r){}, nullptr,
    jsonBody([](AsyncWebServerRequest* req, JsonDocument& d) {
      JsonArray arr = d["presets"].as<JsonArray>();
      if (arr.isNull()) { req->send(400, "application/json",
          "{\"ok\":false,\"err\":\"no presets\"}"); return; }
      uint8_t n = 0;
      for (JsonObject o : arr) {
        if (n >= MAX_PRESETS) break;
        Preset& p = cfg.presets[n];
        strncpy(p.name, o["name"] | "preset", sizeof(p.name) - 1);
        p.name[sizeof(p.name) - 1] = 0;
        p.effect     = o["effect"] | 0;
        p.r          = o["r"] | 0;
        p.g          = o["g"] | 0;
        p.b          = o["b"] | 0;
        p.speed      = o["speed"] | 128;
        p.brightness = o["brightness"] | 200;
        n++;
      }
      cfg.presetCount = n;
      if (cfg.defaultPreset >= n) cfg.defaultPreset = 0;
      ConfigStore::save();
      sendState(req);
    }));

  // --- 实时预览某个预设(不持久, 立刻显示) ---
  server.on("/api/apply", HTTP_POST, [](AsyncWebServerRequest* r){}, nullptr,
    jsonBody([](AsyncWebServerRequest* req, JsonDocument& d) {
      Preset p{};
      strncpy(p.name, "live", sizeof(p.name) - 1);
      p.effect     = d["effect"] | 0;
      p.r          = d["r"] | 0;
      p.g          = d["g"] | 0;
      p.b          = d["b"] | 0;
      p.speed      = d["speed"] | 128;
      p.brightness = d["brightness"] | 200;
      LedEngine::applyPreset(p);     // 预览显示在装饰灯上
      ok(req);
    }));

  // --- 上电轮换设置 ---
  server.on("/api/rotate", HTTP_POST, [](AsyncWebServerRequest* r){}, nullptr,
    jsonBody([](AsyncWebServerRequest* req, JsonDocument& d) {
      if (d["rotateOnBoot"].is<bool>())  cfg.rotateOnBoot  = d["rotateOnBoot"];
      if (d["defaultPreset"].is<int>())  cfg.defaultPreset = d["defaultPreset"];
      ConfigStore::save();
      sendState(req);
    }));

  // --- RC 联动设置 ---
  server.on("/api/rc", HTTP_POST, [](AsyncWebServerRequest* r){}, nullptr,
    jsonBody([](AsyncWebServerRequest* req, JsonDocument& d) {
      RcConfig& rc = cfg.rc;
      if (d["brakeEnabled"].is<bool>())   rc.brakeEnabled = d["brakeEnabled"];
      if (d["brakeReverse"].is<bool>())   rc.brakeReverse = d["brakeReverse"];
      if (d["brakePin"].is<int>())        rc.brakePin = d["brakePin"];
      if (d["brakeLedCount"].is<int>())   rc.brakeLedCount = d["brakeLedCount"];
      if (d["centerUs"].is<int>())        rc.centerUs = d["centerUs"];
      if (d["brakeUs"].is<int>())         rc.brakeUs = d["brakeUs"];
      if (d["brakeLedPin"].is<int>())     rc.brakeLedPin = d["brakeLedPin"];
      if (d["hazardMode"].is<int>())      rc.hazardMode = d["hazardMode"];

      auto getTurn = [](JsonObject o, TurnSide& t) {
        if (o["enabled"].is<bool>())    t.enabled = o["enabled"];
        if (o["channelPin"].is<int>())  t.channelPin = o["channelPin"];
        if (o["ledPin"].is<int>())      t.ledPin = o["ledPin"];
        if (o["ledCount"].is<int>())    t.ledCount = o["ledCount"];
        if (o["mode"].is<int>())        t.mode = o["mode"];
        if (o["triggerUs"].is<int>())   t.triggerUs = o["triggerUs"];
        if (o["holdMs"].is<int>())      t.holdMs = o["holdMs"];
        if (o["reverse"].is<bool>())    t.reverse = o["reverse"];
      };
      if (d["left"].is<JsonObject>())  getTurn(d["left"].as<JsonObject>(),  rc.left);
      if (d["right"].is<JsonObject>()) getTurn(d["right"].as<JsonObject>(), rc.right);
      ConfigStore::save();
      sendState(req);
      scheduleReboot();              // 引脚/灯带变更需重启重建灯带与中断
    }));

  // --- RC 实时脉宽监视 (标定阈值用, 前端轮询) ---
  server.on("/api/rc/live", HTTP_GET, [](AsyncWebServerRequest* req){
    char buf[96];
    snprintf(buf, sizeof(buf),
             "{\"brake\":%u,\"left\":%u,\"right\":%u,\"state\":\"%s\"}",
             RcInput::brakePulse(), RcInput::leftPulse(),
             RcInput::rightPulse(), RcInput::stateName());
    req->send(200, "application/json", buf);
  });

  // --- WiFi 配置(保存后重启生效) ---
  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest* r){}, nullptr,
    jsonBody([](AsyncWebServerRequest* req, JsonDocument& d) {
      strncpy(cfg.wifiSsid, d["ssid"] | "", sizeof(cfg.wifiSsid) - 1);
      strncpy(cfg.wifiPass, d["pass"] | "", sizeof(cfg.wifiPass) - 1);
      cfg.wifiSsid[sizeof(cfg.wifiSsid) - 1] = 0;
      cfg.wifiPass[sizeof(cfg.wifiPass) - 1] = 0;
      ConfigStore::save();
      req->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
      scheduleReboot();
    }));

  // --- AP 配网热点设置(下次开AP生效) ---
  server.on("/api/ap", HTTP_POST, [](AsyncWebServerRequest* r){}, nullptr,
    jsonBody([](AsyncWebServerRequest* req, JsonDocument& d) {
      if (d["apSsid"].is<const char*>()) {
        strncpy(cfg.apSsid, d["apSsid"] | "Solis-Taillight", sizeof(cfg.apSsid) - 1);
        cfg.apSsid[sizeof(cfg.apSsid) - 1] = 0;
      }
      if (d["apFallback"].is<bool>()) cfg.apFallback = d["apFallback"];
      ConfigStore::save();
      sendState(req);
    }));

  // --- 恢复出厂 ---
  server.on("/api/factory", HTTP_POST, [](AsyncWebServerRequest* req){
    ConfigStore::factoryReset();
    req->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
    scheduleReboot();
  });

  // --- 重启 ---
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* req){
    req->send(200, "application/json", "{\"ok\":true}");
    scheduleReboot();
  });

  // --- OTA 固件上传 ---
  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      bool fail = Update.hasError();
      req->send(200, "application/json",
                fail ? "{\"ok\":false}" : "{\"ok\":true,\"reboot\":true}");
      if (!fail) scheduleReboot();
    },
    otaUpload);

  // --- 静态页面 ---
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.onNotFound([](AsyncWebServerRequest* req){
    req->send(404, "text/plain", "not found");
  });

  server.begin();
  Serial.println("[Web] server started on :80");
}
