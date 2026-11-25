#pragma once
// Vascolet 2: un nivel en juego (mapa, personajes, física, cámara y HUD), reconstruido del exe
// (reglas de GameScripts 0x42d170, descriptores de personajes 0x465a10). Ver NOTAS-V2.md.
#include "map.h"
#include "res.h"
#include <memory>
#include <string>
#include <vector>

namespace v2 {

constexpr int VIEW_W = 512, VIEW_H = 384;
constexpr float AUTO_CAM_SPEED = 60.0f;   // px/s de la cámara automática del nivel 3

// Descriptor de cada tipo de personaje (0x465a10): carpeta, animación inicial, gravedad, vida y caja
// (medio ancho contra paredes, medio ancho contra el piso, alto).
struct CharDef {
    const char* name;
    const char* folder;
    const char* anim;
    float gravity;
    int life;
    float wallW, floorW, height;
};
const CharDef* charDef(const std::string& name);

// Lo que se lleva de un nivel al siguiente.
struct Progress {
    int lives = 5;
    int energy = 5;
    int coins = 0;
};

struct Actor {
    const CharDef* def = nullptr;
    std::string type;          // "Scorpion", "Coin"...
    std::string cmd;           // comandos iniciales del mapa (type): forward, dontFall, up...
    Props props;
    float x = 0, y = 0;        // centro de los pies
    float x0 = 0, y0 = 0;      // posición inicial
    float vx = 0, vy = 0;
    float ax = 0;              // fuerza horizontal de este paso
    float gravity = 0, maxVx = 9999, friction = 0, maxFall = 900;
    bool left = false;         // mirando a la izquierda
    bool onFloor = false, hitWall = false, hitFloor = false, hitCeil = false, solidCollide = true;
    int state = 0;
    float stateT = 0;
    int life = 1;
    bool dead = false, removed = false;
    bool stompable = true, harmful = true, invincible = false;
    float flashT = 0;          // parpadeo
    float t = 0;               // reloj libre
    float px = 0, py = 0;      // posición anterior (interpolación)
    int phase = 0;             // dato libre por tipo
    float a = 0, b = 0, c = 0; // datos libres por tipo
    Animator anim;
    float w() const { return def ? def->wallW : 8; }
    float fw() const { return def ? def->floorW : 8; }
    float h() const { return def ? def->height : 16; }
    bool has(const char* c) const;
    SDL_FRect box() const { return { x - w(), y - h(), w() * 2, h() }; }
};

class World {
public:
    bool load(int level, const Progress& p);     // level 1..5
    void update(float dt);
    void draw();
    void drawHud(float uiW);
    void setInterp(float a) { interp = a; }
    bool won() const { return winT > 3.0f; }
    bool gameOver() const { return overT > 3.0f; }
    Progress progress() const { return prog; }
    int level() const { return lvl; }
    int coins() const { return prog.coins; }
    int totalCoins() const { return coinTotal; }

private:
    TileMap map, back, backXO;
    int lvl = 1;
    Progress prog;
    int coinTotal = 0;
    std::vector<std::unique_ptr<Actor>> actors;
    Actor* player = nullptr;
    float camX = 0, camY = 0, pcamX = 0, pcamY = 0;
    float interp = 1;
    // Estado del jugador
    bool runEnabled = false, doubleJumpEnabled = false, attackEnabled = false, canDoubleJump = false;
    int keys[4] = { 0, 0, 0, 0 };  // roja, verde, azul, calavera
    float invT = 0;                // invencible tras un golpe
    float saveX = 0, saveY = 0;    // último punto de guardado
    bool hasLosing = false;
    float winT = -1, overT = -1;
    float shakeT = 0;              // temblor de pantalla
    float sx = 0, sy = 0;          // desplazamiento del temblor en este cuadro
    bool autoCam = false;          // nivel 3: la cámara avanza sola por camPath
    std::vector<SDL_FPoint> camPath;
    float camD = 0, saveCamD = 0;
    int enemyFire() const;
    float bossBar = -1;            // vida del jefe (0..1) o -1 sin barra
    std::string bossIcon;
    std::string msg; float msgT = 0; bool msgLinger = false;
    const Actor* msgSign = nullptr;
    std::string music;
    struct Zone { float x, y, w, h; float alpha = 1; };
    std::vector<Zone> secrets;
    std::vector<MapObject> camLines;
    std::vector<SDL_FRect> waterRects;
    float doubleTapT = 0; int lastDir = 0;

    Actor* spawn(const std::string& type, float x, float y, const std::string& cmd = "", const Props* props = nullptr);
    void updatePlayer(float dt);
    void playerState(int s);
    void updateActor(Actor& a, float dt);
    void physics(Actor& a, float dt);
    void interactions();
    void hurtPlayer(bool fromTile);
    void killEnemy(Actor& e, bool stomp);
    bool shotHits(Actor& shot, bool fire);
    void updateCamera(bool snap);
    void showMsg(const std::string& key, bool sign);
    bool solidAt(float x, float y, bool forFloor = false, float prevBottom = -1e9f) const;
    uint8_t tileFlags(float x, float y) const;
    bool inWater(const Actor& a) const;
    bool cliffAhead(const Actor& a) const;
    void drawBackground();
    void drawLayer(const Layer& l, bool fg);
    int countOf(const std::string& type) const;
    Actor* find(const std::string& type);
};

}
