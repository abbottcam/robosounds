// Renders every screen of the slate to PGM images so the layout can be checked
// without hardware. Usage: preview <config.json> <output-dir>
#include <ArduinoJson.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "gfx.h"
#include "render.h"

static void savePgm(const std::string &path) {
  std::ofstream out(path, std::ios::binary);
  out << "P5\n" << gfx::W << " " << gfx::H << "\n255\n";
  for (int y = 0; y < gfx::H; y++)
    for (int x = 0; x < gfx::W; x++) out.put((char)(gfx::screenPixel(x, y) * 17));
}

int main(int argc, char **argv) {
  if (argc < 3) return 1;
  std::ifstream in(argv[1]);
  std::stringstream ss;
  ss << in.rdbuf();
  JsonDocument config;
  if (deserializeJson(config, ss.str())) return 2;

  std::vector<uint8_t> fb(gfx::FB_BYTES);
  gfx::begin(fb.data(), config["flip"] | false, config["invert"] | false);
  std::string dir = argv[2];
  const char *names[] = {"lens", "cast", "crew", "notes"};
  for (int p = 0; p < PAGE_COUNT; p++) {
    renderPage(p, config, "82%", false);
    savePgm(dir + "/" + names[p] + ".pgm");
  }
  renderEditScreen("SLATE-3F2A", "40718263");
  savePgm(dir + "/edit.pgm");
  return 0;
}
