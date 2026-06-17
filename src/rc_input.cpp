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

// 转向点动一次性定时 + 双闪锁存
static bool     prevLeft = false, prevRight = false, prevBoth = false;
static uint32_t leftUntil = 0, rightUntil = 0;
static bool     hazardLatch = false;
static uint32_t lastLeftTap = 0, lastRightTap = 0;   // 双击检测
#define HZ_DTAP_MS 400

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
static void IRAM_ATTR isrBrake() { edge(chBrake, cfg.rc.brakePin,         true); }
static void IRAM_ATTR isrLeft()  { edge(chLeft,  cfg.rc.left.channelPin,  true); }
static void IRAM_ATTR isrRight() { edge(chRight, cfg.rc.right.channelPin, true); }

static uint16_t pulseOf(const Channel& ch) {
  return (millis() - ch.lastMs < 500) ? ch.width : 0;
}

void RcInput::begin() {
  if (aBrake >= 0) { detachInterrupt(aBrake); aBrake = -1; }
  if (aLeft  >= 0) { detachInterrupt(aLeft);  aLeft  = -1; }
  if (aRight >= 0) { detachInterrupt(aRight); aRight = -1; }
  prevLeft = prevRight = prevBoth = false;
  leftUntil = rightUntil = lastLeftTap = lastRightTap = 0;
  hazardLatch = false;

  if (!cfg.rc.brakeEnabled && !cfg.rc.left.enabled && !cfg.rc.right.enabled) {
    stState = "off"; return;
  }

  if (cfg.rc.brakeEnabled) {
    pinMode(cfg.rc.brakePin, INPUT);
    attachInterrupt(cfg.rc.brakePin, isrBrake, CHANGE);
    aBrake = cfg.rc.brakePin;
  }
  if (cfg.rc.left.enabled) {
    pinMode(cfg.rc.left.channelPin, INPUT);
    attachInterrupt(cfg.rc.left.channelPin, isrLeft, CHANGE);
    aLeft = cfg.rc.left.channelPin;
  }
  if (cfg.rc.right.enabled) {
    pinMode(cfg.rc.right.channelPin, INPUT);
    attachInterrupt(cfg.rc.right.channelPin, isrRight, CHANGE);
    aRight = cfg.rc.right.channelPin;
  }
  Serial.printf("[RC] brake=%d(GPIO%u) L=%d(GPIO%u) R=%d(GPIO%u)\n",
                cfg.rc.brakeEnabled, cfg.rc.brakePin,
                cfg.rc.left.enabled, cfg.rc.left.channelPin,
                cfg.rc.right.enabled, cfg.rc.right.channelPin);
}

uint16_t RcInput::brakePulse() { return pulseOf(chBrake); }
uint16_t RcInput::leftPulse()  { return pulseOf(chLeft);  }
uint16_t RcInput::rightPulse() { return pulseOf(chRight); }
const char* RcInput::stateName(){ return stState; }

bool RcInput::hasSignal() {
  if (!cfg.rc.brakeEnabled && !cfg.rc.left.enabled && !cfg.rc.right.enabled) return false;
  return brakePulse() || leftPulse() || rightPulse();
}

void RcInput::loop() {
  const RcConfig& rc = cfg.rc;
  if (!rc.brakeEnabled && !rc.left.enabled && !rc.right.enabled) return;
  uint32_t now = millis();

  // 刹车: 油门偏离中位超过阈值, 电平实时; brakeReverse 决定判定在中位哪一侧
  bool brakeActive = false;
  if (rc.brakeEnabled) {
    int bW = brakePulse();
    if (bW) brakeActive = rc.brakeReverse ? (bW > (int)rc.centerUs + (int)rc.brakeUs)
                                          : (bW < (int)rc.centerUs - (int)rc.brakeUs);
    LedEngine::setBrake(brakeActive);
  }

  // 左/右转向: 仅点动
  bool lPressed = rc.left.enabled  && leftPulse()  && leftPulse()  > rc.left.triggerUs;
  bool rPressed = rc.right.enabled && rightPulse() && rightPulse() > rc.right.triggerUs;
  bool lRise = lPressed && !prevLeft;
  bool rRise = rPressed && !prevRight;
  bool bothNow = lPressed && rPressed;

  // 双闪触发 (锁存, 再次触发取消)
  bool block = false;
  if (rc.hazardMode == 0) {                          // 左右同时
    if (bothNow && !prevBoth) { hazardLatch = !hazardLatch; leftUntil = rightUntil = 0; }
    block = bothNow;                                 // 同时按下不算单边
  } else if (rc.hazardMode == 1) {                   // 双击任意转向
    if (lRise) { if (now - lastLeftTap  < HZ_DTAP_MS) { hazardLatch = !hazardLatch; leftUntil = rightUntil = 0; } lastLeftTap  = now; }
    if (rRise) { if (now - lastRightTap < HZ_DTAP_MS) { hazardLatch = !hazardLatch; leftUntil = rightUntil = 0; } lastRightTap = now; }
  } else {                                           // 关闭
    hazardLatch = false;
  }
  prevBoth = bothNow;

  // 单边点动 (锁存双闪时忽略)
  bool lActive = false, rActive = false;
  if (!hazardLatch) {
    if (lRise && !block) leftUntil  = now + rc.left.holdMs;
    if (rRise && !block) rightUntil = now + rc.right.holdMs;
    lActive = (int32_t)(now - leftUntil)  < 0;
    rActive = (int32_t)(now - rightUntil) < 0;
  }
  prevLeft = lPressed; prevRight = rPressed;
  LedEngine::setTurn(lActive, rActive, hazardLatch);

  // 状态名(实时监视用); 显示优先级 刹车 > 双闪 > 转向
  if      (brakeActive)  stState = "brake";
  else if (hazardLatch)  stState = "hazard";
  else if (lActive)      stState = "left";
  else if (rActive)      stState = "right";
  else                   stState = "none";
}
