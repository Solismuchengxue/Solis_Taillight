#include "rc_input.h"
#include "config.h"
#include "led_engine.h"

// 单通道脉宽采集 (ISR 写, loop 读)
struct Channel {
  volatile uint32_t startT = 0;
  volatile uint16_t width  = 0;     // 最近脉宽 us
  volatile uint32_t lastMs = 0;     // 最近更新时刻 ms (判失联)
};

static Channel chBrake, chLeft, chRight;
static int8_t  aBrake = -1, aLeft = -1, aRight = -1;   // 已 attach 的引脚
static const char* stState = "off";

// 转向点动一次性定时
static bool     prevLeft = false, prevRight = false;
static uint32_t leftUntil = 0, rightUntil = 0;

// 通用边沿处理: activeHigh=false 表示信号反相(脉冲为低电平期)
static inline void IRAM_ATTR edge(Channel& ch, uint8_t pin, bool activeHigh) {
  bool level = digitalRead(pin);
  if (level == activeHigh) {          // 脉冲起始边沿
    ch.startT = micros();
  } else {                            // 脉冲结束边沿
    uint32_t w = micros() - ch.startT;
    if (w >= 500 && w <= 2500) {      // 合理舵机脉宽才采纳
      ch.width  = (uint16_t)w;
      ch.lastMs = millis();
    }
  }
}

// 直连方案: 信号高电平为脉冲 (active-high), 不再做信号反相
static void IRAM_ATTR isrBrake() { edge(chBrake, cfg.rc.brakePin, true); }
static void IRAM_ATTR isrLeft()  { edge(chLeft,  cfg.rc.leftPin,  true); }
static void IRAM_ATTR isrRight() { edge(chRight, cfg.rc.rightPin, true); }

static uint16_t pulseOf(const Channel& ch) {
  return (millis() - ch.lastMs < 500) ? ch.width : 0;
}

void RcInput::begin() {
  if (aBrake >= 0) { detachInterrupt(aBrake); aBrake = -1; }
  if (aLeft  >= 0) { detachInterrupt(aLeft);  aLeft  = -1; }
  if (aRight >= 0) { detachInterrupt(aRight); aRight = -1; }
  prevLeft = prevRight = false;
  leftUntil = rightUntil = 0;

  if (!cfg.rc.brakeEnabled && !cfg.rc.turnEnabled) { stState = "off"; return; }

  if (cfg.rc.brakeEnabled) {
    pinMode(cfg.rc.brakePin, INPUT);
    attachInterrupt(cfg.rc.brakePin, isrBrake, CHANGE);
    aBrake = cfg.rc.brakePin;
  }
  if (cfg.rc.turnEnabled) {
    pinMode(cfg.rc.leftPin,  INPUT);
    pinMode(cfg.rc.rightPin, INPUT);
    attachInterrupt(cfg.rc.leftPin,  isrLeft,  CHANGE);
    attachInterrupt(cfg.rc.rightPin, isrRight, CHANGE);
    aLeft = cfg.rc.leftPin; aRight = cfg.rc.rightPin;
  }
  Serial.printf("[RC] brake=%d(GPIO%u) turn=%d(L%u R%u mode=%u) \n",
                cfg.rc.brakeEnabled, cfg.rc.brakePin,
                cfg.rc.turnEnabled, cfg.rc.leftPin, cfg.rc.rightPin, cfg.rc.turnMode);
}

uint16_t RcInput::brakePulse() { return pulseOf(chBrake); }
uint16_t RcInput::leftPulse()  { return pulseOf(chLeft);  }
uint16_t RcInput::rightPulse() { return pulseOf(chRight); }
const char* RcInput::stateName(){ return stState; }

bool RcInput::hasSignal() {
  if (!cfg.rc.brakeEnabled && !cfg.rc.turnEnabled) return false;
  return brakePulse() || leftPulse() || rightPulse();
}

void RcInput::loop() {
  const RcConfig& rc = cfg.rc;
  if (!rc.brakeEnabled && !rc.turnEnabled) return;   // 全停用时不干预常规灯效
  uint32_t now = millis();

  // 刹车: 油门偏离中位超过阈值, 电平实时; brakeReverse 决定判定在中位哪一侧
  bool brakeActive = false;
  if (rc.brakeEnabled) {
    int bW = brakePulse();
    if (bW) brakeActive = rc.brakeReverse ? (bW > (int)rc.centerUs + (int)rc.brakeUs)
                                          : (bW < (int)rc.centerUs - (int)rc.brakeUs);
    LedEngine::setBrake(brakeActive);
  }

  // 转向: 点动(上升沿触发保持 turnHoldMs) 或 两点(高亮低灭)
  bool lActive = false, rActive = false;
  if (rc.turnEnabled) {
    bool lPressed = leftPulse()  && leftPulse()  > rc.turnTriggerUs;
    bool rPressed = rightPulse() && rightPulse() > rc.turnTriggerUs;
    if (rc.turnMode == TURN_MAINTAINED) {
      lActive = lPressed; rActive = rPressed;             // 保持式
    } else {
      if (lPressed && !prevLeft)  leftUntil  = now + rc.turnHoldMs;
      if (rPressed && !prevRight) rightUntil = now + rc.turnHoldMs;
      lActive = (int32_t)(now - leftUntil)  < 0;          // 点动定时
      rActive = (int32_t)(now - rightUntil) < 0;
    }
    prevLeft = lPressed; prevRight = rPressed;
    LedEngine::setTurn(lActive, rActive);
  }

  // 状态名(实时监视用); 显示优先级 刹车 > 转向
  if      (brakeActive)        stState = "brake";
  else if (lActive && rActive) stState = "hazard";
  else if (lActive)            stState = "left";
  else if (rActive)            stState = "right";
  else                         stState = "none";
}
