#include "tilemap.h"
#include "../platform.h"
#include <tinyxml2.h>
#include <cstdlib>
#include <cstring>

namespace v1 {

float MapObject::propf(const char* k, float def) const { auto it = props.find(k); return it == props.end() ? def : (float)atof(it->second.c_str()); }
int   MapObject::propi(const char* k, int def) const   { auto it = props.find(k); return it == props.end() ? def : atoi(it->second.c_str()); }
bool  MapObject::propb(const char* k, bool def) const  { auto it = props.find(k); return it == props.end() ? def : (it->second == "true" || it->second == "1"); }

static std::vector<uint8_t> b64decode(const char* s) {
    static int8_t T[256]; static bool init = false;
    if (!init) {
        memset(T, -1, sizeof T);
        const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; i++) T[(uint8_t)a[i]] = (int8_t)i;
        init = true;
    }
    std::vector<uint8_t> out;
    uint32_t acc = 0; int bits = 0;
    for (; *s; s++) {
        int v = T[(uint8_t)*s];
        if (v < 0) continue;
        acc = (acc << 6) | v; bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back((uint8_t)(acc >> bits)); }
    }
    return out;
}

static void readProps(tinyxml2::XMLElement* parent, Props& out) {
    auto* ps = parent->FirstChildElement("properties");
    if (!ps) return;
    for (auto* p = ps->FirstChildElement("property"); p; p = p->NextSiblingElement("property")) {
        const char* n = p->Attribute("name"); const char* v = p->Attribute("value");
        if (n) out[n] = v ? v : "";
    }
}

bool TileMap::load(const std::string& pakName) {
    auto data = plat::assetRead(pakName);
    if (data.empty()) return false;
    data.push_back(0);
    tinyxml2::XMLDocument doc;
    if (doc.Parse((const char*)data.data(), data.size() - 1) != tinyxml2::XML_SUCCESS) {
        plat::log("tmx parse error %s", pakName.c_str()); return false;
    }
    auto* m = doc.FirstChildElement("map");
    if (!m) return false;
    width = m->IntAttribute("width"); height = m->IntAttribute("height");
    tileW = m->IntAttribute("tilewidth", 32); tileH = m->IntAttribute("tileheight", 32);
    readProps(m, props);

    if (auto* ts = m->FirstChildElement("tileset")) {
        firstGid = ts->IntAttribute("firstgid", 1);
        if (auto* img = ts->FirstChildElement("image")) tilesetImage = img->Attribute("source");
        int maxId = 0;
        for (auto* t = ts->FirstChildElement("tile"); t; t = t->NextSiblingElement("tile")) {
            int id = t->IntAttribute("id");
            readProps(t, tileProps[id]);
            if (id > maxId) maxId = id;
        }
        tileFlagsById.assign(maxId + 1, 0);
        for (auto& kv : tileProps) {
            uint8_t f = 0;
            auto is = [&](const char* k) { auto it = kv.second.find(k); return it != kv.second.end() && it->second == "true"; };
            if (is("pInvisible")) f |= TF_INVISIBLE;
            if (is("pKilling"))   f |= TF_KILLING;
            if (is("pPlatform"))  f |= TF_PLATFORM;
            if (is("pHard"))      f |= TF_HARD;
            if (is("pLadder"))    f |= TF_LADDER;
            if (is("pLadderEnd")) f |= TF_LADDER_END;
            tileFlagsById[kv.first] = f;
        }
    }
    if (auto* l = m->FirstChildElement("layer")) {
        readProps(l, layerProps);
        auto* d = l->FirstChildElement("data");
        auto raw = b64decode(d->GetText());
        gids.assign(width * height, 0);
        for (int i = 0; i < width * height && (size_t)(i * 4 + 3) < raw.size(); i++)
            gids[i] = raw[i*4] | (raw[i*4+1] << 8) | (raw[i*4+2] << 16) | ((uint32_t)raw[i*4+3] << 24);
    }
    for (auto* og = m->FirstChildElement("objectgroup"); og; og = og->NextSiblingElement("objectgroup")) {
        for (auto* o = og->FirstChildElement("object"); o; o = o->NextSiblingElement("object")) {
            MapObject mo;
            mo.name = o->Attribute("name") ? o->Attribute("name") : "";
            mo.type = o->Attribute("type") ? o->Attribute("type") : "";
            mo.x = o->IntAttribute("x"); mo.y = o->IntAttribute("y");
            mo.w = o->IntAttribute("width"); mo.h = o->IntAttribute("height");
            readProps(o, mo.props);
            objects.push_back(std::move(mo));
        }
    }
    plat::log("map %s: %dx%d, %zu objects", pakName.c_str(), width, height, objects.size());
    return true;
}

}
