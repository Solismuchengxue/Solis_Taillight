// Web 控制台: 静态页面(LittleFS) + REST API + 固件 OTA
#pragma once
namespace Web {
  void begin();
  void loop();   // 处理延迟重启(让响应先发完再重启)
}
