#pragma once
// Page layouts. Pure drawing code (no hardware), so it can also be built on a
// computer to produce preview images: see tools/preview.

#include <ArduinoJson.h>
#include <stdint.h>

enum Page : uint8_t { PAGE_LENS = 0, PAGE_CREW = 1, PAGE_NOTES = 2, PAGE_COUNT = 3 };

// Draws a full page into the framebuffer given to gfx::begin().
void renderPage(uint8_t page, JsonDocument &config, const char *battery, bool firstRunHint);

// Instructions shown while the Wi-Fi edit page is running.
void renderEditScreen(const char *ssid, const char *password);
