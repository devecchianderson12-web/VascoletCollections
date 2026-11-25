#include "defs.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <memory>

namespace v1 {

static std::vector<std::unique_ptr<ImageMap>> g_maps;
static std::vector<std::unique_ptr<Anim>> g_anims;
static std::map<std::string, plat::Image*> g_textures;

static std::string imageSetFor(const std::string& animSet) {
    if (animSet == "AssetsAnimationsVascolet") return "AssetsVascolet";
    if (animSet == "AssetsAnimationsEnemies") return "AssetsEnemies";
    return "";
}

const ImageMap* imageMap(const std::string& name, const char* set) {
    const ImageMap* any = nullptr;
    for (auto& m : g_maps) if (m->name == name) {
        if (!set || m->set == set) return m.get();
        if (!any) any = m.get();
    }
    return set ? any : any;
}

const Anim* anim(const std::string& name, const char* set) {
    const Anim* any = nullptr;
    for (auto& a : g_anims) if (a->name == name) {
        if (!set || a->set == set) return a.get();
        if (!any) any = a.get();
    }
    return any;
}

static void parseDefs(const std::string& text) {
    std::istringstream in(text);
    std::string line, curSet, curKind;
    ImageMap* im = nullptr;
    Anim* an = nullptr;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        std::string tag; ls >> tag;
        if (tag == "SET") { ls >> curKind >> curSet; }
        else if (tag == "IMAGEMAP") {
            std::string name; ls >> name;
            im = nullptr;
            for (auto& m : g_maps) if (m->name == name && m->set == curSet) { im = m.get(); im->frames.clear(); }
            if (!im) { g_maps.emplace_back(new ImageMap); im = g_maps.back().get(); im->set = curSet; im->name = name; }
        } else if (tag == "F" && im) {
            Frame f; int idx; float extra = 0;
            ls >> idx >> f.texture >> f.x >> f.y >> f.w >> f.h >> f.pivotX >> f.pivotY >> extra;
            im->frames.push_back(f);
        } else if (tag == "ANIM") {
            std::string name, flags; unsigned cnt; float fps = 0; int total = 0;
            ls >> name >> fps >> flags >> cnt >> total;
            an = nullptr;
            for (auto& a : g_anims) if (a->name == name && a->set == curSet) { an = a.get(); an->keys.clear(); }
            if (!an) { g_anims.emplace_back(new Anim); an = g_anims.back().get(); an->set = curSet; an->name = name; }
            an->fps = fps; an->totalTicks = total;
            an->loop = (strtoul(flags.c_str(), nullptr, 16) & 0xff) != 0;
            an->facesRight = (strtoul(flags.c_str(), nullptr, 16) >> 8) == 0xffffff;
        } else if (tag == "K" && an) {
            KeyFrame k; std::string mapName; float start;
            ls >> mapName >> k.frame >> k.ticks >> start;
            std::string imgSet = imageSetFor(an->set);
            k.map = imageMap(mapName, imgSet.empty() ? nullptr : imgSet.c_str());
            if (k.map && k.frame >= 0 && k.frame < (int)k.map->frames.size()) an->keys.push_back(k);
        }
    }
}

bool loadDefs() {
    auto layers = plat::assetReadLayers("data/v1_defs.txt");
    if (layers.empty() || layers[0].empty()) { plat::error("data/v1_defs.txt not found"); return false; }
    for (auto& l : layers) if (!l.empty()) parseDefs(std::string(l.begin(), l.end()));
    plat::log("defs: %zu image maps, %zu anims (%zu mod file(s))", g_maps.size(), g_anims.size(), layers.size() - 1);
    return true;
}

