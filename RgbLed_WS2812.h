#ifndef RGBLED_WS2812_H
#define RGBLED_WS2812_H

#include <Arduino.h>
#include <FastLED.h>
#ifndef RGBLED_WS2812_H
#define RGBLED_WS2812_H

#include <Arduino.h>
#include <FastLED.h>
#include "BoardPins.h"

#ifndef LED_COLOR_ORDER
#define LED_COLOR_ORDER RGB   // set once in BoardPins.h if you want to override
#endif

class RgbLed {
public:
  void begin();

  // Hard states
  void off();
  void setAllOnWhite();

  // Per-relay color (0..7)
  void setForRelay(uint8_t relayIndex);

  // Explicit RGB
  void setRGB(uint8_t r, uint8_t g, uint8_t b);

  // Reflect relay bitmask. (0 => idle, all => white, many => blend)
  void setForMask(uint8_t mask);

  // Heartbeat control (white blip every 5s while idle)
  void setHeartbeatEnabled(bool on) { _heartbeatEnabled = on; }
  void tick();  // call from loop()

  // Push current color to the LED
  void show();

private:
  // palette for up to 8 relays
  static constexpr uint8_t PALETTE[8][3] = {
    {255,   0,   0}, // 0 Red
    {255, 128,   0}, // 1 Orange
    {255, 255,   0}, // 2 Yellow
    {128, 255,   0}, // 3 Chartreuse
    {  0, 255,   0}, // 4 Green
    {  0, 255, 255}, // 5 Cyan
    {  0,   0, 255}, // 6 Blue
    {255,   0, 255}  // 7 Magenta
  };

  static constexpr uint8_t  NUM_LEDS = 1;
  static constexpr uint32_t HEARTBEAT_PERIOD_MS = 5000; // every 5s
  static constexpr uint32_t HEARTBEAT_PULSE_MS  = 120;  // blink length

  // current color
  uint8_t _r = 0, _g = 0, _b = 0;
  CRGB    _leds[NUM_LEDS];

  // heartbeat state
  bool     _idle = true;              // true when mask == 0
  bool     _heartbeatEnabled = true;  // can be toggled
  bool     _inPulse = false;
  uint32_t _nextBeatMs = 0;
  uint32_t _beatOffMs  = 0;

  void startHeartbeatPulse_();
};

#endif // RGBLED_WS2812_H
#include "BoardPins.h"

#ifndef LED_COLOR_ORDER
#define LED_COLOR_ORDER RGB   // set once in BoardPins.h if you want to override
#endif

class RgbLed {
public:
  void begin();

  // Hard states
  void off();
  void setAllOnWhite();

  // Per-relay color (0..7)
  void setForRelay(uint8_t relayIndex);

  // Explicit RGB
  void setRGB(uint8_t r, uint8_t g, uint8_t b);

  // Reflect relay bitmask. (0 => idle, all => white, many => blend)
  void setForMask(uint8_t mask);

  // Heartbeat control (white blip every 5s while idle)
  void setHeartbeatEnabled(bool on) { _heartbeatEnabled = on; }
  void tick();  // call from loop()

  // Push current color to the LED
  void show();

private:
  // palette for up to 8 relays
  static constexpr uint8_t PALETTE[8][3] = {
    {255,   0,   0}, // 0 Red
    {255, 128,   0}, // 1 Orange
    {255, 255,   0}, // 2 Yellow
    {128, 255,   0}, // 3 Chartreuse
    {  0, 255,   0}, // 4 Green
    {  0, 255, 255}, // 5 Cyan
    {  0,   0, 255}, // 6 Blue
    {255,   0, 255}  // 7 Magenta
  };

  static constexpr uint8_t  NUM_LEDS = 1;
  static constexpr uint32_t HEARTBEAT_PERIOD_MS = 5000; // every 5s
  static constexpr uint32_t HEARTBEAT_PULSE_MS  = 120;  // blink length

  // current color
  uint8_t _r = 0, _g = 0, _b = 0;
  CRGB    _leds[NUM_LEDS];

  // heartbeat state
  bool     _idle = true;              // true when mask == 0
  bool     _heartbeatEnabled = true;  // can be toggled
  bool     _inPulse = false;
  uint32_t _nextBeatMs = 0;
  uint32_t _beatOffMs  = 0;

  void startHeartbeatPulse_();
};

#endif // RGBLED_WS2812_H
