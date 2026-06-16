// RC 输入: 油门(刹车,电平) + 左/右转向两独立通道(点动,触发保持 turnHoldMs)
#pragma once
#include <Arduino.h>

namespace RcInput {
  void begin();              // 按 cfg.rc attach/detach 中断 (可重复调用以重配)
  void loop();               // 计算状态并设置 LedEngine 叠加灯效
  uint16_t brakePulse();     // 油门通道最近脉宽 (0=无信号)
  uint16_t leftPulse();      // 左转通道最近脉宽
  uint16_t rightPulse();     // 右转通道最近脉宽
  const char* stateName();   // "off"/"none"/"brake"/"left"/"right"/"hazard"
  bool hasSignal();
}