static plat::Image* texture(Frame& f) {
    if (f.img) return f.img;
    auto it = g_textures.find(f.texture);
    static std::map<std::string, uint32_t> failedAt;
    if (it != g_textures.end() && !it->second) {
        uint32_t &t = failedAt[f.texture];
        if (plat::ticksMs() - t < 500) return nullptr;
        g_textures.erase(it); it = g_textures.end();
    }
    if (it == g_textures.end()) {
        std::vector<SDL_Rect> used = { { (int)f.x, (int)f.y, (int)f.w + 1, (int)f.h + 1 } };
        plat::Image* img = plat::loadImage(f.texture, nullptr, &used);
        if (!img) { failedAt[f.texture] = plat::ticksMs(); plat::error("texture %s failed to load (will retry)", f.texture.c_str()); }
        it = g_textures.emplace(f.texture, img).first;
    }
    f.img = it->second;
    return f.img;
}

static std::map<std::string, std::pair<float, float>> g_sizes;
static bool g_sizesLoaded = false;

bool collisionSize(const std::string& asset, float& w, float& h) {
    if (!g_sizesLoaded) {
        g_sizesLoaded = true;
        for (auto& data : plat::assetReadLayers("data/v1_sizes.txt")) {
            std::istringstream in(std::string(data.begin(), data.end()));
            std::string n; float a, b;
            while (in >> n >> a >> b) g_sizes[n] = { a, b };
        }
    }
    auto it = g_sizes.find(asset);
    if (it == g_sizes.end()) return false;
    w = it->second.first; h = it->second.second;
    return true;
}

const Frame* flashFrame(const Frame* f) {
    if (!f) return f;
    for (auto& m : g_maps) {
        if (m->frames.empty() || f < m->frames.data() || f >= m->frames.data() + m->frames.size()) continue;
        size_t idx = f - m->frames.data();
        const ImageMap* r = imageMap(m->name + "_R", m->set.c_str());
        if (r && idx < r->frames.size() && r != m.get()) return &r->frames[idx];
        return f;
    }
    return f;
}

void warm(const ImageMap* m) {
    if (!m) return;
    for (auto& f : m->frames) {

        if (plat::freeMemoryKB() > 20500) return;
        plat::Image* img = texture(const_cast<Frame&>(f));
        plat::ensurePages(img, SDL_Rect{ (int)f.x, (int)f.y, (int)f.w + 1, (int)f.h + 1 });
    }
}
void warm(const Anim* a) {
    if (!a) return;
    for (auto& k : a->keys) if (k.map) warm(k.map);
}

void drawFramePart(const Frame& fc, float x, float y, float fromX, float width) {
    Frame& f = const_cast<Frame&>(fc);
    plat::Image* img = texture(f);
    if (!img) return;
    if (fromX < 0) { width += fromX; fromX = 0; }
    if (fromX + width > f.w) width = f.w - fromX;
    if (width <= 0) return;
    SDL_Rect src = { (int)(f.x + fromX), (int)f.y, (int)std::ceil(width), (int)f.h };
    SDL_FRect dst = { x - f.pivotX + fromX, y - f.pivotY, (float)src.w, f.h };
    plat::drawImage(img, src, dst);
}

void drawFrame(const Frame& fc, float x, float y, bool flipH, uint8_t alpha, bool flipV) {
    Frame& f = const_cast<Frame&>(fc);
    plat::Image* img = texture(f);
    if (!img) return;
    SDL_Rect src = { (int)f.x, (int)f.y, (int)f.w, (int)f.h };
    float px = flipH ? (f.w - f.pivotX) : f.pivotX;
    float py = flipV ? (f.h - f.pivotY) : f.pivotY;
    SDL_FRect dst = { x - px, y - py, f.w, f.h };
    plat::drawImage(img, src, dst, flipH, alpha, flipV);
}

void drawFrameScaled(const Frame& fc, float cx, float cy, float scale) {
    Frame& f = const_cast<Frame&>(fc);
    plat::Image* img = texture(f);
    if (!img) return;
    const float w = f.w * scale, h = f.h * scale;
    plat::drawImage(img, SDL_Rect{ (int)f.x, (int)f.y, (int)f.w, (int)f.h }, SDL_FRect{ cx - w * 0.5f, cy - h * 0.5f, w, h });
}

