#pragma once
// Vascolet 2: escenas entre capítulos (cutscene01-06.tmx + guiones de 0x450810 con los textos cs*-msg*).
#include "map.h"
#include "res.h"
#include <string>
#include <vector>

namespace v2 {

class Cutscene {
public:
    bool load(int n);              // 1..6
    void update(float dt);
    void draw();                   // mundo (512x384)
    void drawUi(float uiW);        // diálogos y "Presiona Enter para saltear"
    bool done() const { return finished; }
    void skip() { finished = true; }
    void setInterp(float) {}

    struct CA {
        std::string id;
        Animator anim;
        float x = 0, y = 0, vx = 0, vy = 0, g = 0, ground = 0;
        bool left = false, visible = true;
        float tx = 0, speed = 0; bool moving = false;
    };
    struct Step {
        int op;
        std::string a, b, c;
        float x = 0, y = 0, z = 0;
    };

private:
    int num = 1;
    TileMap map, back, backXO;
    float camX = 384, camY = 320;
    std::vector<CA> actors;
    std::vector<Step> steps;
    size_t pc = 0;
    float t = 0, flash = 0, shake = 0;
    bool finished = false;
    std::string sayKey, speaker;
    float sayT = 0;
    CA* find(const std::string& id);
    void script(int n);
    void drawBackground();
};

}
