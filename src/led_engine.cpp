#include "led_engine.h"
#include <FastLED.h>

static CRGB     leds[MAX_LEDS];
static uint16_t count = 6;

static Preset   curPreset;
static Overlay  curOverlay = OV_NONE;

// 运行期选择数据脚: FastLED 的引脚是编译期模板, 故用 switch 覆盖候选脚。
// 候选脚避开 strapping(2/8/9)。需要新脚就在这里补一行。
static void addLedsForPin(uint8_t pin) {
  FastLED.clear(true);
  switch (pin) {
    case 0:  FastLED.addLeds<WS2812B, 0,  GRB>(leds, MAX_LEDS); break;
    case 1:  FastLED.addLeds<WS2812B, 1,  GRB>(leds, MAX_LEDS); break;
    case 4:  FastLED.addLeds<WS2812B, 4,  GRB>(leds, MAX_LEDS); break;
    case 5:  FastLED.addLeds<WS2812B, 5,  GRB>(leds, MAX_LEDS); break;
    case 6:  FastLED.addLeds<WS2812B, 6,  GRB>(leds, MAX_LEDS); break;
    case 7:  FastLED.addLeds<WS2812B, 7,  GRB>(leds, MAX_LEDS); break;
    case 10: FastLED.addLeds<WS2812B, 10, GRB>(leds, MAX_LEDS); break;
    case 20: FastLED.addLeds<WS2812B, 20, GRB>(leds, MAX_LEDS); break;
    case 21: FastLED.addLeds<WS2812B, 21, GRB>(leds, MAX_LEDS); break;
    case 3:
    default: FastLED.addLeds<WS2812B, 3,  GRB>(leds, MAX_LEDS); break;
  }
}

void LedEngine::begin() {
  count = constrain(cfg.ledCount, (uint16_t)1, (uint16_t)MAX_LEDS);
  addLedsForPin(cfg.ledPin);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(cfg.brightness);
  // 默认先应用 defaultPreset (main 里会按轮换逻辑覆盖)
  applyPreset(cfg.presets[min<uint8_t>(cfg.defaultPreset, cfg.presetCount - 1)]);
}

void LedEngine::reinit() {
  FastLED.clearData();
  begin();
}

void LedEngine::applyPreset(const Preset& p) {
  curPreset = p;
}

void LedEngine::setOverlay(Overlay ov) { curOverlay = ov; }
Overlay LedEngine::overlay()           { return curOverlay; }

// ---------------- 常规灯效渲染 ----------------
static void renderEffect() {
  static uint8_t  beat = 0;        // 通用相位
  static uint32_t lastStep = 0;
  const Preset& p = curPreset;
  CRGB col(p.r, p.g, p.b);

  // speed(0-255) 映射成步进间隔(慢->快)
  uint16_t stepMs = map(p.speed, 0, 255, 120, 8);
  uint32_t now = millis();
  bool tick = (now - lastStep) >= stepMs;
  if (tick) lastStep = now;

  switch (p.effect) {
    case FX_SOLID:
      fill_solid(leds, count, col);
      break;

    case FX_BREATHE: {
      uint8_t b = beatsin8(map(p.speed, 0, 255, 6, 40), 10, 255);
      CRGB c = col; c.nscale8_video(b);
      fill_solid(leds, count, c);
      break;
    }

    case FX_RAINBOW:
      if (tick) beat++;
      fill_rainbow(leds, count, beat, max<int>(1, 255 / count));
      break;

    case FX_SCANNER: {
      fadeToBlackBy(leds, count, 64);
      uint16_t pos = beatsin16(map(p.speed, 0, 255, 8, 60), 0, count - 1);
      leds[pos] = col;
      break;
    }

    case FX_BLINK: {
      static bool on = false;
      if (tick) on = !on;
      fill_solid(leds, count, on ? col : CRGB::Black);
      break;
    }

    case FX_SPARKLE:
      fadeToBlackBy(leds, count, 40);
      if (tick) leds[random16(count)] = col;
      break;

    case FX_COMET: {
      static uint16_t head = 0;
      fadeToBlackBy(leds, count, 80);
      leds[head] = col;
      if (tick) head = (head + 1) % count;
      break;
    }

    default:
      fill_solid(leds, count, col);
      break;
  }
}

// ---------------- RC 叠加灯效渲染 ----------------
static const CRGB AMBER(255, 90, 0);   // 转向灯琥珀色

static void renderOverlay() {
  static uint32_t lastStep = 0;
  static bool blinkOn = false;
  uint32_t now = millis();
  if (now - lastStep >= 220) { lastStep = now; blinkOn = !blinkOn; }

  uint16_t half = count / 2;

  switch (curOverlay) {
    case OV_BRAKE:
      // 刹车: 整条满红常亮(最高优先, 最醒目)
      fill_solid(leds, count, CRGB::Red);
      break;

    case OV_LEFT:
      fill_solid(leds, count, CRGB::Black);
      if (blinkOn) for (uint16_t i = 0; i < half; i++) leds[i] = AMBER;
      break;

    case OV_RIGHT:
      fill_solid(leds, count, CRGB::Black);
      if (blinkOn) for (uint16_t i = count - half; i < count; i++) leds[i] = AMBER;
      break;

    case OV_HAZARD:
      fill_solid(leds, count, blinkOn ? AMBER : CRGB::Black);
      break;

    default:
      break;
  }
}

void LedEngine::loop() {
  if (curOverlay != OV_NONE) renderOverlay();
  else                       renderEffect();

  // 该预设亮度与全局亮度取较小, 防止超过总亮度上限
  FastLED.setBrightness(min<uint8_t>(curPreset.brightness, cfg.brightness));
  FastLED.show();
}
