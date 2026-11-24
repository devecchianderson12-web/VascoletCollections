#pragma once
#include "../platform.h"
#include <string>

namespace vc {

// Fuente de mapa de bits de Vascolet 2 (lang/fonts/*.png + .xml con la posición de cada letra).
class Font {
public:
    bool load(const std::string& base); // ej: "v2/assets/lang/fonts/PalatinoLinotype"
    float width(const std::string& utf8, float scale = 1.0f) const;
    float height(float scale = 1.0f) const { return lineH * scale; }
    void draw(const std::string& utf8, float x, float y, float scale = 1.0f, SDL_Color c = { 255, 255, 255, 255 }) const;
    void drawCentered(const std::string& utf8, float cx, float y, float scale = 1.0f, SDL_Color c = { 255, 255, 255, 255 }) const {
        draw(utf8, cx - width(utf8, scale) * 0.5f, y, scale, c);
    }
private:
    struct Glyph { int x = 0, y = 0, w = 0, h = 0; bool ok = false; };
    Glyph glyphs[256];
    plat::Image* img = nullptr;
    float lineH = 28;
    const Glyph* glyph(unsigned cp) const;
};

Font& uiFont();     // texto normal
Font& titleFont();  // títulos

}
