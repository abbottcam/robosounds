// Lens Slate: an e-paper lens chart, crew list and notes screen for the
// camera body, running on a LilyGo T5 4.7" e-paper board.
//
// How it behaves:
//   * The board spends almost all of its time in deep sleep. The e-paper keeps
//     showing the last page without using any power.
//   * Short press of the user button: wake, show the next page (Lens -> Crew ->
//     Notes), go back to sleep.
//   * Hold the button for 3 seconds: edit mode. The board starts its own Wi-Fi
//     network and serves an edit page to your phone. Saving updates the screen
//     and puts the board back to sleep.

#ifndef BOARD_HAS_PSRAM
#error "Enable PSRAM (the board definitions in boards/ do this)."
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <driver/rtc_io.h>

#include "epd_driver.h"
#include "utilities.h"

#include "gfx.h"
#include "render.h"
#include "web_page.h"

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

static const char *CONFIG_PATH = "/slate.json";
static const uint32_t HOLD_FOR_EDIT_MS = 3000;
static const uint32_t EDIT_TIMEOUT_MS = 10UL * 60UL * 1000UL;  // 10 minutes

static const size_t MAX_LENSES = 12;
static const size_t MAX_CREW = 10;
static const size_t MAX_NOTES_CHARS = 1500;
static const size_t MAX_FIELD_CHARS = 40;

// Survives deep sleep (not a power cycle).
RTC_DATA_ATTR uint8_t currentPage = PAGE_LENS;

static uint8_t *framebuffer = nullptr;
static JsonDocument config;
static bool showFirstRunHint = false;

// Edit mode state
static WebServer server(80);
static DNSServer dns;
static bool editMode = false;
static bool saveDone = false;
static uint32_t editStartedAt = 0;
static uint32_t lastActivityAt = 0;
static bool buttonArmed = false;

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

// First-boot contents: a lens set laid out like a printed lens card.
static void loadDefaults() {
  config.clear();
  config["set"] = "Zero Optiks Leica R";
  config["flip"] = false;
  JsonArray lenses = config["lenses"].to<JsonArray>();
  const char *demo[][3] = {
      {"19mm", "T3", "1'"},       {"24mm", "T3", "1'"},
      {"28mm", "T3", "1'"},       {"35mm", "T1.6", "1' 8\""},
      {"50mm", "T1.5", "1'"},     {"60mm", "T2.9", "10' \xC2\xBC\""},
      {"80mm", "T1.5", "1' 8\""}, {"90mm", "T2.1", "2'"},
      {"100mm", "T2.9", "17\""},  {"135mm", "T2.9", "2' 11\""},
  };
  for (auto &l : demo) {
    JsonObject o = lenses.add<JsonObject>();
    o["f"] = l[0];
    o["t"] = l[1];
    o["cf"] = l[2];
    o["d"] = "95mm";
  }
  JsonArray crew = config["crew"].to<JsonArray>();
  const char *demoCrew[][3] = {
      {"DP", "Your Name", "+44 7000 000000"},
      {"1st AC", "Name", "+44 7000 000000"},
      {"2nd AC", "Name", "+44 7000 000000"},
  };
  for (auto &c : demoCrew) {
    JsonObject o = crew.add<JsonObject>();
    o["role"] = c[0];
    o["name"] = c[1];
    o["phone"] = c[2];
  }
  config["notes"] =
      "Hold the button for 3 seconds to edit this slate from your phone.\n\n"
      "A short press flips between Lens, Crew and Notes.";
}

static bool loadConfig() {
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) return false;
  DeserializationError err = deserializeJson(config, f);
  f.close();
  return !err;
}

// Truncates to at most maxLen bytes without cutting a UTF-8 character in half.
static String clip(const char *s, size_t maxLen) {
  String out = s ? s : "";
  if (out.length() <= maxLen) return out;
  size_t n = maxLen;
  while (n > 0 && (((uint8_t)out[n]) & 0xC0) == 0x80) n--;
  return out.substring(0, n);
}

// Copies only the fields we know about, with length limits, so a bad request
// can't fill the flash or break the layout.
static bool sanitizeInto(JsonDocument &in, JsonDocument &out) {
  if (!in.is<JsonObject>()) return false;
  out.clear();
  out["set"] = clip(in["set"] | "", MAX_FIELD_CHARS);
  out["flip"] = in["flip"] | false;

  JsonArray lensesOut = out["lenses"].to<JsonArray>();
  for (JsonObject l : in["lenses"].as<JsonArray>()) {
    if (lensesOut.size() >= MAX_LENSES) break;
    JsonObject o = lensesOut.add<JsonObject>();
    o["f"] = clip(l["f"] | "", 12);
    o["t"] = clip(l["t"] | "", 8);
    o["cf"] = clip(l["cf"] | "", 14);
    o["d"] = clip(l["d"] | "", 10);
  }

  JsonArray crewOut = out["crew"].to<JsonArray>();
  for (JsonObject c : in["crew"].as<JsonArray>()) {
    if (crewOut.size() >= MAX_CREW) break;
    JsonObject o = crewOut.add<JsonObject>();
    o["role"] = clip(c["role"] | "", 16);
    o["name"] = clip(c["name"] | "", MAX_FIELD_CHARS);
    o["phone"] = clip(c["phone"] | "", 24);
  }

  out["notes"] = clip(in["notes"] | "", MAX_NOTES_CHARS);
  return true;
}

static bool saveConfig() {
  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) return false;
  bool ok = serializeJson(config, f) > 0;
  f.close();
  return ok;
}

