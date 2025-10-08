
#pragma once
/*
  StateHelpers.h — header-only helpers for DI and Relay state decoding.
  Intent: Keep data-side logic self-contained for reuse (e.g., RS485 or MQTT gateways).

  Conventions:
  - Digital Inputs (DI) are ACTIVE-LOW on the board hardware.
    * "Raw-High" reading means the MCU pin read HIGH (logic 1).
    * "Active" means the line is asserted (pulled LOW). We expose helpers that return masks where 1=ACTIVE.
  - Relays: g_mask contains the authoritative relay mask (1 bit per channel, 1=ON).
*/

#include <Arduino.h>

// Forward decls expected from the main .ino:
namespace BoardPins {
  extern const uint8_t DI_COUNT;
  extern const uint8_t RELAY_COUNT;
}
extern uint8_t g_mask;                 // relay ON/OFF mask from the .ino
extern uint8_t readDI_mask();          // RAW-HIGH mask from the .ino

// ---------------- DI helpers ----------------
inline uint8_t diRawMaskHigh() {
  // Bit=1 means the MCU pin reads HIGH (NOT active on active-low hardware).
  return readDI_mask();
}

inline uint8_t diActiveMask() {
  // Convert raw-high to active (active-low): ACTIVE bit = 1 when pin is LOW.
  // active = !rawHigh  -> mask inversion within valid DI_COUNT bits.
  const uint8_t raw = diRawMaskHigh();
  const uint8_t maskN = (BoardPins::DI_COUNT >= 8) ? 0xFFu : ((1u << BoardPins::DI_COUNT) - 1u);
  return (~raw) & maskN;
}

inline bool diRawHigh(uint8_t idx) {
  if (idx >= BoardPins::DI_COUNT) return false;
  return (diRawMaskHigh() >> idx) & 0x1;
}

inline bool diActive(uint8_t idx) {
  if (idx >= BoardPins::DI_COUNT) return false;
  return (diActiveMask() >> idx) & 0x1;
}

// ---------------- Relay helpers ----------------
inline uint8_t getRelayMask() {
  // Bit=1 means Relay is ON
  return g_mask;
}

inline bool getRelay(uint8_t idx) {
  if (idx >= BoardPins::RELAY_COUNT) return false;
  return (getRelayMask() >> idx) & 0x1;
}

inline void setRelayBit(uint8_t idx, bool on) {
  // Optional convenience for data-plane logic (does not write hardware):
  if (idx >= BoardPins::RELAY_COUNT) return;
  if (on) g_mask |= (1u << idx);
  else    g_mask &= ~(1u << idx);
}

inline void setRelaysMaskLocal(uint8_t mask) {
  // Optional local setter for data-plane logic (does not write hardware)
  const uint8_t validMask = (BoardPins::RELAY_COUNT >= 8) ? 0xFFu : ((1u << BoardPins::RELAY_COUNT) - 1u);
  g_mask = mask & validMask;
}
