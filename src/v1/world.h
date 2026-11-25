#pragma once
#include <set>
#include "tilemap.h"
#include "defs.h"
#include "camera.h"
#include "../platform.h"
#include <string>
#include <vector>

namespace v1 {

struct Box { float x, y, w, h; };
inline bool overlap(const Box& a, const Box& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

// Épocas (niveles) en el orden del juego original.
constexpr int LEVELS = 5;
extern const char* const LEVEL_MAP[LEVELS];     // archivo .tmx
extern const char* const LEVEL_MUSIC[LEVELS];
extern const char* const LEVEL_SUFFIX[LEVELS];  // sufijo de puertas y plataformas
extern const char* const LEVEL_NAME[LEVELS];

enum HeroState { HS_GROUND, HS_AIR, HS_CLIMB, HS_HURT, HS_DEAD, HS_WIN };

struct Hero {
    Box box{};
    float vx = 0, vy = 0;
    bool left = false, onGround = false, doubleJumped = false;
    bool stillJump = false;   // saltó sin dirección: en el aire acelera la mitad (original)
    HeroState state = HS_AIR;
    float invuln = 0, hurtT = 0, deadT = 0;
    int platform = -1;          // plataforma móvil sobre la que está parado
    AnimPlayer anim;
    float cx() const { return box.x + box.w * 0.5f; }
    float bottom() const { return box.y + box.h; }
};

enum EnemyKind { EK_WALKER, EK_JUMPER, EK_BIRD, EK_STATIC, EK_THROWER, EK_PIRANHA, EK_GUILLOTINE, EK_STALACTITE };

struct Enemy {
    EnemyKind kind = EK_WALKER;
    Box box{};
    float x0 = 0, y0 = 0, h0 = 0, vx = 0, vy = 0, ay = 0, jumpVy = 0;
    float savedVx = 0, landT = 0; bool landed = false;   // serpiente saltarina
    float dx = 0, dy = 0, duration = 1, t = 0, wait = 0, minT = 1, maxT = 2, timer = 0;
    float offX = 0, offY = 0, bulletVx = 0;
    bool collidesFloor = false, collidesH = false, collidesV = false, left = true, dead = false, stompable = true;
    bool falling = false, up = true;
    std::string asset, animName, bullet, splash;
    AnimPlayer anim, splashAnim;
    const ImageMap* still = nullptr;
};

struct Item { Box box; std::string asset, counter; int count = 1; bool taken = false; float bob = 0; const ImageMap* img = nullptr; };
struct Door { Box box; std::string key; const ImageMap* img = nullptr; bool opening = false, gone = false; float off = 0; };
struct Platform { Box box; float sx, sy, dx, dy, duration = 2, wait = 0.5f, t = 0, waitT = 0, lastX = 0, lastY = 0; int dir = 1; const ImageMap* img = nullptr; };
struct Sign { Box box; std::string text; };
struct SavePoint { Box box; bool active = false; };
struct Meteor { float x = 0, y = 0, vx = 0, vy = 0, ay = 0; int bounces = 0; bool alive = true; };
struct Shot { float x, y, vx, vy, cy = 0; bool alive = true; const ImageMap* img = nullptr; };   // cy: del pivote al centro
struct Effect { float x, y; AnimPlayer anim; bool additive = false; };

struct Counters {
    int lives = 10, points = 0;
    int vascolets = 0, cereals = 0;   // del nivel actual (4 vasos y 4 bowls en el HUD)
    int coins = 0;                    // monedas juntadas en el nivel (de 100)
    int keys[4] = {};           // amarilla, roja, verde, azul
};

class World {
public:
    bool load(int level, const Counters& carry);
    void update(float dt);
    void draw();
    bool won() const { return winT > 2.4f; }
    bool gameOver() const { return outOfLives; }
    const Counters& counters() const { return cnt; }
    int level() const { return levelNo; }
    void setInterp(float a) { interp = a < 0 ? 0 : (a > 1 ? 1 : a); }
    void refitCamera();
    void drawHud();   // HUD y carteles (se dibuja aparte, en el espacio de la interfaz)

private:
    // ---- lógica ----
    void updateHero(float dt);
    void updateEnemies(float dt);
    void updatePlatforms(float dt);
    void moveBox(Box& b, float& vx, float& vy, float dt, bool solidPlatforms, bool* ground, bool* wall);
    bool solidAt(float x, float y) const;
    bool platformTop(float x, float y, float prevBottom) const;
    bool ladderAt(float x, float y) const;
    bool killingAt(const Box& b) const;
    void hurtHero(float fromX);
    void killHero();
    void respawn();
    void award(const Item& it);
    void spawnEffect(const char* anim, float x, float y);

    void drawBackground();
    void drawFrameContents();

    int levelNo = 0;
    TileMap map;
    plat::Image* tiles = nullptr;
    uint32_t filler = 0;        // bloque de relleno de fuera del nivel (no se dibuja)
    float boundX0 = 0, boundY0 = 0, boundX1 = 0, boundY1 = 0;   // zona jugable (límites de la cámara)
    Hero hero;
    float spawnX = 0, spawnY = 0;
    bool riding = false;        // montado (dinosaurio en la Prehistoria, patineta en el Futuro)
    const char* mount = "";     // prefijo de las animaciones: "D_" dinosaurio, "P_" patineta
    const Anim* heroAnim(const char* name) const;
    float liquidBottom(float x, float y) const;
    bool fillerCell(int tx, int ty) const;
    void roomBounds();
    void centerSmallRoom();
    void screenCamera(bool snap);
    void resetScreenEnemies();
    std::vector<Enemy> enemiesInit;   // estado inicial (se restaura al entrar a su pantalla)
    int screenX = -1, screenY = -1;   // pantalla (16x12 bloques) que muestra la cámara
    std::vector<Enemy> enemies;
    std::vector<Item> items;
    std::vector<Door> doors;
    std::vector<Platform> platforms;
    std::vector<Sign> signs;
    std::vector<SavePoint> saves;
    std::vector<Shot> shots;
    std::vector<Meteor> meteors;
    std::set<int> meteorScreens;       // pantallas con lluvia de meteoritos (x * 1000 + y)
    float meteorT = 1.0f;
    const ImageMap* meteorImg = nullptr;
    void updateMeteorites(float dt);
    std::vector<Effect> effects;
    const Sign* activeSign = nullptr;
    const Sign* lastSign = nullptr;
    std::string message;        // franja negra con texto debajo del HUD (carteles, "Juego Salvado")
    float messageT = 0;
    float savedT = 0;           // franja de "Juego Salvado" (debajo de la de los carteles)
    void showMessage(const std::string& m, float secs) { message = m; messageT = secs; }
    Counters cnt;
    bool outOfLives = false;
    float winT = 0;
    float stepT = 0; bool stepAlt = false;   // sonido de pasos y escaleras
    float powerT = 0;            // cereales: invencible un rato
    AnimPlayer powerAnim;
    GameCamera cam;
    float camX = 0, camY = 0;
    float interp = 1, prevCamX = 0, prevCamY = 0, prevHeroX = 0, prevHeroY = 0;

public:
    // Iluminación HD (lighting.cpp).
    void hdBackdrop(int cx, int cy);
    void hdTileShadows(int cx, int cy);
    void hdTileRims(int cx, int cy);
    void hdContactShadow(float wx, float feetY, bool grounded, float width, int cx, int cy);
    void hdLighting(int cx, int cy);
    void drawLightMap(int cx, int cy);
    std::vector<uint8_t> hdDepth;
};

void playSfx(const char* file);

}
