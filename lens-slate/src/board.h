#pragma once
// Everything that differs between the supported boards. Each board_*.cpp file
// implements these for one board, chosen by a build flag in platformio.ini:
//
//   SLATE_BOARD_T5      LilyGo T5 4.7" (ESP32 or ESP32-S3), LilyGo-EPD47 library
//   SLATE_BOARD_T5PRO   LilyGo T5 4.7" S3 E-Paper Pro, FastEPD library
//                       (add SLATE_PRO_H752 for the older H752 hardware)

#include <Arduino.h>

// Call first, before anything else touches the hardware.
void boardBegin();

// The button that wakes the board and flips pages (pressed = LOW).
int boardButtonPin();

// Battery as a short label: "82%", "CHG 82%", "USB", or "" if unknown.
String boardBattery();

// Shows a framebuffer in gfx layout (960 x 540, 4 bits per pixel).
void boardShow(const uint8_t *framebuffer);

// Powers everything down and deep-sleeps until the button is pressed.
[[noreturn]] void boardSleep();
