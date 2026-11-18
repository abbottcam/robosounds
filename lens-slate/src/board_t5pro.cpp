// LilyGo T5 4.7" S3 E-Paper Pro (the one with the case and built-in battery),
// using the FastEPD library.
//
// Two hardware revisions exist; the revision is printed on the circuit board.
//   H752-01 (current): TPS65185 display power chip, I2C on GPIO 39/40.
//   H752    (older):   shift-register display power, I2C on GPIO 6/5.
//                      Build with -DSLATE_PRO_H752 (env T5-ePaper-S3-Pro-H752).
// Pin numbers below are from LilyGo's T5S3-4.7-e-paper-PRO repository.
#if defined(SLATE_BOARD_T5PRO)

#include <FastEPD.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>

#include "board.h"

namespace {

#if defined(SLATE_PRO_H752)
constexpr int PIN_SDA = 6;
constexpr int PIN_SCL = 5;
constexpr int PIN_LORA_CS = 46;
constexpr int PIN_LORA_RST = 43;
constexpr int PIN_SD_CS = 16;
constexpr int PIN_TOUCH_RST = 41;
constexpr int PIN_FRONTLIGHT = 40;
#else
constexpr int PIN_SDA = 39;
constexpr int PIN_SCL = 40;
constexpr int PIN_LORA_CS = 46;
constexpr int PIN_LORA_RST = 1;
constexpr int PIN_SD_CS = 12;
constexpr int PIN_TOUCH_RST = 9;
constexpr int PIN_FRONTLIGHT = 11;
#endif
constexpr int PIN_BOOT_BUTTON = 0;  // the side button marked BOOT

constexpr uint8_t FUEL_GAUGE_ADDR = 0x55;  // BQ27220
constexpr uint8_t CMD_CURRENT = 0x0C;      // mA, signed, positive = charging
constexpr uint8_t CMD_STATE_OF_CHARGE = 0x2C;  // %

// Pins we park in a fixed state during deep sleep (as LilyGo's own firmware
// does), so the radio and touch chip stay in reset and the light stays off.
const int HELD_PINS[] = {PIN_LORA_RST, PIN_TOUCH_RST, PIN_FRONTLIGHT};

FASTEPD epaper;
bool displayReady = false;

// FastEPD gives the LCD peripheral a spare "D/C" pin it never really uses;
// on the H752-01 that's GPIO 0, the BOOT button. Take it back for the button
// after FastEPD has set the bus up.
void reclaimButton() {
  gpio_reset_pin((gpio_num_t)PIN_BOOT_BUTTON);
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
}

// The display is started only when first needed, so the button can be read
// (short press vs. 3 s hold) before FastEPD touches GPIO 0.
void ensureDisplay() {
  if (displayReady) return;
#if defined(SLATE_PRO_H752)
  epaper.initPanel(BB_PANEL_LILYGO_T5PRO);
  Wire.begin(PIN_SDA, PIN_SCL);
#else
  // Same set-up as LilyGo's FastEPD examples for this board. FastEPD also
  // starts the I2C bus (39/40), which the fuel gauge shares.
  epaper.initPanel(BB_PANEL_EPDIY_V7, 26666666);
  epaper.setPanelSize(960, 540);
#endif
  epaper.setMode(BB_MODE_4BPP);
  reclaimButton();
  displayReady = true;
}

bool readGauge(uint8_t cmd, int16_t &value) {
  Wire.beginTransmission(FUEL_GAUGE_ADDR);
  Wire.write(cmd);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(FUEL_GAUGE_ADDR, (uint8_t)2) != 2) return false;
  uint8_t lo = Wire.read();
  uint8_t hi = Wire.read();
  value = (int16_t)(lo | (hi << 8));
  return true;
}

}  // namespace

void boardBegin() {
  // Release the pins we parked before sleeping.
  for (int pin : HELD_PINS) gpio_hold_dis((gpio_num_t)pin);
  gpio_deep_sleep_hold_dis();

  // LoRa and the SD card share the SPI bus: keep both deselected.
  pinMode(PIN_LORA_CS, OUTPUT);
  digitalWrite(PIN_LORA_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  rtc_gpio_deinit((gpio_num_t)PIN_BOOT_BUTTON);
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
}

int boardButtonPin() { return PIN_BOOT_BUTTON; }

String boardBattery() {
  ensureDisplay();  // also starts the I2C bus the fuel gauge is on
  int16_t soc, current;
  if (!readGauge(CMD_STATE_OF_CHARGE, soc) || soc < 0 || soc > 100) return "";
  String label = String(soc) + "%";
  if (readGauge(CMD_CURRENT, current) && current > 20) label = "CHG " + label;
  return label;
}

void boardShow(const uint8_t *framebuffer) {
  ensureDisplay();
  // Copy our framebuffer (even x in the low nibble, 0 = black) into FastEPD's.
  for (int y = 0; y < 540; y++) {
    const uint8_t *row = framebuffer + y * 480;
    for (int x = 0; x < 960; x += 2) {
      uint8_t b = row[x / 2];
      epaper.drawPixelFast(x, y, b & 0x0F);
      epaper.drawPixelFast(x + 1, y, b >> 4);
    }
  }
  epaper.fullUpdate(CLEAR_FAST);  // also switches the panel's high voltage off
  reclaimButton();
}

void boardSleep() {
  if (displayReady) {
    epaper.einkPower(0);
    epaper.deInit();
  }

  pinMode(PIN_LORA_RST, OUTPUT);
  digitalWrite(PIN_LORA_RST, LOW);
  pinMode(PIN_TOUCH_RST, OUTPUT);
  digitalWrite(PIN_TOUCH_RST, LOW);
  pinMode(PIN_FRONTLIGHT, OUTPUT);
  digitalWrite(PIN_FRONTLIGHT, LOW);
  for (int pin : HELD_PINS) gpio_hold_en((gpio_num_t)pin);
  gpio_deep_sleep_hold_en();

  rtc_gpio_pullup_en((gpio_num_t)PIN_BOOT_BUTTON);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_BOOT_BUTTON);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BOOT_BUTTON, 0);
  esp_deep_sleep_start();
}

#endif
