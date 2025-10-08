#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

/*
  BoardPins.h — Waveshare ESP32-S3-PoE-ETH-8DI-8RO
  Single source of truth for all board pin mappings & a few tiny helpers.
  Safe to include in one-translation-unit Arduino projects.
  This file defines pins in a namespace to avoid global pollution.
  Aliases at the bottom for backward compatibility with older sketches.
*/

namespace BoardPins {
  // --------- Ethernet (W5500 over SPI) ---------
  // Pins for the W5500 Ethernet chip using SPI.
  inline constexpr int W5500_CS   = 16;  // ETH_CS (chip select)
  inline constexpr int W5500_INT  = 12;  // ETH_INT (interrupt)
  inline constexpr int W5500_RST  = 39;  // Not exposed on this board
  inline constexpr int SPI_SCLK   = 15;  // ETH_SCLK (clock)
  inline constexpr int SPI_MISO   = 14;  // ETH_MISO (master in slave out)
  inline constexpr int SPI_MOSI   = 13;  // ETH_MOSI (master out slave in)

  // --------- I2C (TCA9554 expander + RTC, etc.) ---------
  // Pins for I2C bus, used for the TCA9554 I/O expander controlling relays.
  inline constexpr int I2C_SDA    = 42;  // Data line
  inline constexpr int I2C_SCL    = 41;  // Clock line
  inline constexpr uint8_t TCA9554_ADDR = 0x20; // Default address (A2..A0 = 000)

  // --------- Digital Inputs (opto-isolated) ---------
  // Array of pins for the 8 opto-isolated digital inputs.
  inline constexpr uint8_t DI_COUNT = 8;
  inline const uint8_t DI_PINS[DI_COUNT] = { 4, 5, 6, 7, 8, 9, 10, 11 };

  // --------- Relays (via TCA9554) ---------
  // Number of relays controlled by the TCA9554 expander (pins 0..7).
  inline constexpr uint8_t RELAY_COUNT = 8;

  // --------- Aux peripherals ---------
  // Miscellaneous pins for onboard peripherals.
  inline constexpr int BOOT_BTN  = 0;   // Boot button
  inline constexpr int BUZZER    = 46;  // Buzzer control
  inline constexpr int RGB_LED   = 38;  // WS2812 RGB LED control

  // RS485 / CAN transceiver (through digital isolation)
  inline constexpr int RS485_TX  = 17;  // TXD / driver input
  inline constexpr int RS485_RX  = 18;  // RXD / receiver output

  // TF (SD) Card (if used later)
  inline constexpr int SD_D0     = 45;  // Data 0
  inline constexpr int SD_CMD    = 47;  // Command
  inline constexpr int SD_CLK    = 48;  // Clock

  // --------- Convenience helpers (optional) ---------
  // Initialize I2C with specified frequency (default 400kHz).
  inline void beginI2C(uint32_t freq = 400000) {
    Wire.begin(I2C_SDA, I2C_SCL, freq);
  }

  // Initialize SPI with board-specific pins.
  inline void beginSPI() {
    SPI.begin(SPI_SCLK, SPI_MISO, SPI_MOSI);
  }

  // Configure digital input pins (with optional pullups).
  inline void configInputs(bool usePullups = true) {
    for (uint8_t i = 0; i < DI_COUNT; ++i) {
      pinMode(DI_PINS[i], usePullups ? INPUT_PULLUP : INPUT);
    }
  }
} // namespace BoardPins

// ===== Backward-compatibility aliases for the main sketch =====
// (The starter sketch references these names.)
inline constexpr int PIN_W5500_CS  = BoardPins::W5500_CS;
inline constexpr int PIN_W5500_INT = BoardPins::W5500_INT;
inline constexpr int PIN_W5500_RST = BoardPins::W5500_RST;
inline constexpr int PIN_SCK       = BoardPins::SPI_SCLK;
inline constexpr int PIN_MISO      = BoardPins::SPI_MISO;
inline constexpr int PIN_MOSI      = BoardPins::SPI_MOSI;

inline constexpr int PIN_I2C_SDA   = BoardPins::I2C_SDA;
inline constexpr int PIN_I2C_SCL   = BoardPins::I2C_SCL;
inline constexpr uint8_t TCA_ADDR  = BoardPins::TCA9554_ADDR;

inline constexpr uint8_t DI_COUNT  = BoardPins::DI_COUNT;
inline const uint8_t INPUT_PINS[DI_COUNT] = { 4, 5, 6, 7, 8, 9, 10, 11 };

// Extras
inline constexpr int PIN_BOOT_BTN  = BoardPins::BOOT_BTN;
inline constexpr int PIN_BUZZER    = BoardPins::BUZZER;
inline constexpr int PIN_RGB       = BoardPins::RGB_LED;
inline constexpr int PIN_RS485_TX  = BoardPins::RS485_TX;
inline constexpr int PIN_RS485_RX  = BoardPins::RS485_RX;
inline constexpr int PIN_SD_D0     = BoardPins::SD_D0;
inline constexpr int PIN_SD_CMD    = BoardPins::SD_CMD;
inline constexpr int PIN_SD_CLK    = BoardPins::SD_CLK;
