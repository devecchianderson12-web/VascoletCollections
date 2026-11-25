#pragma once
// Vascolet 2: mapas .tmx (Tiled) del original: capas de tiles de 32 px, tilesets con propiedades
// (solid, up, damage, water, invisible) y grupos de objetos (Enemies, Items, General, Platforms, Camera...).
#include "../platform.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace v2 {

using Props = std::map<std::string, std::string>;

struct MapObject {
    std::string name, type;
    float x = 0, y = 0, w = 0, h = 0;
    Props props;
    bool hasProp(const char* k) const { return props.count(k) != 0; }
    std::string prop(const char* k, const std::string& def = "") const { auto it = props.find(k); return it == props.end() ? def : it->second; }
};

enum TileFlag : uint8_t { T_SOLID = 1, T_UP = 2, T_DAMAGE = 4, T_WATER = 8, T_INVISIBLE = 16 };

struct Tileset {
    plat::Image* img = nullptr;
    int firstGid = 1, cols = 16, count = 0;
    std::vector<uint8_t> flags;   // por id local
};

struct Layer {
    std::string name;
    int w = 0, h = 0;
    bool visible = true;
    float hScale = 1, vScale = 1;      // paralaje (fondos)
    std::vector<int> gids;
    int at(int x, int y) const { return x < 0 || y < 0 || x >= w || y >= h ? 0 : gids[(size_t)y * w + x]; }
};

struct TileMap {
    int w = 0, h = 0, tw = 32, th = 32;
    Props props;
    std::vector<Tileset> tilesets;
    std::vector<Layer> layers;
    std::map<std::string, std::vector<MapObject>> groups;
    bool load(const std::string& path);   // "v2/assets/data/maps/level1.tmx"
    const Layer* layer(const std::string& n) const;
    uint8_t flagsOfGid(int gid) const;
    void drawTile(int gid, float x, float y, float size = 32.0f, uint8_t alpha = 255) const;
    const std::vector<MapObject>& group(const std::string& n) const;
    std::string prop(const std::string& k, const std::string& def = "") const { auto it = props.find(k); return it == props.end() ? def : it->second; }
    // Dibuja la parte visible de una capa (cámara en cx, cy); alpha(tx, ty) opcional (paredes secretas).
    void drawView(const Layer& l, float cx, float cy, int vw, int vh, const std::function<uint8_t(int, int)>& alpha = nullptr) const;
};

// Fondos con paralaje (background + backgroundXO del mapa), de la capa más lejana a la más cercana.
void drawParallax(const TileMap& back, const TileMap& xo, float cx, float cy, int vw, int vh);

}
