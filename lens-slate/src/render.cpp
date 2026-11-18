#include "render.h"

#include <stdio.h>
#include <string.h>

#include "gfx.h"

// Fonts that come with the LilyGo library. FiraSans also has Latin-1
// characters such as ¼ ½ ¾ Ø and accented letters; the Roboto fonts are
// plain ASCII.
#include "firasans.h"
#include "roboto12.h"
#include "roboto18.h"
#include "roboto32.h"

using namespace gfx;

static const GFXfont *BODY = &FiraSans;
static const GFXfont *SMALL = &Roboto12;

static const char *PAGE_NAMES[PAGE_COUNT] = {"LENS", "CREW", "NOTES"};

// Layout (portrait, 540 x 960)
static const int HEADER_H = 58;
static const int MARGIN = 10;
static const int CONTENT_W = W - 2 * MARGIN;
static const int BODY_TOP = HEADER_H + 16;
static const int FOOTER_Y = 912;

// ---------------------------------------------------------------------------

static void drawTabs(uint8_t active) {
  const int tabW = W / PAGE_COUNT;
  for (uint8_t i = 0; i < PAGE_COUNT; i++) {
    int x = i * tabW;
    int base = centredBaseline(&Roboto18, 0, HEADER_H);
    if (i == active) {
      fillRect(x, 0, tabW, HEADER_H, BLACK);
      textCentered(&Roboto18, PAGE_NAMES[i], x + tabW / 2, base, WHITE);
    } else {
      textCentered(&Roboto18, PAGE_NAMES[i], x + tabW / 2, base, DARK);
    }
  }
  hline(0, HEADER_H, W, 3);
}

static void drawFooter(const char *battery, bool hint) {
  hline(0, FOOTER_Y, W, 2);
  int base = FOOTER_Y + 34;
  if (hint) text(SMALL, "Hold button 3 s to edit", MARGIN, base, DARK);
  textRight(SMALL, battery, W - MARGIN, base);
}

static void emptyMessage(const char *what) {
  char msg[64];
  snprintf(msg, sizeof(msg), "No %s yet.", what);
  textCentered(BODY, msg, W / 2, 440);
  textCentered(SMALL, "Hold the button for 3 seconds to edit.", W / 2, 490, DARK);
}

static void cellText(const char *s, int x, int w, int top, int h) {
  char buf[48];
  fit(BODY, s, w - 10, buf, sizeof(buf));
  textCentered(BODY, buf, x + w / 2, centredBaseline(BODY, top, h));
}

// ---------------------------------------------------------------------------
// Lens chart: a table like a printed lens card.
//
//   +-----------------------------+
//   |     Zero Optiks Leica R     |
//   +-------+-----+--------+------+
//   | LENS  | T/S |  C/F   |  Ø   |
//   +-------+-----+--------+      |
//   | 19mm  | T3  |   1'   | 95mm |   <- Ø is one merged cell when every
//   | ...   |     |        |      |      lens has the same front diameter
//   +-------+-----+--------+------+
// ---------------------------------------------------------------------------

static void drawLensPage(JsonDocument &cfg) {
  JsonArray lenses = cfg["lenses"].as<JsonArray>();
  const int n = lenses.size();

  const int top = BODY_TOP;
  const int titleH = 66;
  const int headH = 54;
  const int rowsTop = top + titleH + headH;
  const int avail = FOOTER_Y - 16 - rowsTop;
  const int rowH = n > 0 ? (avail / n < 66 ? avail / n : 66) : 0;

  // Diameter column: hidden if nobody filled it in, merged if all the same.
  bool anyDia = false, sameDia = true;
  const char *firstDia = nullptr;
  for (JsonObject l : lenses) {
    const char *d = l["d"] | "";
    if (*d) anyDia = true;
    if (!firstDia) firstDia = d;
    else if (strcmp(firstDia, d) != 0) sameDia = false;
  }
  const bool merged = anyDia && sameDia && n > 1;
  const int cols = anyDia ? 4 : 3;
  static const char *heads[4] = {"LENS", "T/S", "C/F", "\xC3\x98"};  // Ø
  static const char *keys[4] = {"f", "t", "cf", "d"};

  // Column widths: widest content + padding, spare space shared out evenly.
  int colW[4] = {0, 0, 0, 0};
  int total = 0;
  for (int c = 0; c < cols; c++) {
    int need = textWidth(BODY, heads[c]);
    for (JsonObject l : lenses) {
      int w = textWidth(BODY, l[keys[c]] | "");
      if (w > need) need = w;
    }
    colW[c] = need + 16;
    total += colW[c];
  }
  if (total <= CONTENT_W) {
    int extra = CONTENT_W - total;
    for (int c = 0; c < cols; c++) colW[c] += extra / cols;
    colW[cols - 1] += extra % cols;
  } else {
    int used = 0;
    for (int c = 0; c < cols; c++) {
      colW[c] = colW[c] * CONTENT_W / total;
      used += colW[c];
    }
    colW[cols - 1] += CONTENT_W - used;
  }
  int colX[5];
  colX[0] = MARGIN;
  for (int c = 0; c < cols; c++) colX[c + 1] = colX[c] + colW[c];

  const int bottom = rowsTop + n * rowH;

  // Title row
  char title[64];
  fit(BODY, cfg["set"] | "", CONTENT_W - 24, title, sizeof(title));
  textCentered(BODY, title, W / 2, centredBaseline(BODY, top, titleH));
  hline(MARGIN, top + titleH, CONTENT_W, 2);

  // Header row
  for (int c = 0; c < cols; c++) cellText(heads[c], colX[c], colW[c], top + titleH, headH);
  hline(MARGIN, rowsTop, CONTENT_W, 2);

  // Rows
  int i = 0;
  for (JsonObject l : lenses) {
    int y = rowsTop + i * rowH;
    for (int c = 0; c < cols; c++) {
      if (c == 3 && merged) continue;
      cellText(l[keys[c]] | "", colX[c], colW[c], y, rowH);
    }
    if (i < n - 1) {
      int lineW = (merged ? colX[3] : colX[cols]) - MARGIN;
      hline(MARGIN, y + rowH - 1, lineW, 2);
    }
    i++;
  }
  if (merged) cellText(firstDia, colX[3], colW[3], rowsTop, n * rowH);

  // Grid
  for (int c = 1; c < cols; c++) vline(colX[c] - 1, top + titleH, bottom - top - titleH, 2);
  frame(MARGIN, top, CONTENT_W, bottom - top + 2, 3);

  if (n == 0) emptyMessage("lenses");
}

