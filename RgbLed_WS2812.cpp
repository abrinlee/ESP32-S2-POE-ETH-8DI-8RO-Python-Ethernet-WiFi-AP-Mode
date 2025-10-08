#include "RgbLed_WS2812.h"

void RgbLed::begin() {
  // Explicit WS2812 @800kHz with selectable color order
  FastLED.addLeds<WS2812, BoardPins::RGB_LED, LED_COLOR_ORDER>(_leds, NUM_LEDS);
  FastLED.setBrightness(64);
  off();
  _nextBeatMs = millis() + HEARTBEAT_PERIOD_MS;  // schedule first heartbeat
}

void RgbLed::off() {
  _r = _g = _b = 0;
  show();
}

void RgbLed::setAllOnWhite() {
  _r = _g = _b = 255;
  show();
}

void RgbLed::setForRelay(uint8_t relayIndex) {
  const uint8_t idx = (relayIndex & 0x07);
  _r = PALETTE[idx][0];
  _g = PALETTE[idx][1];
  _b = PALETTE[idx][2];
  _idle = false;        // explicit color => not idle
  _inPulse = false;     // cancel heartbeat pulse
  show();
}

void RgbLed::setRGB(uint8_t r, uint8_t g, uint8_t b) {
  _r = r; _g = g; _b = b;
  _idle = (r == 0 && g == 0 && b == 0);
  _inPulse = false;
  show();
}

void RgbLed::setForMask(uint8_t mask) {
  if (mask == 0) {
    // idle state: keep LED off; heartbeat tick() will flash if enabled
    _idle = true;
    _inPulse = false;
    off();
    return;
  }

  // all relays on => white
  const uint8_t full = (uint8_t)((1U << (BoardPins::RELAY_COUNT >= 8 ? 8 : BoardPins::RELAY_COUNT)) - 1);
  if (mask == full) {
    _idle = false;
    _inPulse = false;
    setAllOnWhite();
    return;
  }

  // blend active relay colors
  uint32_t r = 0, g = 0, b = 0;
  uint8_t count = 0;
  for (uint8_t i = 0; i < BoardPins::RELAY_COUNT && i < 8; ++i) {
    if (mask & (1U << i)) {
      r += PALETTE[i][0];
      g += PALETTE[i][1];
      b += PALETTE[i][2];
      ++count;
    }
  }
  if (count == 0) { _idle = true; off(); return; }

  _r = (uint8_t)(r / count);
  _g = (uint8_t)(g / count);
  _b = (uint8_t)(b / count);
  _idle = false;
  _inPulse = false;
  show();
}

void RgbLed::show() {
  _leds[0].setRGB(_r, _g, _b);
  FastLED.show();
}

void RgbLed::startHeartbeatPulse_() {
  // briefly show white, then turn back off
  _inPulse = true;
  _beatOffMs = millis() + HEARTBEAT_PULSE_MS;
  _leds[0].setRGB(255, 255, 255);
  FastLED.show();
}

void RgbLed::tick() {
  if (!_heartbeatEnabled || !_idle) return;

  const uint32_t now = millis();

  if (_inPulse) {
    if ((int32_t)(now - _beatOffMs) >= 0) {
      _inPulse = false;
      off(); // restore to off after pulse
      _nextBeatMs = now + HEARTBEAT_PERIOD_MS; // schedule next beat
    }
    return;
  }

  // Not in pulse; time to start one?
  if ((int32_t)(now - _nextBeatMs) >= 0) {
    startHeartbeatPulse_();
  }
}
