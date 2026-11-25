#include "res.h"
#include "../launcher/font.h"
#include "../launcher/settings.h"
#include <tinyxml2.h>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace v2 {

namespace {

const std::string ROOT = "v2/assets/";

bool parseXml(tinyxml2::XMLDocument& doc, const std::string& path) {
    auto d = plat::assetRead(path);
    if (d.empty()) { plat::log("v2: falta %s", path.c_str()); return false; }
    size_t off = d.size() >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF ? 3 : 0;
    return doc.Parse((const char*)d.data() + off, d.size() - off) == tinyxml2::XML_SUCCESS;
}

plat::Image* image(const std::string& path) {
    static std::map<std::string, plat::Image*> cache;
    auto it = cache.find(path);
    if (it != cache.end()) return it->second;
    plat::Image* i = plat::loadImage(path);
    cache[path] = i;
    return i;
}

std::string dirOf(const std::string& p) { const size_t s = p.find_last_of('/'); return s == std::string::npos ? "" : p.substr(0, s + 1); }

// Carpeta de cada personaje → (animaciones, biblioteca de sprites).
struct CharFiles { const char* folder; const char* anim; const char* lib; };
const CharFiles CHAR_FILES[] = {
    { "amigos", "Amigos", "images/amigos" }, { "anguila", "Anguila", "images/anguila" }, { "axe", "Axe", "images/axe" },
    { "bat", "Bat", "images/bat" }, { "blowfish", "Blowfish", "images/blowfish" }, { "body", "Body", "images/body" },
    { "bola-de-pinchos", "Spykes", "images/bola-de-pinchos" }, { "boomerang", "Boomerang", "images/boomerang" },
    { "bruja", "Witch", "images/bruja" }, { "carteles-ayuda-guardar", "Signs", "images/carteles-ayuda-guardar" },
    { "dragon", "BigDragon", "images/dragon" }, { "energia", "Energy", "images/energia" },
    { "escorpion", "Scorpion", "images/escorpion" }, { "explosion", "Explosion", "images/explosion" },
    { "feetball", "SpykeFeet", "images/feetball" }, { "fenix", "Fenix", "images/fenix" },
    { "fireball", "Fireball", "images/fireball" }, { "fireball2", "Fireball2", "images/fireball2" },
    { "fish", "Fish", "images/fish" }, { "friends", "Friends", "images/friends" }, { "ghost", "Ghost", "images/ghost" },
    { "golem", "Golem", "images/golem" }, { "head", "Head", "images/head" }, { "iguana", "Iguana", "images/Iguana" },
    { "itemCereals", "ItemCereals", "images/bowl" }, { "libro", "Libro", "images/libro" },
    { "llave-azul", "BlueKey", "images/llave-azul" }, { "llave-roja", "RedKey", "images/llave-roja" },
    { "llave-verde", "GreenKey", "images/llave-verde" }, { "malmark", "Malmark", "images/malmark" },
    { "micro", "Micro", "images/micro" }, { "mina", "Mina", "minaImages/Mina" }, { "moneda", "Coin", "images/moneda" },
    { "ogro", "Ogre", "images/ogro" }, { "pajaro", "Bird", "images/pajaro" }, { "particles", "Particles", "images/particles" },
    { "patineta", "Patineta", "images/patineta" }, { "pirana", "Piranha", "images/pirana" },
    { "plataforma", "Platform", "images/plataforma" }, { "portal", "Portal", "images/portal" },
    { "pterodactilo", "Pterodactil", "images/pterodactilo" },
    { "pterodactilo-verde", "pterodactilo/animData/Pterodactil", "images/pterodactilo-verde" },
    { "puercoespin", "Hedgehog", "images/puercoespin" }, { "puerta-azul", "BlueDoor", "images/puerta-azul" },
    { "puerta-calavera", "SkullDoor", "images/puerta-calavera" }, { "puerta-roja", "RedDoor", "images/puerta-roja" },
    { "puerta-verde", "GreenDoor", "images/puerta-verde" }, { "raton", "Mouse", "images/raton" },
    { "rocas", "Rocks", "images/rocas" }, { "skullkey", "SkullKey", "images/skullkey" },
    { "sneaker", "Sneaker", "images/sneaker" }, { "snorkel", "Snorkel", "images/snorkel" },
    { "spring", "Spring", "images/spring" }, { "uglyDragon", "UglyDragon", "images/uglyDragon" },
    { "vascolet", "Vascolet", "images/av" }, { "vaso", "ItemVascolet", "images/vaso" },
    { "vida-extra", "ExtraLive", "images/vida-extra" }, { "willy", "Willy", "images/willy" },
};

}

