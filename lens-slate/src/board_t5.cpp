// LilyGo T5 4.7" (original ESP32 or ESP32-S3 version), LilyGo-EPD47 library.
#if defined(SLATE_BOARD_T5)

#include <driver/rtc_io.h>

#include "board.h"
#include "epd_driver.h"
#include "utilities.h"

void boardBegin() {
  // After a wake from deep sleep the button pin is still an RTC pin.
  rtc_gpio_deinit((gpio_num_t)BUTTON_1);
  pinMode(BUTTON_1, INPUT_PULLUP);
  epd_init();
}

int boardButtonPin() { return BUTTON_1; }

String boardBattery() {
  // The battery divider is switched by the display power rail.
  epd_poweron();
  delay(10);
  uint32_t mv = analogReadMilliVolts(BATT_PIN) * 2;  // 1:2 divider
  epd_poweroff();
  if (mv > 4350) return "USB";
  if (mv < 3000) return "LOW";
  int pct = constrain(map((long)mv, 3300, 4150, 0, 100), 0, 100);
  return String(pct) + "%";
}

void boardShow(const uint8_t *framebuffer) {
  epd_poweron();
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), (uint8_t *)framebuffer);
  epd_poweroff_all();
}

void boardSleep() {
  epd_poweroff_all();
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  rtc_gpio_pullup_en((gpio_num_t)BUTTON_1);
  rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_1);
#endif
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, 0);
  esp_deep_sleep_start();
}

#endif
