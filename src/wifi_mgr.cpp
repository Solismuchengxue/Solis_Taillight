#include "wifi_mgr.h"
#include "config.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static const char* AP_PASS = "";          // 开放热点, 配完即连家里/车上路由
static const char* HOSTNAME = "Solis-Taillight";  // 路由器显示名 + http://solis-taillight.local

static bool apMode = false;

static const char* apName() {
  return cfg.apSsid[0] ? cfg.apSsid : "Solis-Taillight";
}

static bool connectSta(uint32_t timeoutMs) {
  if (cfg.wifiSsid[0] == 0) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
  Serial.printf("[WiFi] connecting to \"%s\" ...\n", cfg.wifiSsid);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

static void startAp() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  // 固定 AP 网段为 192.168.0.1
  WiFi.softAPConfig(IPAddress(192, 168, 0, 1),
                    IPAddress(192, 168, 0, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName(), strlen(AP_PASS) ? AP_PASS : nullptr);
  Serial.printf("[WiFi] AP mode: SSID=\"%s\"  http://%s\n",
                apName(), WiFi.softAPIP().toString().c_str());
}

void WifiMgr::begin() {
  bool haveCreds = cfg.wifiSsid[0] != 0;

  if (haveCreds && connectSta(12000)) {
    apMode = false;
    Serial.printf("[WiFi] connected, IP=%s  http://%s.local\n",
                  WiFi.localIP().toString().c_str(), HOSTNAME);
  } else if (!haveCreds) {
    // 从未配过 WiFi: 必须开 AP 才能配置(无视回退开关)
    Serial.println("[WiFi] no saved WiFi -> AP config mode");
    startAp();
  } else if (cfg.apFallback) {
    Serial.println("[WiFi] STA failed -> AP fallback");
    startAp();
  } else {
    // 连不上且关闭了回退: 保持后台重连, 不开 AP
    Serial.println("[WiFi] STA failed, AP fallback disabled -> keep retrying");
    WiFi.setAutoReconnect(true);
  }

  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
  }
}

void WifiMgr::loop() {
  // STA 掉线时被动重连(由底层 autoreconnect 处理, 这里仅留扩展位)
}

bool   WifiMgr::isAp() { return apMode; }
String WifiMgr::ip()   { return apMode ? WiFi.softAPIP().toString()
                                       : WiFi.localIP().toString(); }
String WifiMgr::ssid() { return apMode ? String(apName()) : WiFi.SSID(); }
