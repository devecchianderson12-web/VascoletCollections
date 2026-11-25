#pragma once
// Vascolet 2: recursos del motor "bat" del original (bibliotecas de sprites, animaciones, textos, sonidos).
#include "../platform.h"
#include <map>
#include <string>
#include <vector>

namespace v2 {

// Un sprite de una biblioteca (images/*.xml): rectángulo dentro de un png y su punto de anclaje (axis).
struct Sprite {
    plat::Image* img = nullptr;
    SDL_Rect r{};
    int ax = 0, ay = 0;
};
struct SpriteLib {
    std::map<std::string, Sprite> sprites;
    const Sprite* get(const std::string& key) const;
};
// Carga (una vez) una biblioteca: "assets/characters/vascolet/images/av.xml".
const SpriteLib* spriteLib(const std::string& xmlPath);

struct FrameSprite { const Sprite* sp = nullptr; float ox = 0, oy = 0; };
struct Frame {
    float dur = 0.1f;                  // segundos
    std::vector<FrameSprite> sprites;
    std::vector<std::string> flags;    // Custom, PlaySound, MakeEffect, PhysicEvent
    bool flag(const char* f) const;
};
struct Anim {
    std::string name;
    int loop = -1;                     // cuadro al que vuelve al terminar; -1 = se queda en el último
    std::vector<Frame> frames;
};
struct AnimSet {
    std::map<std::string, Anim> anims;
    const Anim* get(const std::string& n) const;
};
// Personaje: carpeta en assets/characters, archivo de animaciones y de sprites.
const AnimSet* animSet(const std::string& folder);

// Reproduce una animación cuadro por cuadro (duraciones del xml).
struct Animator {
    const AnimSet* set = nullptr;
    const Anim* anim = nullptr;
    std::string name;
    int frame = 0;
    float t = 0;
    bool ended = false;
    bool newFrame = true;     // el cuadro actual empezó en este paso
    void play(const std::string& n, bool restart = false);
    void update(float dt);
    const Frame* cur() const { return anim && frame < (int)anim->frames.size() ? &anim->frames[frame] : nullptr; }
    bool flag(const char* f) const { const Frame* c = cur(); return c && c->flag(f); }
    bool flagFirst(const char* f) const { return newFrame && flag(f); }
};

// Dibuja un sprite con su anclaje en (x, y) de pantalla. flip = mirando a la izquierda.
void drawSprite(const Sprite* s, float x, float y, bool flip = false, uint8_t alpha = 255, float scale = 1.0f);
void drawFrame(const Frame* f, float x, float y, bool flip = false, uint8_t alpha = 255, float scale = 1.0f);

// Textos del juego (assets/data/texts.xml).
std::string text(const std::string& key);

// Sonidos (assets/audio/fx/win/<nombre>.ogg) y música (assets/audio/music/win/<archivo>).
void sfx(const std::string& name);
void music(const std::string& file, bool loop = true);

// Fuentes del original.
struct BmpFont;
void drawText(const std::string& s, float x, float y, float scale = 1.0f, SDL_Color c = { 255, 255, 255, 255 }, int font = 0);
float textWidth(const std::string& s, float scale = 1.0f, int font = 0);
float textHeight(float scale = 1.0f, int font = 0);
enum { FONT_MAIN = 0, FONT_HUD = 1, FONT_SMALL = 2 };

}
