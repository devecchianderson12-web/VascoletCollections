#include "map.h"
#include <tinyxml2.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace v2 {

static void readProps(tinyxml2::XMLElement* parent, Props& out) {
    auto* ps = parent->FirstChildElement("properties");
    for (auto* p = ps ? ps->FirstChildElement("property") : nullptr; p; p = p->NextSiblingElement("property")) {
        const char* n = p->Attribute("name"); const char* v = p->Attribute("value");
        if (n) out[n] = v ? v : "";
    }
}

static bool parse(tinyxml2::XMLDocument& doc, const std::string& path) {
    auto d = plat::assetRead(path);
    if (d.empty()) { plat::log("v2: falta %s", path.c_str()); return false; }
    size_t off = d.size() >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF ? 3 : 0;
    return doc.Parse((const char*)d.data() + off, d.size() - off) == tinyxml2::XML_SUCCESS;
}

static std::string dirOf(const std::string& p) { const size_t s = p.find_last_of('/'); return s == std::string::npos ? "" : p.substr(0, s + 1); }

// "a/b/../c" → "a/c"
static std::string normPath(const std::string& p) {
    std::vector<std::string> parts;
    size_t i = 0;
    while (i <= p.size()) {
        size_t j = p.find('/', i);
        if (j == std::string::npos) j = p.size();
        const std::string s = p.substr(i, j - i);
        if (s == "..") { if (!parts.empty()) parts.pop_back(); }
        else if (!s.empty() && s != ".") parts.push_back(s);
        i = j + 1;
    }
    std::string r;
    for (size_t k = 0; k < parts.size(); k++) { if (k) r += '/'; r += parts[k]; }
    return r;
}

static plat::Image* tilesetImage(const std::string& path) {
    static std::map<std::string, plat::Image*> cache;
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    plat::Image* i = plat::loadTileset(path, 32, 32);
    cache[path] = i;
    return i;
}

static bool loadTsx(const std::string& path, Tileset& ts) {
    struct Cached { plat::Image* img; int cols, count; std::vector<uint8_t> flags; };
    static std::map<std::string, Cached> cache;
    auto it = cache.find(path);
    if (it == cache.end()) {
        tinyxml2::XMLDocument doc;
        if (!parse(doc, path)) return false;
        auto* root = doc.FirstChildElement("tileset");
        if (!root) return false;
        Cached c{ nullptr, 16, 0, {} };
        if (auto* im = root->FirstChildElement("image")) {
            const char* src = im->Attribute("source");
            c.img = tilesetImage(normPath(dirOf(path) + (src ? src : "")));
            c.cols = std::max(1, im->IntAttribute("width", 512) / 32);
            c.count = c.cols * (im->IntAttribute("height", 1024) / 32);
        }
        c.flags.assign(c.count, 0);
        for (auto* t = root->FirstChildElement("tile"); t; t = t->NextSiblingElement("tile")) {
            const int id = t->IntAttribute("id", -1);
            if (id < 0 || id >= c.count) continue;
            Props p; readProps(t, p);
            uint8_t f = 0;
            if (p.count("solid")) f |= T_SOLID;
            if (p.count("up")) f |= T_UP;
            if (p.count("damage")) f |= T_DAMAGE;
            if (p.count("water")) f |= T_WATER;
            if (p.count("invisible")) f |= T_INVISIBLE;
            c.flags[id] = f;
        }
        it = cache.emplace(path, std::move(c)).first;
    }
    ts.img = it->second.img; ts.cols = it->second.cols; ts.count = it->second.count; ts.flags = it->second.flags;
    return true;
}

bool TileMap::load(const std::string& path) {
    tinyxml2::XMLDocument doc;
    if (!parse(doc, path)) return false;
    auto* root = doc.FirstChildElement("map");
    if (!root) return false;
    w = root->IntAttribute("width"); h = root->IntAttribute("height");
    tw = root->IntAttribute("tilewidth", 32); th = root->IntAttribute("tileheight", 32);
    readProps(root, props);
    const std::string dir = dirOf(path);
    for (auto* e = root->FirstChildElement(); e; e = e->NextSiblingElement()) {
        const std::string tag = e->Name();
        if (tag == "tileset") {
            Tileset ts;
            ts.firstGid = e->IntAttribute("firstgid", 1);
            if (const char* src = e->Attribute("source")) loadTsx(normPath(dir + src), ts);
            tilesets.push_back(std::move(ts));
        } else if (tag == "layer") {
            Layer l;
            l.name = e->Attribute("name") ? e->Attribute("name") : "";
            l.w = e->IntAttribute("width"); l.h = e->IntAttribute("height");
            l.visible = e->IntAttribute("visible", 1) != 0;
            Props p; readProps(e, p);
            if (p.count("hScale")) l.hScale = (float)atof(p["hScale"].c_str());
            if (p.count("vScale")) l.vScale = (float)atof(p["vScale"].c_str());
            l.gids.assign((size_t)l.w * l.h, 0);
            if (auto* d = e->FirstChildElement("data")) {
                const char* s = d->GetText();
                size_t i = 0;
                while (s && *s && i < l.gids.size()) {
                    while (*s && (*s < '0' || *s > '9')) s++;
                    if (!*s) break;
                    l.gids[i++] = (int)(strtoul(s, (char**)&s, 10) & 0x1FFFFFFF);
                }
            }
            layers.push_back(std::move(l));
        } else if (tag == "objectgroup") {
            const std::string gname = e->Attribute("name") ? e->Attribute("name") : "";
            auto& list = groups[gname];
            for (auto* o = e->FirstChildElement("object"); o; o = o->NextSiblingElement("object")) {
                MapObject m;
                m.name = o->Attribute("name") ? o->Attribute("name") : "";
                m.type = o->Attribute("type") ? o->Attribute("type") : "";
                m.x = o->FloatAttribute("x"); m.y = o->FloatAttribute("y");
                m.w = o->FloatAttribute("width"); m.h = o->FloatAttribute("height");
                readProps(o, m.props);
                list.push_back(std::move(m));
            }
        }
    }
    return w > 0 && h > 0;
}