const Sprite* SpriteLib::get(const std::string& key) const {
    auto it = sprites.find(key);
    return it == sprites.end() ? nullptr : &it->second;
}

const SpriteLib* spriteLib(const std::string& xmlPath) {
    static std::map<std::string, std::unique_ptr<SpriteLib>> cache;
    auto it = cache.find(xmlPath);
    if (it != cache.end()) return it->second.get();
    auto lib = std::make_unique<SpriteLib>();
    tinyxml2::XMLDocument doc;
    if (parseXml(doc, xmlPath)) {
        const std::string dir = dirOf(xmlPath);
        auto* data = doc.RootElement() ? doc.RootElement()->FirstChildElement("data") : nullptr;
        for (auto* e = data ? data->FirstChildElement("SpriteData") : nullptr; e; e = e->NextSiblingElement("SpriteData")) {
            Sprite s;
            const char* src = e->Attribute("sourceImage");
            s.img = image(dir + (src ? src : "") + ".png");
            int a[2] = { 0, 0 }, r[4] = { 0, 0, 0, 0 };
            if (const char* ax = e->Attribute("axis")) sscanf(ax, "%d,%d", &a[0], &a[1]);
            if (const char* rc = e->Attribute("sourceRect")) sscanf(rc, "%d,%d,%d,%d", &r[0], &r[1], &r[2], &r[3]);
            s.ax = a[0]; s.ay = a[1]; s.r = { r[0], r[1], r[2], r[3] };
            if (const char* k = e->Attribute("keyName")) lib->sprites[k] = s;
        }
    }
    SpriteLib* p = lib.get();
    cache[xmlPath] = std::move(lib);
    return p;
}

bool Frame::flag(const char* f) const {
    for (auto& x : flags) if (x == f) return true;
    return false;
}

const Anim* AnimSet::get(const std::string& n) const {
    auto it = anims.find(n);
    return it == anims.end() ? nullptr : &it->second;
}

const AnimSet* animSet(const std::string& folder) {
    static std::map<std::string, std::unique_ptr<AnimSet>> cache;
    auto it = cache.find(folder);
    if (it != cache.end()) return it->second.get();
    auto set = std::make_unique<AnimSet>();
    const CharFiles* cf = nullptr;
    for (auto& c : CHAR_FILES) if (folder == c.folder) cf = &c;
    if (cf) {
        const std::string base = ROOT + "characters/" + folder + "/";
        const SpriteLib* lib = spriteLib(base + cf->lib + ".xml");
        tinyxml2::XMLDocument doc;
        // Un nombre con "/" es una ruta desde characters/ (el pterodáctilo verde usa las animaciones del rojo).
        const std::string animPath = strchr(cf->anim, '/') ? ROOT + "characters/" + cf->anim + ".xml" : base + "animData/" + cf->anim + ".xml";
        if (parseXml(doc, animPath)) {
            auto* anims = doc.RootElement() ? doc.RootElement()->FirstChildElement("Animations") : nullptr;
            for (auto* a = anims ? anims->FirstChildElement("Animation") : nullptr; a; a = a->NextSiblingElement("Animation")) {
                Anim an;
                an.name = a->Attribute("name") ? a->Attribute("name") : "";
                const float fps = std::max(1.0f, a->FloatAttribute("fps", 12));
                an.loop = a->IntAttribute("loopIndex", -1);
                for (auto* f = a->FirstChildElement("Frame"); f; f = f->NextSiblingElement("Frame")) {
                    Frame fr;
                    fr.dur = std::max(1, f->IntAttribute("duration", 1)) / fps;
                    for (auto* s = f->FirstChildElement("Sprite"); s; s = s->NextSiblingElement("Sprite")) {
                        FrameSprite fs;
                        fs.sp = lib->get(s->Attribute("image") ? s->Attribute("image") : "");
                        fs.ox = s->FloatAttribute("xOffset"); fs.oy = s->FloatAttribute("yOffset");
                        if (fs.sp) fr.sprites.push_back(fs);
                    }
                    for (auto* g = f->FirstChildElement("Flag"); g; g = g->NextSiblingElement("Flag"))
                        if (const char* t = g->Attribute("type")) fr.flags.push_back(t);
                    an.frames.push_back(fr);
                }
                if (an.loop >= (int)an.frames.size()) an.loop = -1;
                set->anims[an.name] = std::move(an);
            }
        }
    } else plat::log("v2: personaje desconocido %s", folder.c_str());
    AnimSet* p = set.get();
    cache[folder] = std::move(set);
    return p;
}

