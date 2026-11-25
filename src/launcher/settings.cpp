#include "settings.h"
#include "../platform.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>

namespace vc {

const int STICK_SIZES[4] = { 75, 100, 125, 150 };
const char* const STICK_THEMES[5] = { "Rojo", "Lima", "Cian", "Fuego", "Blanco" };
const char* const TOUCH_STYLES[3] = { "Brillante", "Plano", "Aro dorado" };

Settings& settings() { static Settings s; return s; }

// Se guarda como texto "clave=valor" para poder agregar opciones sin romper los guardados viejos.
static const char* HEADER = "VascoletCollection 1";

struct Field { const char* key; int Settings::* ptr; int lo, hi; };
static const Field FIELDS[] = {
    { "music", &Settings::music, 0, 100 }, { "sfx", &Settings::sfx, 0, 100 },
    { "fullscreen", &Settings::fullscreen, 0, 1 }, { "smooth", &Settings::smooth, 0, 1 },
    { "sharpness", &Settings::sharpness, 0, 2 }, { "hd", &Settings::hd, 0, 1 },
    { "tv", &Settings::tvEffect, 0, 1 }, { "camFixed", &Settings::camFixed, 0, 1 }, { "stretch", &Settings::stretch, 0, 1 },
    { "fps120", &Settings::fps120, 0, 1 }, { "modernArt", &Settings::modernArt, 0, 1 },
    { "hdOff", &Settings::hdOff, 0, 255 }, { "stickType", &Settings::stickType, 0, 1 },
    { "touchStyle", &Settings::touchStyle, 0, 2 }, { "stickSize", &Settings::stickSize, 0, 3 },
    { "touchColor", &Settings::stickTheme, 0, 4 }, { "touchAlpha", &Settings::touchAlpha, 0, 100 },
    { "v1Unlocked", &Settings::v1Unlocked, 1, 5 },
};

void Settings::load() {
    auto d = plat::saveRead();
    std::string text(d.begin(), d.end()), line;
    if (text.compare(0, strlen(HEADER), HEADER) != 0) return;
    std::istringstream in(text);
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        for (const Field& f : FIELDS)
            if (k == f.key) this->*f.ptr = std::max(f.lo, std::min(f.hi, atoi(v.c_str())));
        if (k == "touchPos") {
            std::istringstream vs(v);
            for (auto& p : touchPos) { unsigned u = 0xFFFF; if (vs >> u) p = (uint16_t)std::min(u, 0xFFFFu); }
        }
        if (k == "v1Coins") {
            std::istringstream vs(v);
            for (auto& c : v1Coins) { int n = 0; if (vs >> n) c = std::max(0, std::min(100, n)); }
        }
    }
}

void Settings::save() const {
    std::string out = std::string(HEADER) + "\n";
    for (const Field& f : FIELDS) out += std::string(f.key) + "=" + std::to_string(this->*f.ptr) + "\n";
    out += "touchPos=";
    for (int i = 0; i < 10; i++) out += (i ? " " : "") + std::to_string(touchPos[i]);
    out += "\nv1Coins=";
    for (int i = 0; i < 5; i++) out += (i ? " " : "") + std::to_string(v1Coins[i]);
    out += "\n";
    plat::saveWrite(std::vector<uint8_t>(out.begin(), out.end()));
    plat::flushSave();
}

void Settings::apply() const {
    plat::setMusicVolume(music * 128 / 100);
    plat::setSfxVolume(sfx * 128 / 100);
    plat::setFullscreen(fullscreen != 0);
    plat::setSmooth(smooth != 0);
    plat::setTvEffect(tvEffect != 0);
    plat::setImageQuality(sharpness, hd != 0);
    plat::setTouchOpacity(touchAlpha);
    plat::setTouchStick(STICK_SIZES[stickSize & 3], stickTheme, stickType, touchStyle);
    plat::setTouchLayout(touchPos);
    plat::setHighFps(fps120 != 0);
    plat::setModernArt(modernArt != 0);
}

}
