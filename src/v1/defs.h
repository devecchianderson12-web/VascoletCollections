#pragma once
#include <string>
#include <vector>
#include <map>
#include "../platform.h"

namespace v1 {

struct Frame {
    std::string texture;
    float x, y, w, h;
    float pivotX, pivotY;
    plat::Image* img = nullptr;
};

struct ImageMap {
    std::string set, name;
    std::vector<Frame> frames;
};

struct KeyFrame {
    const ImageMap* map;
    int frame;
    int ticks;
};

struct Anim {
    std::string set, name;
    int fps = 30;
    bool loop = false;
    bool facesRight = false;   // el dibujo original mira a la derecha (banderas 0xffffffxx del juego)
    int totalTicks = 0;
    std::vector<KeyFrame> keys;
};

bool loadDefs();

const ImageMap* imageMap(const std::string& name, const char* set = nullptr);
const Anim* anim(const std::string& name, const char* set = nullptr);

bool collisionSize(const std::string& asset, float& w, float& h);

const Frame* flashFrame(const Frame* f);

void warm(const ImageMap* m);
void warm(const Anim* a);

void releaseTextures(const char* set = nullptr);
void releaseTextures(const ImageMap* map);

void prefetch(const ImageMap* m);

void drawFramePart(const Frame& f, float x, float y, float fromX, float width);

void drawFrameRect(const Frame& f, float sx, float sy, float w, float h, float x, float y);
void setFrameTexture(const char* map, const char* set, int idx, const std::string& tex);

void drawFrame(const Frame& f, float x, float y, bool flipH = false, uint8_t alpha = 255, bool flipV = false);
// Dibuja el cuadro entero escalado, centrado en (cx, cy) (HUD).
void drawFrameScaled(const Frame& f, float cx, float cy, float scale);

struct AnimPlayer {
    const Anim* a = nullptr;
    float time = 0;
    void play(const Anim* an, bool restart = false) {
        if (an != a || restart) { a = an; time = 0; }
    }
    void update(float dt) { time += dt; }
    bool finished() const;
    const Frame* current() const;
    int keyIndex() const;
    void draw(float x, float y, bool flipH = false, uint8_t alpha = 255) const {
        if (const Frame* f = current()) drawFrame(*f, x, y, flipH, alpha);
    }
};

}
