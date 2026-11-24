#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace v1 {

using Props = std::map<std::string, std::string>;

struct MapObject {
    std::string name, type;
    int x, y, w, h;
    Props props;
    std::string prop(const char* k, const char* def = "") const {
        auto it = props.find(k); return it == props.end() ? def : it->second;
    }
    float propf(const char* k, float def = 0) const;
    int   propi(const char* k, int def = 0) const;
    bool  propb(const char* k, bool def = false) const;
};

enum TileFlags : uint8_t {
    TF_INVISIBLE = 1, TF_KILLING = 2, TF_PLATFORM = 4, TF_HARD = 8,
    TF_LADDER = 16, TF_LADDER_END = 32,
};

struct TileMap {
    int width = 0, height = 0, tileW = 32, tileH = 32;
    Props props;
    std::string tilesetImage;
    int firstGid = 1;
    std::map<int, Props> tileProps;
    std::vector<uint8_t> tileFlagsById;
    std::vector<uint32_t> gids;
    std::vector<MapObject> objects;

    bool load(const std::string& pakName);
    uint32_t gidAt(int tx, int ty) const {
        if (tx < 0 || ty < 0 || tx >= width || ty >= height) return 0;
        return gids[ty * width + tx];
    }
    uint8_t flagsAt(int tx, int ty) const {
        uint32_t g = gidAt(tx, ty);
        if (!g) return 0;
        int id = (int)g - firstGid;
        return (id >= 0 && id < (int)tileFlagsById.size()) ? tileFlagsById[id] : 0;
    }
    std::string prop(const char* k, const char* def = "") const {
        auto it = props.find(k); return it == props.end() ? def : it->second;
    }
};

}