void Animator::play(const std::string& n, bool restart) {
    if (!restart && n == name && anim) return;
    name = n;
    anim = set ? set->get(n) : nullptr;
    frame = 0; t = 0; ended = false; newFrame = true;
    if (anim && anim->frames.empty()) anim = nullptr;
}

void Animator::update(float dt) {
    newFrame = false;
    if (!anim || ended) return;
    t += dt;
    int guard = 0;
    while (t >= anim->frames[frame].dur && guard++ < 64) {
        t -= anim->frames[frame].dur;
        if (frame + 1 < (int)anim->frames.size()) { frame++; newFrame = true; }
        else if (anim->loop >= 0) { frame = anim->loop; newFrame = true; }
        else { ended = true; t = 0; break; }
    }
}

void drawSprite(const Sprite* s, float x, float y, bool flip, uint8_t alpha, float scale) {
    if (!s || !s->img) return;
    const float ax = flip ? s->r.w - s->ax : s->ax;
    plat::drawImage(s->img, s->r, SDL_FRect{ x - ax * scale, y - s->ay * scale, s->r.w * scale, s->r.h * scale }, flip, alpha);
}

void drawFrame(const Frame* f, float x, float y, bool flip, uint8_t alpha, float scale) {
    if (!f) return;
    for (auto& s : f->sprites) drawSprite(s.sp, x + (flip ? -s.ox : s.ox) * scale, y + s.oy * scale, flip, alpha, scale);
}

// ---------------- Textos ----------------
std::string text(const std::string& key) {
    static std::map<std::string, std::string> texts;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        tinyxml2::XMLDocument doc;
        if (parseXml(doc, ROOT + "data/texts.xml")) {
            auto* t = doc.RootElement() ? doc.RootElement()->FirstChildElement("texts") : nullptr;
            for (auto* n = t ? t->FirstChildElement("node") : nullptr; n; n = n->NextSiblingElement("node")) {
                const char* k = n->Attribute("key"); const char* v = n->Attribute("text");
                if (!k || !v) continue;
                std::string s;
                for (const char* p = v; *p; p++) {
                    if (p[0] == '\\' && p[1] == 'n') { s += '\n'; p++; }
                    else s += *p;
                }
                texts[k] = s;
            }
        }
    }
    auto it = texts.find(key);
    return it == texts.end() ? key : it->second;
}

// ---------------- Sonido ----------------
void sfx(const std::string& name) {
    static std::map<std::string, Mix_Chunk*> cache;
    auto it = cache.find(name);
    Mix_Chunk* c = nullptr;
    if (it == cache.end()) { c = plat::loadSound(ROOT + "audio/fx/win/" + name + ".ogg"); cache[name] = c; }
    else c = it->second;
    if (c) plat::playSound(c);
}

void music(const std::string& file, bool loop) {
    if (file.empty()) { plat::stopMusic(); return; }
    plat::playMusic(ROOT + "audio/music/win/" + file, loop);
}

// ---------------- Fuentes ----------------
static vc::Font* font(int i) {
    static vc::Font f[3];
    static bool ok[3] = { false, false, false };
    static const char* const NAME[3] = { "vasco2Font", "hudfont", "ProFont" };
    if (i < 0 || i > 2) i = 0;
    if (!ok[i]) { f[i].load(ROOT + "lang/fonts/" + NAME[i]); ok[i] = true; }
    return &f[i];
}

void drawText(const std::string& s, float x, float y, float scale, SDL_Color c, int f) {
    font(f)->draw(s, x, y, scale, c);
}
float textWidth(const std::string& s, float scale, int f) { return font(f)->width(s, scale); }
float textHeight(float scale, int f) { return font(f)->height(scale); }

}