// ---------------------------------------------------------------------------
// Battery
// ---------------------------------------------------------------------------

// Must be called after epd_poweron(): the battery divider is switched by the
// display power rail.
static String batteryLabel() {
  delay(10);
  uint32_t mv = analogReadMilliVolts(BATT_PIN) * 2;  // 1:2 divider on the board
  if (mv > 4350) return "USB";
  if (mv < 3000) return "LOW";
  int pct = constrain(map((long)mv, 3300, 4150, 0, 100), 0, 100);
  return String(pct) + "%";
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

static void pushToScreen() {
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
}

static void showPage(uint8_t page) {
  epd_poweron();
  String battery = batteryLabel();
  gfx::begin(framebuffer, config["flip"] | false);
  renderPage(page, config, battery.c_str(), showFirstRunHint);
  pushToScreen();
  epd_poweroff_all();
}

// ---------------------------------------------------------------------------
// Sleep / button
// ---------------------------------------------------------------------------

static void goToSleep() {
  epd_poweroff_all();
  WiFi.mode(WIFI_OFF);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  rtc_gpio_pullup_en((gpio_num_t)BUTTON_1);
  rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_1);
#endif
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, 0);  // button pulls low
  esp_deep_sleep_start();
}

static bool buttonDown() { return digitalRead(BUTTON_1) == LOW; }

// Returns true if the button is held for HOLD_FOR_EDIT_MS.
static bool heldForEdit() {
  uint32_t start = millis();
  while (buttonDown()) {
    if (millis() - start >= HOLD_FOR_EDIT_MS) return true;
    delay(10);
  }
  return false;
}

// ---------------------------------------------------------------------------
// Edit mode (Wi-Fi access point + web page)
// ---------------------------------------------------------------------------

static String apName() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[24];
  snprintf(buf, sizeof(buf), "SLATE-%04X", (unsigned)((mac >> 32) & 0xFFFF));
  return String(buf);
}

static String apPassword() {
  // Stable per device, printed on the screen in edit mode.
  uint64_t mac = ESP.getEfuseMac();
  uint32_t mixed = (uint32_t)(mac ^ (mac >> 23) ^ 0x5EED5EEDu);
  char buf[12];
  snprintf(buf, sizeof(buf), "%08lu", (unsigned long)(mixed % 100000000UL));
  return String(buf);
}

static void showEditScreen(const String &ssid, const String &pass) {
  epd_poweron();
  gfx::begin(framebuffer, config["flip"] | false);
  renderEditScreen(ssid.c_str(), pass.c_str());
  pushToScreen();
  epd_poweroff_all();
}

static void finishEditMode(bool saved) {
  server.stop();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  if (saved) currentPage = PAGE_LENS;
  showFirstRunHint = false;
  showPage(currentPage);
  goToSleep();
}

static void handleRoot() {
  lastActivityAt = millis();
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", WEB_PAGE);
}

static void handleData() {
  lastActivityAt = millis();
  String out;
  serializeJson(config, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out);
}

static void handleSave() {
  lastActivityAt = millis();
  JsonDocument in;
  if (deserializeJson(in, server.arg("plain"))) {
    server.send(400, "text/plain", "Could not read the data.");
    return;
  }
  JsonDocument clean;
  if (!sanitizeInto(in, clean)) {
    server.send(400, "text/plain", "Unexpected data.");
    return;
  }
  config = clean;
  if (!saveConfig()) {
    server.send(500, "text/plain", "Could not save to flash.");
    return;
  }
  server.send(200, "text/plain", "Saved");
  saveDone = true;
}

// Anything else (including the phone's captive-portal checks) goes to the edit
// page, which makes iOS and Android pop it up automatically.
static void handleRedirect() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

static void startEditMode() {
  editMode = true;
  String ssid = apName();
  String pass = apPassword();
  showEditScreen(ssid, pass);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid.c_str(), pass.c_str());
  dns.start(53, "*", IPAddress(192, 168, 4, 1));

  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleRedirect);
  server.begin();

  editStartedAt = lastActivityAt = millis();
  buttonArmed = false;  // wait for the 3 s hold to be released first
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  // After a wake from deep sleep the button pin is still an RTC pin.
  rtc_gpio_deinit((gpio_num_t)BUTTON_1);
  pinMode(BUTTON_1, INPUT_PULLUP);

  framebuffer = (uint8_t *)ps_calloc(1, EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) {
    Serial.println("PSRAM allocation failed");
    while (true) delay(1000);
  }
  epd_init();

  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
  if (!loadConfig()) {
    loadDefaults();
    saveConfig();
    showFirstRunHint = true;
  }

  bool wokeByButton = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;

  if (wokeByButton) {
    if (heldForEdit()) {
      startEditMode();
      return;  // loop() runs the web server
    }
    currentPage = (currentPage + 1) % PAGE_COUNT;
  } else {
    currentPage = PAGE_LENS;  // fresh power-on or reset
  }

  showPage(currentPage);
  goToSleep();
}

void loop() {
  if (!editMode) {
    goToSleep();
    return;
  }

  dns.processNextRequest();
  server.handleClient();

  if (saveDone) {
    delay(300);  // let the "Saved" response reach the phone
    finishEditMode(true);
  }

  if (!buttonDown()) buttonArmed = true;
  if (buttonArmed && buttonDown()) {
    delay(30);
    if (buttonDown()) finishEditMode(false);
  }

  if (millis() - lastActivityAt > EDIT_TIMEOUT_MS ||
      millis() - editStartedAt > 3 * EDIT_TIMEOUT_MS) {
    finishEditMode(false);
  }
  delay(2);
}
