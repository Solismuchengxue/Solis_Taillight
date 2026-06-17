#include "wifi_mgr.h"
#include "config.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static const char* AP_PASS = "";          // 开放热点, 配完即连家里/车上路由
static const char* MDNS_NAME = "Solis-Taillight";  // http://solis-taillight.local (mDNS, 与espota一致)

static bool   apMode = false;
static String g_hostname;                  // 路由器显示的 DHCP 主机名 SolisTaillight-<MAC后3字节>

static const char* apName() {
  return cfg.apSsid[0] ? cfg.apSsid : "Solis-Taillight";
}

// arduino-esp32 需在 STA 启动事件里设主机名才会生效(否则用默认 esp32c3-xxxxxx)
static void onWiFiEvent(WiFiEvent_t event) {
  if (event == ARDUINO_EVENT_WIFI_STA_START && g_hostname.length())
    WiFi.setHostname(g_hostname.c_str());
}

static bool connectSta(uint32_t timeoutMs) {
  if (cfg.wifiSsid[0] == 0) return false;
  WiFi.setHostname(g_hostname.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(g_hostname.c_str());
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
  // 组装 DHCP 主机名: SolisTaillight-<MAC 后3字节> (保留默认那串后缀)
  uint8_t mac[6]; WiFi.macAddress(mac);
  char buf[40];
  snprintf(buf, sizeof(buf), "SolisTaillight-%02X%02X%02X", mac[3], mac[4], mac[5]);
  g_hostname = buf;
  WiFi.onEvent(onWiFiEvent);

  bool haveCreds = cfg.wifiSsid[0] != 0;

  if (haveCreds && connectSta(12000)) {
    apMode = false;
    Serial.printf("[WiFi] connected, host=%s IP=%s  http://%s.local\n",
                  g_hostname.c_str(), WiFi.localIP().toString().c_str(), MDNS_NAME);
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

  if (MDNS.begin(MDNS_NAME)) {
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
