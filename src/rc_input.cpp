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

static void IRAM_ATTR isrBrake() { edge(chBrake, cfg.rc.brakePin, !cfg.rc.invert); }
static void IRAM_ATTR isrLeft()  { edge(chLeft,  cfg.rc.leftPin,  !cfg.rc.invert); }
static void IRAM_ATTR isrRight() { edge(chRight, cfg.rc.rightPin, !cfg.rc.invert); }

static uint16_t pulseOf(const Channel& ch) {
  return (millis() - ch.lastMs < 500) ? ch.width : 0;
}

void RcInput::begin() {
  if (aBrake >= 0) { detachInterrupt(aBrake); aBrake = -1; }
  if (aLeft  >= 0) { detachInterrupt(aLeft);  aLeft  = -1; }
  if (aRight >= 0) { detachInterrupt(aRight); aRight = -1; }
  prevLeft = prevRight = false;
  leftUntil = rightUntil = 0;

  if (!cfg.rc.enabled) { stState = "off"; return; }

  pinMode(cfg.rc.brakePin, INPUT);
  pinMode(cfg.rc.leftPin,  INPUT);
  pinMode(cfg.rc.rightPin, INPUT);
  attachInterrupt(cfg.rc.brakePin, isrBrake, CHANGE);
  attachInterrupt(cfg.rc.leftPin,  isrLeft,  CHANGE);
  attachInterrupt(cfg.rc.rightPin, isrRight, CHANGE);
  aBrake = cfg.rc.brakePin; aLeft = cfg.rc.leftPin; aRight = cfg.rc.rightPin;
  Serial.printf("[RC] enabled: brake=GPIO%u left=GPIO%u right=GPIO%u invert=%d hold=%ums\n",
                cfg.rc.brakePin, cfg.rc.leftPin, cfg.rc.rightPin,
                cfg.rc.invert, cfg.rc.turnHoldMs);
}

uint16_t RcInput::brakePulse() { return pulseOf(chBrake); }
uint16_t RcInput::leftPulse()  { return pulseOf(chLeft);  }
uint16_t RcInput::rightPulse() { return pulseOf(chRight); }
const char* RcInput::stateName(){ return stState; }

bool RcInput::hasSignal() {
  if (!cfg.rc.enabled) return false;
  return brakePulse() || leftPulse() || rightPulse();
}

void RcInput::loop() {
  if (!cfg.rc.enabled) return;             // 停用时完全不干预常规灯效

  const RcConfig& rc = cfg.rc;
  uint32_t now = millis();

  // 转向点动: 检测"按下"上升沿, 触发一次 turnHoldMs 保持窗口
  bool lPressed = leftPulse()  && leftPulse()  > rc.turnTriggerUs;
  bool rPressed = rightPulse() && rightPulse() > rc.turnTriggerUs;
  if (lPressed && !prevLeft)  leftUntil  = now + rc.turnHoldMs;
  if (rPressed && !prevRight) rightUntil = now + rc.turnHoldMs;
  prevLeft = lPressed; prevRight = rPressed;
  bool lActive = (int32_t)(now - leftUntil)  < 0;
  bool rActive = (int32_t)(now - rightUntil) < 0;

  // 刹车: 油门低于 (中位 - 刹车阈值), 电平实时
  uint16_t bW = brakePulse();
  bool brakeActive = bW && bW < (int)rc.centerUs - (int)rc.brakeUs;

  // 优先级: 刹车 > 双闪 > 单向转向
  Overlay ov; const char* st;
  if (brakeActive)            { ov = OV_BRAKE;  st = "brake";  }
  else if (lActive && rActive){ ov = OV_HAZARD; st = "hazard"; }
  else if (lActive)           { ov = OV_LEFT;   st = "left";   }
  else if (rActive)           { ov = OV_RIGHT;  st = "right";  }
  else                        { ov = OV_NONE;   st = "none";   }

  stState = st;
  if (LedEngine::overlay() != ov) LedEngine::setOverlay(ov);
}