bool AnimPlayer::finished() const {
    if (!a || a->loop || a->totalTicks <= 0) return false;
    return time * a->fps >= a->totalTicks;
}

const Frame* AnimPlayer::current() const {
    if (!a || a->keys.empty()) return nullptr;
    int tick = (int)(time * a->fps);
    if (a->totalTicks > 0) {
        if (a->loop) tick %= a->totalTicks;
        else if (tick >= a->totalTicks) tick = a->totalTicks - 1;
    }
    for (auto& k : a->keys) {
        if (tick < k.ticks) return &k.map->frames[k.frame];
        tick -= k.ticks;
    }
    auto& k = a->keys.back();
    return &k.map->frames[k.frame];
}

}

namespace v1 {
int AnimPlayer::keyIndex() const {
    if (!a || a->keys.empty()) return -1;
    int tick = (int)(time * a->fps);
    if (a->totalTicks > 0) {
        if (a->loop) tick %= a->totalTicks;
        else if (tick >= a->totalTicks) return (int)a->keys.size() - 1;
    }
    for (size_t i = 0; i < a->keys.size(); i++) {
        if (tick < a->keys[i].ticks) return (int)i;
        tick -= a->keys[i].ticks;
    }
    return (int)a->keys.size() - 1;
}
}

namespace v1 {
void releaseTextures(const char* set) {
    std::vector<std::string> names;
    if (set) {
        for (auto& m : g_maps) if (m->set == set) for (auto& f : m->frames) names.push_back(f.texture);
    } else {
        for (auto& kv : g_textures) names.push_back(kv.first);
    }
    size_t freed = 0;
    for (auto& n : names) {
        auto it = g_textures.find(n);
        if (it == g_textures.end()) continue;
        for (auto& m : g_maps) for (auto& f : m->frames) if (f.img == it->second) f.img = nullptr;
        plat::freeImage(it->second);
        g_textures.erase(it);
        freed++;
    }
    if (!set) plat::cancelPrefetches();
    plat::log("released %zu textures (%s); heap in use %u KB", freed, set ? set : "all", plat::freeMemoryKB());
}
}

namespace v1 {

void releaseTextures(const ImageMap* map) {
    if (!map) return;
    for (auto& fr : map->frames) {
        auto it = g_textures.find(fr.texture);
        if (it == g_textures.end()) continue;
        for (auto& m : g_maps) for (auto& f : m->frames) if (f.img == it->second) f.img = nullptr;
        plat::freeImage(it->second);
        g_textures.erase(it);
    }
}
}

namespace v1 {

void prefetch(const ImageMap* m) {
    if (!m) return;
    for (auto& f : m->frames) {
        if (f.img || g_textures.count(f.texture)) continue;
        std::vector<SDL_Rect> used;
        for (auto& fr : m->frames)
            if (fr.texture == f.texture) used.push_back({ (int)fr.x, (int)fr.y, (int)fr.w + 1, (int)fr.h + 1 });
        plat::prefetchImage(f.texture, &used);
    }
}
}

namespace v1 {

void drawFrameRect(const Frame& fc, float sx, float sy, float w, float h, float x, float y) {
    Frame& f = const_cast<Frame&>(fc);
    plat::Image* img = texture(f);
    if (!img || w <= 0 || h <= 0) return;
    SDL_Rect src = { (int)(f.x + sx), (int)(f.y + sy), (int)w, (int)h };
    plat::drawImage(img, src, SDL_FRect{ x, y, w, h });
}
}

namespace v1 {

void setFrameTexture(const char* map, const char* set, int idx, const std::string& tex) {
    for (auto& m : g_maps) if (m->name == map && (!set || m->set == set) && idx < (int)m->frames.size()) {
        Frame& f = m->frames[idx];
        if (f.texture != tex) { f.texture = tex; f.img = nullptr; }
    }
}
}
