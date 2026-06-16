#include "led_engine.h"
#include <FastLED.h>

// 每条物理灯带: 渲染缓冲(全亮度, 灯效在此持续演化) + 显示缓冲(按预设亮度缩放后送 FastLED)
static CRGB     renderBuf[3][MAX_LEDS];
static CRGB     showBuf[3][MAX_LEDS];
static uint8_t  stripPin[3];
static int      nStrips = 0;
static uint16_t count   = 6;

static Preset   decoPreset;          // 装饰灯当前预设
static bool     brakeOn = false, turnLeft = false, turnRight = false;

// 运行期绑定数据脚 (FastLED 引脚是编译期模板, 用 switch 覆盖候选脚)
static void addStrip(uint8_t pin, CRGB* buf) {
  switch (pin) {
    case 0:  FastLED.addLeds<WS2812B, 0,  GRB>(buf, MAX_LEDS); break;
    case 1:  FastLED.addLeds<WS2812B, 1,  GRB>(buf, MAX_LEDS); break;
    case 4:  FastLED.addLeds<WS2812B, 4,  GRB>(buf, MAX_LEDS); break;
    case 5:  FastLED.addLeds<WS2812B, 5,  GRB>(buf, MAX_LEDS); break;
    case 6:  FastLED.addLeds<WS2812B, 6,  GRB>(buf, MAX_LEDS); break;
    case 7:  FastLED.addLeds<WS2812B, 7,  GRB>(buf, MAX_LEDS); break;
    case 10: FastLED.addLeds<WS2812B, 10, GRB>(buf, MAX_LEDS); break;
    case 20: FastLED.addLeds<WS2812B, 20, GRB>(buf, MAX_LEDS); break;
    case 21: FastLED.addLeds<WS2812B, 21, GRB>(buf, MAX_LEDS); break;
    case 3:
    default: FastLED.addLeds<WS2812B, 3,  GRB>(buf, MAX_LEDS); break;
  }
}

// 取/建该数据脚对应的灯带下标
static int stripFor(uint8_t pin) {
  for (int i = 0; i < nStrips; i++) if (stripPin[i] == pin) return i;
  if (nStrips < 3) { stripPin[nStrips] = pin; addStrip(pin, showBuf[nStrips]); return nStrips++; }
  return 0;
}

static const Preset& presetAt(uint8_t idx) {
  uint8_t n = cfg.presetCount ? cfg.presetCount : 1;
  return cfg.presets[min<uint8_t>(idx, n - 1)];
}

void LedEngine::begin() {
  count = constrain(cfg.ledCount, (uint16_t)1, (uint16_t)MAX_LEDS);
  FastLED.clear(true);
  nStrips = 0;
  stripFor(cfg.ledPin);                       // 装饰灯(基础层)优先注册
  if (cfg.rc.turnEnabled)  stripFor(cfg.rc.turnLedPin);
  if (cfg.rc.brakeEnabled) stripFor(cfg.rc.brakeLedPin);
  FastLED.setBrightness(255);                 // 全局不限亮, 亮度按预设逐条缩放
  applyPreset(presetAt(cfg.defaultPreset));
}

void LedEngine::applyPreset(const Preset& p) { decoPreset = p; }
void LedEngine::setBrake(bool on) { brakeOn = on; }
void LedEngine::setTurn(bool left, bool right) { turnLeft = left; turnRight = right; }