const Layer* TileMap::layer(const std::string& n) const {
    for (auto& l : layers) if (l.name == n) return &l;
    return nullptr;
}

uint8_t TileMap::flagsOfGid(int gid) const {
    if (gid <= 0) return 0;
    for (int i = (int)tilesets.size() - 1; i >= 0; i--) {
        const Tileset& t = tilesets[i];
        if (gid >= t.firstGid) { const int id = gid - t.firstGid; return id < (int)t.flags.size() ? t.flags[id] : 0; }
    }
    return 0;
}

void TileMap::drawTile(int gid, float x, float y, float size, uint8_t alpha) const {
    if (gid <= 0) return;
    for (int i = (int)tilesets.size() - 1; i >= 0; i--) {
        const Tileset& t = tilesets[i];
        if (gid < t.firstGid) continue;
        const int id = gid - t.firstGid;
        if (!t.img || id >= t.count) return;
        if (alpha != 255) plat::setDrawTint({ 255, 255, 255, alpha });
        plat::drawTile(t.img, id, SDL_FRect{ x, y, size, size });
        if (alpha != 255) plat::setDrawTint({ 255, 255, 255, 255 });
        return;
    }
}

void TileMap::drawView(const Layer& l, float cx, float cy, int vw, int vh, const std::function<uint8_t(int, int)>& alpha) const {
    const int x0 = std::max(0, (int)std::floor(cx / 32)), y0 = std::max(0, (int)std::floor(cy / 32));
    const int x1 = std::min(l.w - 1, (int)((cx + vw) / 32)), y1 = std::min(l.h - 1, (int)((cy + vh) / 32));
    const float fx = std::floor(cx), fy = std::floor(cy);
    for (int ty = y0; ty <= y1; ty++)
        for (int tx = x0; tx <= x1; tx++) {
            const int g = l.at(tx, ty);
            if (g <= 0 || (flagsOfGid(g) & T_INVISIBLE)) continue;
            const uint8_t a = alpha ? alpha(tx, ty) : 255;
            if (a) drawTile(g, tx * 32 - fx, ty * 32 - fy, 32, a);
        }
}

void drawParallax(const TileMap& back, const TileMap& xo, float cx, float cy, int vw, int vh) {
    std::vector<std::pair<const TileMap*, const Layer*>> ls;
    for (const TileMap* bm : { &back, &xo })
        for (const Layer& l : bm->layers) if (l.visible) ls.push_back({ bm, &l });
    std::stable_sort(ls.begin(), ls.end(), [](auto& a, auto& b) { return a.second->hScale < b.second->hScale; });
    for (auto& e : ls) {
        const TileMap* bm = e.first; const Layer& l = *e.second;
        const float pw = l.w * 32.0f;
        float ox = -std::fmod(cx * l.hScale, pw); if (ox > 0) ox -= pw;
        const float oy = -std::max(0.0f, std::min(cy * (l.vScale < 1 ? l.vScale : 0.0f), (float)l.h * 32 - vh));
        for (float bx = ox; bx < vw; bx += pw)
            for (int ty = 0; ty < l.h; ty++)
                for (int txi = 0; txi < l.w; txi++) {
                    const int g = l.at(txi, ty);
                    if (g <= 1) continue;
                    const float x = bx + txi * 32, y = oy + ty * 32;
                    if (x > vw || x + 32 < 0) continue;
                    bm->drawTile(g, std::floor(x), std::floor(y));
                }
    }
}

const std::vector<MapObject>& TileMap::group(const std::string& n) const {
    static const std::vector<MapObject> none;
    auto it = groups.find(n);
    return it == groups.end() ? none : it->second;
}

}
