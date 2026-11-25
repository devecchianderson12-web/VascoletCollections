#include "font.h"
#include "tinyxml2.h"

namespace vc {

// Decodifica UTF-8; los caracteres fuera de Latin-1 se devuelven como '?'.
static unsigned nextCp(const std::string& s, size_t& i) {
    const unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    // Byte suelto de Latin-1 (no es el comienzo de un UTF-8 válido): se usa tal cual.
    const bool cont = i < s.size() && ((unsigned char)s[i] & 0xC0) == 0x80;
    if ((c & 0xC0) == 0x80 || !cont) return c;
    int extra = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : 0;
    unsigned cp = c & (0x3F >> extra);
    for (int k = 0; k < extra && i < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
    switch (cp) {   // comillas y signos tipográficos: se usa el equivalente simple
        case 0x2018: case 0x2019: return '\'';
        case 0x201C: case 0x201D: case 0x201E: return '"';
        case 0x2013: case 0x2014: return '-';
        case 0x2026: return '.';
    }
    return cp < 256 ? cp : '?';
}

bool Font::load(const std::string& base) {
    auto xml = plat::assetRead(base + ".xml");
    img = plat::loadImage(base + ".png");
    if (xml.empty() || !img) { plat::log("font %s missing", base.c_str()); return false; }
    tinyxml2::XMLDocument doc;
    // El archivo empieza con BOM: se saltea.
    size_t off = xml.size() >= 3 && xml[0] == 0xEF && xml[1] == 0xBB && xml[2] == 0xBF ? 3 : 0;
    if (doc.Parse((const char*)xml.data() + off, xml.size() - off) != tinyxml2::XML_SUCCESS) return false;
    auto* root = doc.FirstChildElement("fontMetrics");
    if (!root) return false;
    for (auto* c = root->FirstChildElement("character"); c; c = c->NextSiblingElement("character")) {
        const unsigned key = c->UnsignedAttribute("key");
        if (key >= 256) continue;
        auto val = [&](const char* n) { auto* e = c->FirstChildElement(n); return e ? e->IntText() : 0; };
        Glyph& g = glyphs[key];
        g.x = val("x"); g.y = val("y"); g.w = val("width"); g.h = val("height"); g.ok = g.w > 0 && g.h > 0;
        if (g.ok) lineH = (float)g.h;
    }
    return true;
}

const Font::Glyph* Font::glyph(unsigned cp) const {
    if (cp < 256 && glyphs[cp].ok) return &glyphs[cp];
    // Mayúsculas acentuadas que falten: se usa la letra sin acento.
    static const char* PLAIN = "AAAAAAACEEEEIIII";
    if (cp >= 192 && cp < 208 && glyphs[(unsigned char)PLAIN[cp - 192]].ok) return &glyphs[(unsigned char)PLAIN[cp - 192]];
    return glyphs['?'].ok ? &glyphs['?'] : nullptr;
}

float Font::width(const std::string& s, float scale) const {
    float w = 0;
    for (size_t i = 0; i < s.size();) if (const Glyph* g = glyph(nextCp(s, i))) w += g->w;
    return w * scale;
}

void Font::draw(const std::string& s, float x, float y, float scale, SDL_Color c) const {
    if (!img) return;
    plat::setDrawTint(c);
    for (size_t i = 0; i < s.size();) {
        const Glyph* g = glyph(nextCp(s, i));
        if (!g) continue;
        plat::drawImage(img, SDL_Rect{ g->x, g->y, g->w, g->h }, SDL_FRect{ x, y, g->w * scale, g->h * scale });
        x += g->w * scale;
    }
    plat::setDrawTint({ 255, 255, 255, 255 });
}

Font& uiFont() {
    static Font f; static bool done = false;
    if (!done) { done = true; f.load("data/ui/font_v1"); }
    return f;
}
Font& titleFont() {
    static Font f; static bool done = false;
    if (!done) { done = true; f.load("data/ui/font_v1"); }
    return f;
}

}
