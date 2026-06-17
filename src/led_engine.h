// 灯效引擎: 最多 3 条灯带(装饰/转向/刹车), 按数据脚去重。
// 同一数据脚上多个功能共用, 优先级 刹车 > 转向 > 装饰。
// 转向/刹车显示各自配置的预设; 亮度按每个预设独立(无全局上限)。
#pragma once
#include <Arduino.h>
#include "config.h"

namespace LedEngine {
  void begin();                       // 按 cfg 建立灯带(数据脚变更需重启)
  void applyPreset(const Preset& p);  // 设置装饰灯当前预设(轮换/默认/实时预览)
  void setBrake(bool on);             // 刹车是否激活
  void setTurn(bool left, bool right, bool hazard);// 转向: 左/右流水 + 双闪锁存
  void loop();                        // 渲染所有灯带并 show
}