// 把某预设的灯效渲染进 buf (全亮度); 各灯效自身在 buf 上持续演化
static void renderEffect(CRGB* buf, uint16_t n, const Preset& p) {
  static uint8_t  beat = 0;
  static uint32_t lastStep = 0;
  CRGB col(p.r, p.g, p.b);

  uint16_t stepMs = map(p.speed, 0, 255, 120, 8);
  uint32_t now = millis();
  bool tick = (now - lastStep) >= stepMs;
  if (tick) lastStep = now;

  switch (p.effect) {
    case FX_SOLID:
      fill_solid(buf, n, col);
      break;
    case FX_BREATHE: {
      uint8_t b = beatsin8(map(p.speed, 0, 255, 6, 40), 10, 255);
      CRGB c = col; c.nscale8_video(b);
      fill_solid(buf, n, c);
      break;
    }
    case FX_RAINBOW:
      if (tick) beat++;
      fill_rainbow(buf, n, beat, max<int>(1, 255 / n));
      break;
    case FX_SCANNER: {
      fadeToBlackBy(buf, n, 64);
      uint16_t pos = beatsin16(map(p.speed, 0, 255, 8, 60), 0, n - 1);
      buf[pos] = col;
      break;
    }
    case FX_BLINK: {
      static bool on = false;
      if (tick) on = !on;
      fill_solid(buf, n, on ? col : CRGB::Black);
      break;
    }
    case FX_SPARKLE:
      fadeToBlackBy(buf, n, 40);
      if (tick) buf[random16(n)] = col;
      break;
    case FX_COMET: {
      static uint16_t head = 0;
      fadeToBlackBy(buf, n, 80);
      buf[head] = col;
      if (tick) head = (head + 1) % n;
      break;
    }
    case FX_COMET_REV: {
      static uint16_t headR = 0;
      fadeToBlackBy(buf, n, 80);
      buf[n - 1 - headR] = col;
      if (tick) headR = (headR + 1) % n;
      break;
    }
    default:
      fill_solid(buf, n, col);
      break;
  }
}

// 交通标准: 刹车=红色常亮, 转向=琥珀色流水
static const CRGB TRAFFIC_AMBER(255, 110, 0);
#define TURN_TAIL 4              // 流水拖尾长度(LED)

void LedEngine::loop() {
  // 转向流水推进 (~70ms/格)
  static uint16_t turnHead = 0;
  static uint32_t lastTurnMs = 0;
  uint32_t nowMs = millis();
  if (nowMs - lastTurnMs > 70) { turnHead = (turnHead + 1) % count; lastTurnMs = nowMs; }

  for (int s = 0; s < nStrips; s++) {
    uint8_t pin = stripPin[s];
    // 优先级: 刹车 > 转向 > 装饰
    if (brakeOn && cfg.rc.brakeEnabled && cfg.rc.brakeLedPin == pin) {
      fill_solid(showBuf[s], count, CRGB::Red);                          // 刹车: 满红常亮(全亮)
    } else if ((turnLeft || turnRight) && cfg.rc.turnEnabled && cfg.rc.turnLedPin == pin) {
      if (turnLeft && turnRight) {
        // 双闪: 整条琥珀闪烁(无方向)
        fill_solid(showBuf[s], count, ((millis() / 333) % 2 == 0) ? TRAFFIC_AMBER : CRGB::Black);
      } else {
        // 单向流水: 左/右方向相反; turnReverse 适配灯带装向
        bool rev = turnLeft ^ cfg.rc.turnReverse;
        int h = rev ? (count - 1 - turnHead) : turnHead;
        for (uint16_t i = 0; i < count; i++) {
          int d = rev ? ((int)i - h) : (h - (int)i);
          if (d < 0) d += count;                                         // 环绕
          uint8_t v = (d < TURN_TAIL) ? (uint8_t)(255 - d * (256 / TURN_TAIL)) : 0;
          showBuf[s][i] = TRAFFIC_AMBER;
          showBuf[s][i].nscale8_video(v);
        }
      }
    } else if (cfg.ledPin == pin) {
      renderEffect(renderBuf[s], count, decoPreset);                     // 装饰: 按预设(逐条缩放亮度)
      uint8_t br = decoPreset.brightness;
      for (uint16_t i = 0; i < count; i++) {
        showBuf[s][i] = renderBuf[s][i];
        if (br < 255) showBuf[s][i].nscale8_video(br);
      }
    } else {
      fill_solid(showBuf[s], count, CRGB::Black);   // 该脚无激活功能 → 灭
    }
  }
  FastLED.show();
}
