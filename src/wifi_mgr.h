// WiFi 管理: 尝试连接已存 WiFi, 失败则开 AP 配网热点
#pragma once
#include <Arduino.h>

namespace WifiMgr {
  void   begin();        // 连接 STA 或回退 AP
  bool   isAp();         // 当前是否处于 AP 配网模式
  String ip();           // 当前 IP (STA 的局域网 IP 或 AP 的 192.168.4.1)
  String ssid();         // STA 模式下连接的 SSID
  void   loop();         // 维护(STA 掉线重连)
}
