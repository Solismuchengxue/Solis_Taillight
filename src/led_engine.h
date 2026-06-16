// 灯效引擎: 渲染常规灯效, 并支持 RC 叠加灯效(优先级更高)
#pragma once
#include <Arduino.h>
#include "config.h"

// RC 叠加状态 (优先于常规灯效显示)
enum Overlay : uint8_t {
  OV_NONE = 0,
  OV_BRAKE,    // 刹车: 整条爆闪红/常亮红
  OV_LEFT,     // 左转: 左侧琥珀流水
  OV_RIGHT,    // 右转: 右侧琥珀流水
  OV_HAZARD    // 双闪(左右同时)
};

namespace LedEngine {
  void begin();                       // 按 cfg 初始化灯条
  void reinit();                      // 引脚/数量变更后重建灯条
  void applyPreset(const Preset& p);  // 切换当前常规灯效
  void setOverlay(Overlay ov);        // 设置 RC 叠加(OV_NONE 取消)
  Overlay overlay();
  void loop();                        // 主循环调用, 负责渲染+show
}