// ---------------------------------------------------------------------------

static void drawCrewPage(JsonDocument &cfg) {
  JsonArray crew = cfg["crew"].as<JsonArray>();
  const int n = crew.size();
  if (n == 0) {
    emptyMessage("crew");
    return;
  }
  int rowH = (FOOTER_Y - 16 - BODY_TOP) / n;
  if (rowH > 92) rowH = 92;

  int i = 0;
  for (JsonObject c : crew) {
    // Line 1: ROLE (small, grey) ............ phone
    // Line 2: Name (large, full width)
    int top = BODY_TOP + i * rowH;
    int line1 = top + rowH * 36 / 100;
    int line2 = top + rowH - 14;

    const char *phone = c["phone"] | "";
    int phoneW = textWidth(&Roboto18, phone);
    textRight(&Roboto18, phone, W - MARGIN, line1, DARK);

    char role[40];
    fit(SMALL, c["role"] | "", CONTENT_W - phoneW - 20, role, sizeof(role));
    for (char *p = role; *p; p++)
      if (*p >= 'a' && *p <= 'z') *p -= 32;
    text(SMALL, role, MARGIN, line1, DARK);

    char name[64];
    fit(BODY, c["name"] | "", CONTENT_W, name, sizeof(name));
    text(BODY, name, MARGIN, line2);

    if (i < n - 1) hline(MARGIN, top + rowH - 1, CONTENT_W, 1, MID);
    i++;
  }
}

// ---------------------------------------------------------------------------

static void drawNotesPage(JsonDocument &cfg) {
  const char *notes = cfg["notes"] | "";
  if (!*notes) {
    emptyMessage("notes");
    return;
  }
  const int lineH = BODY->advance_y;
  const int lastBaseline = FOOTER_Y - 18;
  int y = BODY_TOP + BODY->ascender;

  char line[512] = "";
  char candidate[512];
  const char *p = notes;

  auto flush = [&]() {
    if (y <= lastBaseline) text(BODY, line, MARGIN, y);
    y += lineH;
    line[0] = 0;
  };

  while (*p && y <= lastBaseline) {
    if (*p == '\n') {
      if (line[0]) flush();
      else y += lineH / 2;  // blank line = half-height gap
      p++;
      continue;
    }
    if (*p == ' ') {
      p++;
      continue;
    }
    // Next word
    const char *start = p;
    while (*p && *p != ' ' && *p != '\n') p++;
    int len = p - start;
    if (len > 200) len = 200;

    snprintf(candidate, sizeof(candidate), "%s%s%.*s", line, line[0] ? " " : "", len, start);
    if (textWidth(BODY, candidate) <= CONTENT_W || !line[0]) {
      strncpy(line, candidate, sizeof(line) - 1);
      line[sizeof(line) - 1] = 0;
    } else {
      flush();
      snprintf(line, sizeof(line), "%.*s", len, start);
    }
  }
  if (line[0]) flush();
}

// ---------------------------------------------------------------------------

void renderPage(uint8_t page, JsonDocument &config, const char *battery, bool firstRunHint) {
  clear();
  drawTabs(page);
  switch (page) {
    case PAGE_LENS: drawLensPage(config); break;
    case PAGE_CREW: drawCrewPage(config); break;
    default: drawNotesPage(config); break;
  }
  drawFooter(battery, firstRunHint);
}

void renderEditScreen(const char *ssid, const char *password) {
  clear();
  fillRect(0, 0, W, HEADER_H, BLACK);
  textCentered(&Roboto18, "EDIT MODE", W / 2, centredBaseline(&Roboto18, 0, HEADER_H), WHITE);

  int y = 150;
  text(BODY, "1. On your phone,", MARGIN + 8, y);
  y += 50;
  text(BODY, "join this Wi-Fi:", MARGIN + 8, y);

  y += 50;
  fillRect(MARGIN, y, CONTENT_W, 190, BLACK);
  textCentered(SMALL, "NETWORK", W / 2, y + 40, LIGHT);
  textCentered(&Roboto32, ssid, W / 2, y + 100, WHITE);
  char pw[48];
  snprintf(pw, sizeof(pw), "password  %s", password);
  textCentered(&Roboto18, pw, W / 2, y + 160, WHITE);

  y += 270;
  text(BODY, "2. The edit page opens", MARGIN + 8, y);
  y += 50;
  text(BODY, "by itself. If not, go to:", MARGIN + 8, y);
  y += 70;
  textCentered(&Roboto32, "192.168.4.1", W / 2, y);

  y += 120;
  textCentered(SMALL, "Press the button to leave without saving.", W / 2, y, DARK);
  textCentered(SMALL, "Switches off by itself after 10 minutes.", W / 2, y + 36, DARK);
}
