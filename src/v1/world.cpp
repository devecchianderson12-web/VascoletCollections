// Vascolet 1 (La Máquina del Tiempo): mundo, jugador, enemigos y objetos.
// Los mapas (.tmx), sprites y animaciones son los del juego original; la lógica se reconstruyó
// a partir de las propiedades de los mapas.
#include "world.h"
#include "game.h"
#include "lighting.h"
#include "../launcher/font.h"
#include "../launcher/settings.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace v1 {

const char* const LEVEL_MAP[LEVELS] = { "prehistory", "egypt", "castle", "pirates", "future" };
const char* const LEVEL_MUSIC[LEVELS] = { "prehistoria", "andy", "castillo", "piratas", "futuro" };
const char* const LEVEL_SUFFIX[LEVELS] = { "PREHISTORY", "EGYPT", "CASTLE", "PIRATES", "FUTURE" };
const char* const LEVEL_NAME[LEVELS] = { "Prehistoria", "Egipto", "Castillo", "Piratas", "Futuro" };

// ---- Física del jugador (en píxeles del mapa: un bloque mide 76) ----
// Valores medidos en el juego original (memoria del proceso): velocidad 250, salto -600, gravedad 1800,
// caída máxima 1500, caja del jugador 50x90.
// Valores del Vascolet.exe original (decompilado con REA + Ghidra: constructor Vascolet 0x430dd0 y
// actualización 0x4313c0; tablas en .data 0x50a7c0-0x50a820). Unidades del mapa (bloque = 76).
//  - gravedad 1800, caída máxima 1500, correr 250 (instantáneo en el piso), escalera 150.
//  - salto según Vascolets: 600/700/790/810/850; doble salto según Cereales: 640/680/720/760.
//  - en el aire la velocidad horizontal se acelera 20 por cuadro (10 si saltó quieto) hasta 250 y
//    sin dirección conserva la que traía.
const float RUN = 250, GRAVITY = 1800, MAX_FALL = 1500, CLIMB = 150, STOMP = -510;   // rebote al pisar: -850 x 0,6 (0x4330f0)
const float AIR_ACCEL = 20, AIR_ACCEL_STILL = 10;   // por cuadro de 60 Hz
const float ENEMY_GRAVITY = 1800, ENEMY_MAX_FALL = 1500;
// Carteles del juego: "¡Cada Vascolet que tomes aumenta tu salto!" / "...aumenta tu salto doble!"
const float JUMP_BY_VASCOLETS[5] = { -600, -700, -790, -810, -850 };
const float DJUMP_BY_CEREALS[5] = { 0, -640, -680, -720, -760 };
const int HUD_SLOTS = 4;
const float SIGN_LINGER = 1.2f;   // segundos que queda el texto del cartel al salir de él   // vasos de Vascolet y bowls de cereales que muestra el HUD

void playSfx(const char* file) { plat::playSound(plat::loadSound(std::string("assets/audio/fx/") + file)); }

static const Anim* HA(const std::string& n) { return anim(n, "AssetsAnimationsVascolet"); }
// Animación del jugador según la montura (si la montura no tiene esa animación, se usa la normal).
const Anim* World::heroAnim(const char* name) const {
    if (*mount) if (const Anim* a = HA(std::string(mount) + name)) return a;
    return HA(name);
}

static const Anim* EA(const std::string& n) { return anim(n, "AssetsAnimationsEnemies"); }
static const ImageMap* EM(const std::string& n) { return imageMap(n, "AssetsEnemies"); }

// Textos de los carteles: el juego usa "\a" = á, "\n" = salto de línea, etc.
static std::string decodeText(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '[') { out += '('; continue; }   // la letra del juego no tiene corchetes
        if (s[i] == ']') { out += ')'; continue; }
        if (s[i] != '\\' || i + 1 >= s.size()) { out += s[i]; continue; }
        const char c = s[++i];
        switch (c) {
            case 'a': out += "á"; break; case 'e': out += "é"; break; case 'i': out += "í"; break;
            case 'o': out += "ó"; break; case 'u': out += "ú"; break; case 'n': out += "\n"; break;
            case 'N': out += "ñ"; break; case '?': out += "¿"; break; case '!': out += "¡"; break;
            default: out += c; break;
        }
    }
    return out;
}

static float propf(const MapObject& o, const char* k, float d = 0) { auto s = o.prop(k); return s.empty() ? d : (float)atof(s.c_str()); }
static bool propb(const MapObject& o, const char* k) { auto s = o.prop(k); return s == "true" || s == "True"; }
static float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }

bool World::load(int level, const Counters& carry) {
    levelNo = level;
    cnt = carry;
    cnt.vascolets = 0; cnt.cereals = 0; cnt.coins = 0;
    for (int& k : cnt.keys) k = 0;
    if (!map.load(std::string("assets/data/") + LEVEL_MAP[level] + ".tmx")) return false;
    tiles = plat::loadTileset(map.tilesetImage, map.tileW, map.tileH);
    // Futuro: la cruz roja (tile 328) es una pared marcada en el editor; en el original no se ve.
    if (level == 4 && map.tileFlagsById.size() > 328) map.tileFlagsById[328] |= TF_INVISIBLE;
    // El nivel está rodeado de un bloque sólido de relleno: es el más repetido del mapa. La cámara se
    // limita a la zona con bloques "de verdad", como en el juego original.
    {
        std::map<uint32_t, int> count;
        for (uint32_t g : map.gids) if (g) count[g]++;
        int best = 0;
        for (auto& kv : count) if (kv.second > best) { best = kv.second; filler = kv.first; }
        // Es relleno si ocupa más de la mitad del mapa (en Egipto ese bloque no está marcado como sólido).
        if (best * 2 < (int)map.gids.size()) filler = 0;
        int x0 = map.width, y0 = map.height, x1 = 0, y1 = 0;
        for (int y = 0; y < map.height; y++)
            for (int x = 0; x < map.width; x++) {
                const uint32_t g = map.gids[y * map.width + x];
                if (!g || g == filler || (map.flagsAt(x, y) & TF_INVISIBLE)) continue;
                x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x + 1); y1 = std::max(y1, y + 1);
            }
        if (x1 <= x0) { x0 = 0; y0 = 0; x1 = map.width; y1 = map.height; }
        boundX0 = (float)x0 * map.tileW; boundY0 = (float)y0 * map.tileH; boundX1 = (float)x1 * map.tileW; boundY1 = (float)y1 * map.tileH;
    }
    for (auto& kv : map.layerProps) {
        int x = 0, y = 0;
        if (sscanf(kv.first.c_str(), "pMeteorite(%d,%d)", &x, &y) == 2 && kv.second == "true") meteorScreens.insert((x - 1) * 1000 + (y - 1));
    }
    {
        auto it = map.props.find("pAssetMeteorites");
        meteorImg = EM(it != map.props.end() ? it->second : std::string("METEORITE"));
    }
    riding = level == 0 || level == 4;
    mount = level == 0 ? "D_" : level == 4 ? "P_" : "";
    plat::playMusic(std::string("assets/audio/music/") + LEVEL_MUSIC[level] + ".ogg");

    for (auto& o : map.objects) {
        const Box b = { (float)o.x, (float)o.y, (float)o.w, (float)o.h };
        const std::string& t = o.type;
        const std::string asset = o.prop("pAsset");
        // El objeto de tipo "Vascolet" es siempre el punto de partida (en Piratas y Futuro quedó con
        // propiedades de ítem sobrantes del editor, que el juego original ignora).
        if (t == "Vascolet") {
            hero.box = { b.x + (b.w - 50) * 0.5f, b.y + b.h - 90, 50, 90 };
            spawnX = hero.box.x; spawnY = hero.box.y;
        } else if (t == "Item") {
            Item it; it.box = b; it.asset = asset; it.counter = o.prop("pRequiredItem");
            it.count = std::max(1, o.propi("pCount", 1));
            it.img = imageMap(asset, "AssetsItems");
            it.bob = frand(0, 1.5f);
            items.push_back(it);
        } else if (t == "Door") {
            Door d; d.box = b; d.key = o.prop("pRequiredItem"); d.img = imageMap(asset, "AssetsTilemap");
            doors.push_back(d);
        } else if (t == "PlatformInterp" || t == "Platform") {
            Platform p; p.box = b; p.sx = b.x; p.sy = b.y;
            p.dx = propf(o, "pxDest"); p.dy = propf(o, "pyDest");
            const float speed = propf(o, "pSpeed", 0);
            p.duration = speed > 0 ? std::max(0.2f, std::hypot(p.dx, p.dy) / speed) : std::max(0.2f, propf(o, "pDuration", 2));
            p.wait = propf(o, "pWait", 0.5f);
            p.img = imageMap(asset, "AssetsTilemap");
            p.lastX = b.x; p.lastY = b.y;
            platforms.push_back(p);
        } else if (t == "Information") {
            signs.push_back({ b, decodeText(o.prop("pText")) });
        } else if (t == "SavePoint") {
            saves.push_back({ b, false });
        } else if (t == "Enemy" || t == "Jumper" || t == "Bird" || t == "Fire" || t == "Thrower" ||
                   t == "Piranha" || t == "Guillotine" || t == "Stalactite") {
            Enemy e;
            e.box = b; e.x0 = b.x; e.y0 = b.y; e.h0 = b.h; e.asset = asset;
            e.animName = o.prop("pAnim").empty() ? asset : o.prop("pAnim");
            e.collidesFloor = propb(o, "pCollidesFloor"); e.collidesH = propb(o, "pCollidesH"); e.collidesV = propb(o, "pCollidesV");
            e.vx = propf(o, "pxVel"); e.vy = propf(o, "pyVel"); e.ay = propf(o, "pyAccel", 1800);   // sin pyAccel el original usa 1800 (cargador 0x421560)
            e.duration = std::max(0.1f, propf(o, "pDuration", 2)); e.wait = propf(o, "pWait");
            e.dx = propf(o, "pxDelta"); e.dy = propf(o, "pyDelta");
            e.minT = propf(o, "pMinTime", 1); e.maxT = std::max(e.minT, propf(o, "pMaxTime", e.minT));
            e.offX = propf(o, "pxOffset"); e.offY = propf(o, "pyOffset");
            e.bullet = o.prop("pBulletAsset"); e.splash = o.prop("pAnimSplash");
            e.still = EM(asset);
            e.left = e.vx < 0 || propf(o, "pLook", 0) < 0;
            if (t == "Enemy") e.kind = EK_WALKER;
            else if (t == "Jumper") { e.kind = EK_JUMPER; e.jumpVy = -600; e.savedVx = e.vx; }
            else if (t == "Bird") e.kind = EK_BIRD;
            else if (t == "Fire") { e.kind = EK_STATIC; e.stompable = asset != "FIRE" && asset != "HEDGEHOG"; }
            else if (t == "Thrower") { e.kind = EK_THROWER; e.bulletVx = e.vx; e.vx = 0; e.left = e.bulletVx < 0; e.timer = frand(e.minT, e.maxT); e.stompable = false; }
            else if (t == "Piranha") { e.kind = EK_PIRANHA; e.timer = frand(e.minT, e.maxT); e.stompable = false; }
            else if (t == "Guillotine") { e.kind = EK_GUILLOTINE; e.stompable = false; }
            else if (t == "Stalactite") { e.kind = EK_STALACTITE; e.ay = std::max(400.0f, e.ay); e.stompable = false; }
            if (const Anim* a = EA(e.animName)) e.anim.play(a);
            if (e.kind == EK_PIRANHA) { e.anim.play(EA(e.animName + "_UP")); e.box.y = e.y0; }
            enemies.push_back(e);
        }
    }
    hero.state = HS_AIR;
    hero.anim.play(heroAnim("WALK"));
    powerAnim.play(EA("POWER"));
#ifdef HOST
    // Pruebas en la PC: V1_POS=x,y pone al jugador en ese punto del mapa.
    if (const char* p = getenv("V1_POS")) { float x, y; if (sscanf(p, "%f,%f", &x, &y) == 2) { hero.box.x = x; hero.box.y = y - hero.box.h; } }
#endif
    refitCamera();
    return true;
}

void World::refitCamera() {
    cam.w = (float)LOGICAL_W; cam.h = (float)LOGICAL_H;
    cam.minX = boundX0; cam.minY = boundY0;
    cam.zoneY = 0.85f;   // como en el original: el piso queda al pie de la pantalla
    cam.x = hero.cx() - cam.w * 0.5f; cam.y = hero.bottom() - cam.h * cam.zoneY;
    roomBounds();
    cam.init(cam.levelW, cam.levelH, hero.cx(), hero.bottom());
    centerSmallRoom();
    if (!g_cameraFixed) { screenCamera(true); return; }
    camX = cam.x; camY = cam.y; prevCamX = camX; prevCamY = camY;
}

// Límites de la "habitación" donde está el jugador: la cámara no muestra columnas ni filas que sean
// todo relleno (como en el original, donde cada zona del nivel queda enmarcada por sus paredes).
bool World::fillerCell(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= map.width || ty >= map.height) return true;
    const uint32_t g = map.gids[ty * map.width + tx];
    return g == filler || (map.flagsAt(tx, ty) & TF_INVISIBLE);
}

void World::roomBounds() {
    const int tw = map.tileW, th = map.tileH;
    const int hx = (int)std::floor(hero.cx() / tw), hy = (int)std::floor((hero.box.y + hero.box.h * 0.5f) / th);
    auto colFull = [&](int c, int r0, int r1) { for (int r = r0; r <= r1; r++) if (!fillerCell(c, r)) return false; return true; };
    auto rowFull = [&](int r, int c0, int c1) { for (int c = c0; c <= c1; c++) if (!fillerCell(c, r)) return false; return true; };
    const int r0 = std::max(0, (int)std::floor(cam.y / th)), r1 = std::min(map.height - 1, (int)std::floor((cam.y + cam.h - 1) / th));
    int c = hx; while (c >= 0 && !colFull(c, r0, r1)) c--;
    float x0 = (float)(c + 1) * tw;
    c = hx; while (c < map.width && !colFull(c, r0, r1)) c++;
    float x1 = (float)c * tw;
    x0 = std::max(x0, boundX0); x1 = std::min(x1, boundX1);
    if (x1 <= x0) { x0 = boundX0; x1 = boundX1; }
    // Columnas que se verán con esos límites horizontales.
    const float vx = x1 - x0 < cam.w ? (x0 + x1 - cam.w) * 0.5f : std::max(x0, std::min(x1 - cam.w, cam.x));
    const int c0 = std::max(0, (int)std::floor(vx / tw)), c1 = std::min(map.width - 1, (int)std::floor((vx + cam.w - 1) / tw));
    int r = hy; while (r >= 0 && !rowFull(r, c0, c1)) r--;
    float y0 = (float)(r + 1) * th;
    r = hy; while (r < map.height && !rowFull(r, c0, c1)) r++;
    float y1 = (float)r * th;
    y0 = std::max(y0, boundY0); y1 = std::min(y1, boundY1);
    if (y1 <= y0) { y0 = boundY0; y1 = boundY1; }
    cam.minX = x0; cam.levelW = x1; cam.minY = y0; cam.levelH = y1;
}

// Cámara del original: el nivel está dividido en pantallas de 16x12 bloques y se ve la pantalla donde
// está el jugador; al cruzar el borde, corte directo a la siguiente. Si la vista es más chica que la
// pantalla (vista "Cerca"), dentro de la pantalla sigue al jugador; si es más grande, la centra.
void World::screenCamera(bool snap) {
    const float SW = 16.0f * map.tileW, SH = 12.0f * map.tileH;
    const float hx = hero.cx(), hy = hero.box.y + hero.box.h * 0.5f;
    const int sx = (int)std::floor(hx / SW), sy = (int)std::floor(hy / SH);
    const bool cut = snap || sx != screenX || sy != screenY;
    screenX = sx; screenY = sy;
    auto axis = [](float cell0, float cellSize, float view, float focus, float cur, bool jump, float dt) {
        if (view >= cellSize) return cell0 + (cellSize - view) * 0.5f;
        const float target = std::max(cell0, std::min(cell0 + cellSize - view, focus - view * 0.5f));
        if (jump) return target;
        const float k = std::min(1.0f, dt * 8.0f);     // seguimiento suave dentro de la pantalla
        return std::max(cell0, std::min(cell0 + cellSize - view, cur + (target - cur) * k));
    };
    const float dt = 1.0f / 60.0f;
    camX = axis(sx * SW, SW, cam.w, hx, camX, cut, dt);
    camY = axis(sy * SH, SH, cam.h, hy - cam.h * 0.08f, camY, cut, dt);
    cam.x = camX; cam.y = camY;
    if (cut) { prevCamX = camX; prevCamY = camY; }   // sin arrastre de interpolación en el corte
}

void World::centerSmallRoom() {
    if (cam.levelW - cam.minX < cam.w) cam.x = (cam.minX + cam.levelW - cam.w) * 0.5f;
    if (cam.levelH - cam.minY < cam.h) cam.y = (cam.minY + cam.levelH - cam.h) * 0.5f;
}

// ---------------- Colisiones con el mapa ----------------
bool World::solidAt(float x, float y) const {
    const int tx = (int)std::floor(x / map.tileW), ty = (int)std::floor(y / map.tileH);
    if (tx < 0 || tx >= map.width) return true;
    if (ty < 0) return false;
    if ((map.flagsAt(tx, ty) & TF_HARD) != 0) return true;
    for (auto& d : doors)
        if (!d.gone && x >= d.box.x && x < d.box.x + d.box.w && y >= d.box.y + d.off && y < d.box.y + d.box.h) return true;
    return false;
}

// Base del primer bloque de líquido (agua, lava, ácido) debajo de un punto: la salpicadura dibuja un
// bloque de líquido de 76 con el chorro arriba, así que se apoya ahí y el chorro asoma sobre la superficie.
float World::liquidBottom(float x, float y) const {
    const int tx = (int)std::floor(x / map.tileW), ty0 = (int)std::floor(y / map.tileH);
    for (int ty = ty0; ty < ty0 + 10; ty++)
        if (map.gidAt(tx, ty) && (map.flagsAt(tx, ty) & TF_KILLING)) return (float)(ty + 1) * map.tileH;
    return y + map.tileH * 2.0f;
}

bool World::platformTop(float x, float y, float prevBottom) const {
    const int tx = (int)std::floor(x / map.tileW), ty = (int)std::floor(y / map.tileH);
    const uint8_t f = map.flagsAt(tx, ty);
    if (!(f & (TF_PLATFORM | TF_LADDER_END))) return false;
    return prevBottom <= ty * map.tileH + 1;
}

bool World::ladderAt(float x, float y) const {
    const int tx = (int)std::floor(x / map.tileW), ty = (int)std::floor(y / map.tileH);
    return (map.flagsAt(tx, ty) & (TF_LADDER | TF_LADDER_END)) != 0;
}

bool World::killingAt(const Box& b) const {
    for (float x = b.x + 4; x < b.x + b.w - 4; x += map.tileW * 0.5f)
        for (float y = b.y + 8; y < b.y + b.h; y += map.tileH * 0.5f)
            if (map.flagsAt((int)std::floor(x / map.tileW), (int)std::floor(y / map.tileH)) & TF_KILLING) return true;
    return b.y > map.height * map.tileH;
}

// Mueve una caja contra los bloques sólidos (eje X y después eje Y).
void World::moveBox(Box& b, float& vx, float& vy, float dt, bool solidPlatforms, bool* ground, bool* wall) {
    if (ground) *ground = false;
    if (wall) *wall = false;
    const float dx = vx * dt, dy = vy * dt;
    // Horizontal
    b.x += dx;
    if (dx != 0) {
        const float edge = dx > 0 ? b.x + b.w - 0.01f : b.x;
        bool hit = false;
        for (float y = b.y + 2; y < b.y + b.h - 2 && !hit; y += 20) hit = solidAt(edge, y);
        if (!hit) hit = solidAt(edge, b.y + b.h - 3);
        if (hit) {
            const int tx = (int)std::floor(edge / map.tileW);
            b.x = dx > 0 ? tx * map.tileW - b.w : (tx + 1) * map.tileW;
            for (auto& d : doors)   // una puerta cerrada frena igual que un bloque
                if (!d.gone && b.y + b.h > d.box.y + d.off && b.y < d.box.y + d.box.h) {
                    if (dx > 0 && b.x + b.w > d.box.x && b.x < d.box.x) b.x = d.box.x - b.w;
                    if (dx < 0 && b.x < d.box.x + d.box.w && b.x + b.w > d.box.x + d.box.w) b.x = d.box.x + d.box.w;
                }
            vx = 0;
            if (wall) *wall = true;
        }
    }
    // Vertical
    const float prevBottom = b.y + b.h;
    b.y += dy;
    if (dy > 0) {
        const float edge = b.y + b.h;
        bool hit = false;
        for (float x = b.x + 3; x < b.x + b.w - 3 && !hit; x += 18)
            hit = solidAt(x, edge) || (solidPlatforms && platformTop(x, edge, prevBottom));
        if (!hit) hit = solidAt(b.x + b.w - 3, edge) || (solidPlatforms && platformTop(b.x + b.w - 3, edge, prevBottom));
        if (hit) {
            const int ty = (int)std::floor(edge / map.tileH);
            b.y = ty * map.tileH - b.h;
            vy = 0;
            if (ground) *ground = true;
        }
    } else if (dy < 0) {
        bool hit = false;
        for (float x = b.x + 3; x < b.x + b.w - 3 && !hit; x += 18) hit = solidAt(x, b.y);
        if (hit) {
            const int ty = (int)std::floor(b.y / map.tileH);
            b.y = (ty + 1) * map.tileH;
            vy = 0;
        }
    }
}

// ---------------- Jugador ----------------
void World::hurtHero(float fromX) {
    if (hero.invuln > 0 || powerT > 0 || hero.state == HS_DEAD || hero.state == HS_WIN) return;
    // En el original un golpe cuesta una vida.
    killHero(); return;
    hero.invuln = 1.6f;
    hero.state = HS_HURT; hero.hurtT = 0.45f;
    hero.vx = hero.cx() < fromX ? -260.0f : 260.0f;
    hero.vy = -520;
    hero.anim.play(heroAnim("HURT"), true);
}

void World::killHero() {
    if (hero.state == HS_DEAD) return;
    hero.state = HS_DEAD; hero.deadT = 0;
    hero.vx = 0; hero.vy = -700;
    playSfx("death_2.ogg");
}

void World::respawn() {
    cnt.lives--;
    if (cnt.lives < 0) { cnt.lives = 0; outOfLives = true; return; }
    hero.box.x = spawnX; hero.box.y = spawnY;
    hero.vx = hero.vy = 0;
    hero.state = HS_AIR; hero.invuln = 2.0f; hero.platform = -1;
    hero.anim.play(heroAnim("WALK"), true);
    // Los enemigos que cayeron (estalactitas) vuelven a su lugar.
    for (auto& e : enemies) if (e.kind == EK_STALACTITE) { e.falling = false; e.dead = false; e.box.x = e.x0; e.box.y = e.y0; e.vy = 0; }
    shots.clear();
    refitCamera();
}

void World::award(const Item& it) {
    const std::string& c = it.counter;
    if (c == "VASCOLETS" || c == "CEREALS") spawnEffect("POWER", hero.cx(), hero.box.y + hero.box.h * 0.5f);
    // Como el original (0x433fb0 / 0x42db50): solo los ítems de puntos suman; cada 2.000 puntos, una vida
    // (máximo 99 vidas y 999.999 puntos). Vascolets, cereales y enemigos no dan puntos.
    if (c == "POINTS") {
        const int before = cnt.points / 2000;
        cnt.points = std::min(999999, cnt.points + it.count);
        if (cnt.points / 2000 > before) cnt.lives = std::min(99, cnt.lives + cnt.points / 2000 - before);
        if (it.asset == "COIN") cnt.coins++;
        playSfx("item.ogg");
    }
    else if (c == "LIVES") { cnt.lives = std::min(99, cnt.lives + it.count); playSfx("extra_life.ogg"); }
    else if (c == "VASCOLETS") { cnt.vascolets = std::min(HUD_SLOTS, cnt.vascolets + 1); playSfx("jump_vascolet.ogg"); }
    else if (c == "CEREALS") { cnt.cereals = std::min(HUD_SLOTS, cnt.cereals + 1); playSfx("powerup.ogg"); }
    else if (c == "YELLOW_KEY") { cnt.keys[0] = std::min(9, cnt.keys[0] + 1); playSfx("item.ogg"); }
    else if (c == "RED_KEY") { cnt.keys[1] = std::min(9, cnt.keys[1] + 1); playSfx("item.ogg"); }
    else if (c == "GREEN_KEY") { cnt.keys[2] = std::min(9, cnt.keys[2] + 1); playSfx("item.ogg"); }
    else if (c == "BLUE_KEY") { cnt.keys[3] = std::min(9, cnt.keys[3] + 1); playSfx("item.ogg"); }
    else if (c == "MICROWAVE") {
        hero.state = HS_WIN; hero.vx = 0; hero.vy = 0; winT = 0;
        spawnEffect("POWER", hero.cx(), hero.box.y + hero.box.h * 0.5f);   // como en el original: desaparece en un destello
        plat::stopMusic(); playSfx("microwave.ogg");
    } else playSfx("item.ogg");
}

void World::spawnEffect(const char* name, float x, float y) {
    Effect ef; ef.x = x; ef.y = y; ef.anim.play(EA(name));
    ef.additive = std::string(name) == "POWER";
    if (ef.anim.a) effects.push_back(ef);
}

void World::updateHero(float dt) {
    Hero& h = hero;
    h.anim.update(dt);
    if (h.invuln > 0) h.invuln -= dt;
    if (powerT > 0) { powerT -= dt; powerAnim.update(dt); }

    if (h.state == HS_DEAD) {
        h.deadT += dt;
        h.vy = std::min(h.vy + GRAVITY * dt, MAX_FALL);
        h.box.y += h.vy * dt;
        if (h.deadT > 1.6f) respawn();
        return;
    }
    if (h.state == HS_WIN) { winT += dt; return; }

    const bool L = plat::held(plat::BTN_LEFT), R = plat::held(plat::BTN_RIGHT);
    const bool U = plat::held(plat::BTN_UP), D = plat::held(plat::BTN_DOWN);
    const bool jump = plat::pressed(plat::BTN_JUMP) || plat::pressed(plat::BTN_SHOOT) || plat::pressed(plat::BTN_ACCEPT);

    // Escaleras (no se puede trepar montado en el dinosaurio).
    const float midX = h.cx();
    if (h.state != HS_CLIMB && !riding && h.state != HS_HURT &&
        ((U && ladderAt(midX, h.box.y + h.box.h * 0.6f)) || (D && h.onGround && ladderAt(midX, h.bottom() + 4)))) {
        h.state = HS_CLIMB; h.vx = 0; h.vy = 0;
        h.box.x = std::floor(midX / map.tileW) * map.tileW + (map.tileW - h.box.w) * 0.5f;
        if (D && h.onGround) h.box.y += 8;
        h.anim.play(HA("CLIMB"));
    }
    if (h.state == HS_CLIMB) {
        h.vy = U ? -CLIMB : D ? CLIMB : 0;
        if (U || D) { stepT -= dt; if (stepT <= 0) { stepT = 0.32f; stepAlt = !stepAlt; playSfx(stepAlt ? "stair_1.ogg" : "stair_2.ogg"); } }
        if (!U && !D) h.anim.time -= dt;   // quieto en la escalera
        const float prevBottom = h.bottom();
        h.box.y += h.vy * dt;
        // Arriba de todo: si los pies pasan el borde superior de la escalera, queda parado arriba.
        if (U && !ladderAt(h.cx(), h.bottom() - 2)) {
            const float top = std::floor((prevBottom - 1) / map.tileH) * map.tileH;
            h.box.y = top - h.box.h;
            h.state = HS_GROUND; h.onGround = true; h.vy = 0;
            h.anim.play(HA("WALK"), true); h.anim.time = 0;
            return;
        }
        // Se suelta solo si ya no tiene escalera ni en el cuerpo ni bajo los pies (antes se soltaba
        // justo antes del borde de arriba, se caía y volvía a agarrarse: nunca terminaba de subir).
        if (!ladderAt(h.cx(), h.box.y + h.box.h * 0.85f) && !ladderAt(h.cx(), h.box.y + h.box.h * 0.3f) && !ladderAt(h.cx(), h.bottom() - 2)) {
            h.state = HS_AIR; h.vy = 0;
        }
        bool g = false; float vx = 0, vy = D ? 1 : 0;
        if (D) { moveBox(h.box, vx, vy, 0.0001f, false, &g, nullptr); if (g && !ladderAt(h.cx(), h.bottom() + 2)) { h.state = HS_GROUND; h.onGround = true; } }
        if (jump && (L || R)) { h.state = HS_AIR; h.vy = JUMP_BY_VASCOLETS[std::min(cnt.vascolets, 4)]; h.left = L; h.doubleJumped = false; }
        // Como en el original: con izquierda/derecha se baja de la escalera hacia ese lado.
        // Si al costado hay un escalón a la altura de los pies, sube a ese escalón.
        else if ((L || R) && !U && !D) {
            const float col = std::floor(h.cx() / map.tileW) * map.tileW;   // columna de la escalera
            const float side = L ? col - 2 : col + map.tileW + 2;
            const int row = (int)std::floor((h.bottom() - 1) / map.tileH);
            const float rowTop = (float)row * map.tileH;
            if (solidAt(side, h.bottom() - 1) && !solidAt(side, rowTop - 2)) h.box.y = rowTop - h.box.h;
            h.state = HS_AIR; h.vy = 0; h.left = L;
            h.vx = L ? -RUN : RUN;
        }
        return;
    }

    if (h.state == HS_HURT) {
        h.hurtT -= dt;
        if (h.hurtT <= 0 && h.onGround) h.state = HS_GROUND;
    } else {
        const float dir = (R ? 1.0f : 0.0f) - (L ? 1.0f : 0.0f);
        if (h.onGround) h.vx = dir * RUN;
        else {
            // Control en el aire del original: acelera hacia la dirección elegida, sin frenar solo.
            h.vx += dir * (h.stillJump ? AIR_ACCEL_STILL : AIR_ACCEL) * dt * 60.0f;
            h.vx = std::max(-RUN, std::min(RUN, h.vx));
        }
        if (L && !R) h.left = true;
        if (R && !L) h.left = false;
        if (jump && !h.onGround && h.state == HS_AIR && cnt.cereals > 0 && !h.doubleJumped) {
            // Doble salto: se habilita con el primer Cereal; cada Cereal lo hace más alto.
            h.vy = DJUMP_BY_CEREALS[std::min(cnt.cereals, 4)]; h.doubleJumped = true; h.stillJump = !L && !R;
            playSfx("jump.ogg");
            h.anim.play(heroAnim("JUMP"), true);
        }
        if (jump && h.onGround) {
            h.vy = JUMP_BY_VASCOLETS[std::min(cnt.vascolets, 4)]; h.onGround = false; h.state = HS_AIR; h.platform = -1; h.doubleJumped = false;
            h.stillJump = !L && !R;
            playSfx("jump.ogg");
            h.anim.play(heroAnim("JUMP"), true);
        }
    }

    // Plataforma móvil: lleva al jugador.
    if (h.platform >= 0 && h.platform < (int)platforms.size()) {
        const Platform& p = platforms[h.platform];
        h.box.x += p.box.x - p.lastX; h.box.y += p.box.y - p.lastY;
    }

    h.vy = std::min(h.vy + GRAVITY * dt, MAX_FALL);
    const float prevBottom = h.bottom();
    bool ground = false;
    moveBox(h.box, h.vx, h.vy, dt, !D, &ground, nullptr);
    // Plataformas móviles como piso.
    h.platform = -1;
    if (h.vy >= 0)
        for (size_t i = 0; i < platforms.size(); i++) {
            const Box& pb = platforms[i].box;
            if (h.box.x + h.box.w > pb.x + 6 && h.box.x < pb.x + pb.w - 6 && prevBottom <= pb.y + 12 && h.bottom() >= pb.y) {
                h.box.y = pb.y - h.box.h; h.vy = 0; ground = true; h.platform = (int)i;
            }
        }
    const bool wasGround = h.onGround;
    h.onGround = ground;
    if (ground) h.doubleJumped = false;
    if (ground && !wasGround && h.state == HS_AIR) h.state = HS_GROUND;
    if (!ground && h.state == HS_GROUND) h.state = HS_AIR;
    // Pasos: alternados a pie o con el dinosaurio; la patineta tiene su zumbido.
    if (ground && h.vx != 0 && h.state == HS_GROUND) {
        stepT -= dt;
        if (stepT <= 0) {
            stepAlt = !stepAlt;
            if ((*mount == 'P')) { stepT = 0.6f; playSfx("skate.ogg"); }
            else { stepT = riding ? 0.42f : 0.3f; playSfx(riding ? (stepAlt ? "footstep_dino_1.ogg" : "footstep_dino_2.ogg") : (stepAlt ? "footstep_1.ogg" : "footstep_2.ogg")); }
        }
    } else if (ground) stepT = 0;

    // Animación
    if (h.state == HS_GROUND) {
        const Anim* walk = heroAnim("WALK");
        if (h.anim.a != walk) h.anim.play(walk);
        if (h.vx == 0) h.anim.time = 0;     // parado: primer cuadro
    }
    if (killingAt(h.box)) killHero();
}

// ---------------- Plataformas ----------------
void World::updatePlatforms(float dt) {
    for (auto& p : platforms) {
        p.lastX = p.box.x; p.lastY = p.box.y;
        if (p.waitT > 0) { p.waitT -= dt; continue; }
        p.t += dt * p.dir / p.duration;
        if (p.t >= 1) { p.t = 1; p.dir = -1; p.waitT = p.wait; }
        if (p.t <= 0) { p.t = 0; p.dir = 1; p.waitT = p.wait; }
        const float k = 0.5f - 0.5f * std::cos(p.t * 3.14159265f);
        p.box.x = p.sx + p.dx * k; p.box.y = p.sy + p.dy * k;
    }
}

// ---------------- Enemigos ----------------
void World::updateEnemies(float dt) {
    const Box hb = hero.box;
    const float sx0 = camX, sy0 = camY, sx1 = camX + cam.w, sy1 = camY + cam.h;
    for (auto& e : enemies) {
        if (e.dead) continue;
        // Como en el original, cada enemigo funciona solo mientras su pantalla está a la vista.
        if (!g_cameraFixed && (e.box.x + e.box.w < sx0 || e.box.x > sx1 || e.box.y + e.box.h < sy0 || e.box.y > sy1)) continue;
        e.anim.update(dt);
        switch (e.kind) {
            case EK_WALKER: case EK_JUMPER: {
                bool g = false, wall = false;
                float vx = e.vx;
                bool turn = false;
                // Darse vuelta en el borde de una plataforma o de un pozo (solo si está parado en el piso).
                const float feet = e.box.y + e.box.h;
                // Los del mapa quedan unos píxeles sobre el piso (no tienen gravedad): se mira hasta 20 abajo.
                const float probe = feet + 20;
                const bool standing = e.vy >= 0 && (solidAt(e.box.x + e.box.w * 0.5f, probe) || platformTop(e.box.x + e.box.w * 0.5f, probe, feet));
                if (e.collidesFloor && e.kind == EK_WALKER && standing && e.vx != 0) {
                    const float ahead = e.vx > 0 ? e.box.x + e.box.w + 2 : e.box.x - 2;
                    const float below = probe;
                    if (!solidAt(ahead, below) && !platformTop(ahead, below, e.box.y + e.box.h)) { turn = true; vx = 0; }
                }
                // Aceleración del mapa (pyAccel): 0 en los que caminan, 800 en las serpientes saltarinas.
                e.vy = std::min(e.vy + e.ay * dt, ENEMY_MAX_FALL);
                if (e.kind == EK_JUMPER && e.landed) {
                    // Quieto en el piso hasta que termina la animación; después salta de nuevo.
                    vx = 0; e.vy = 0;
                    if (e.anim.finished() || (e.landT += dt) > 0.5f) {
                        e.landed = false; e.landT = 0; e.vx = e.savedVx; e.vy = e.jumpVy; vx = e.vx;
                        if (const Anim* a = EA(e.animName)) e.anim.play(a, true);
                    }
                }
                moveBox(e.box, vx, e.vy, dt, true, &g, &wall);
                if (wall) turn = true;
                if (turn) { e.vx = -e.vx; if (e.kind == EK_JUMPER) e.savedVx = e.vx; }
                if (e.kind == EK_JUMPER && g && !e.landed && e.vy >= 0) {
                    e.landed = true; e.landT = 0; e.vy = 0;
                    if (const Anim* a = EA(e.animName)) e.anim.play(a, true);
                }
                if (e.vx != 0) e.left = e.vx < 0;
                break;
            }
            case EK_BIRD: {
                e.t += dt;
                const float ph = std::fmod(e.t / e.duration, 2.0f), k = ph < 1 ? ph : 2 - ph;
                const float nx = e.x0 + e.dx * (0.5f - 0.5f * std::cos(k * 3.14159265f));
                e.left = (nx < e.box.x);
                e.box.x = nx;
                e.box.y = e.y0 + std::sin(e.t * 3.0f) * e.dy;
                break;
            }
            case EK_STATIC: break;
            case EK_THROWER: {
                e.timer -= dt;
                if (e.timer <= 0 && std::fabs(hb.x - e.box.x) < LOGICAL_W) {
                    e.timer = frand(e.minT, e.maxT);
                    if (const Anim* a = EA(e.animName)) e.anim.play(a, true);
                    // Como EnemyThrower (0x440580): la bala sale en (centro + pxOffset, pie + pyOffset),
                    // corrida su ancho si dispara a la izquierda; ese punto es su esquina superior izquierda.
                    Shot s; s.img = EM(e.bullet);
                    const Frame* bf = s.img && !s.img->frames.empty() ? &s.img->frames[0] : nullptr;
                    const float bw = bf ? bf->w : 60, px = bf ? bf->pivotX : 30, py = bf ? bf->pivotY : 46;
                    const float tlx = e.box.x + e.box.w * 0.5f + e.offX - (e.offX < 0 ? bw : 0), tly = e.box.y + e.box.h + e.offY;
                    s.x = tlx + px; s.y = tly + py; s.cy = py - (bf ? bf->h * 0.5f : 30);
                    s.vx = e.bulletVx; s.vy = 0;
                    shots.push_back(s);
                    playSfx(e.asset == "CANNON_LASER" ? "laser.ogg" : "cannon.ogg");
                }
                break;
            }
            case EK_PIRANHA: {
                // Salta desde el líquido (agua, lava o ácido) y vuelve a caer.
                if (e.timer > 0) {
                    e.timer -= dt;
                    if (e.timer <= 0) {
                        e.t = 0; e.up = true;
                        e.anim.play(EA(e.animName + "_UP"), true);
                        if (!e.splash.empty()) { spawnEffect((e.splash + "_OUT").c_str(), e.x0 + e.box.w * 0.5f, liquidBottom(e.x0 + e.box.w * 0.5f, e.y0)); }
                    }
                    e.box.y = e.y0 + 2000;   // escondida
                    break;
                }
                e.t += dt;
                const float k = std::min(1.0f, e.t / e.duration);
                e.box.y = e.y0 + e.dy * std::sin(k * 3.14159265f);
                if (e.up && k > 0.5f) { e.up = false; e.anim.play(EA(e.animName + "_DOWN"), true); }
                if (k >= 1) {
                    e.timer = frand(e.minT, e.maxT);
                    if (!e.splash.empty()) spawnEffect((e.splash + "_IN").c_str(), e.x0 + e.box.w * 0.5f, liquidBottom(e.x0 + e.box.w * 0.5f, e.y0));
                }
                break;
            }
            case EK_GUILLOTINE: {
                // Cae rápido, espera y vuelve a subir.
                e.t += dt;
                const float cycle = e.duration * 2 + e.wait * 2, p = std::fmod(e.t, cycle);
                float k;
                if (p < e.wait) k = 0;
                else if (p < e.wait + e.duration) k = (p - e.wait) / e.duration;
                else if (p < e.wait * 2 + e.duration) k = 1;
                else k = 1 - (p - e.wait * 2 - e.duration) / e.duration;
                const float prevK = e.vy; e.vy = k;
                if (k >= 1 && prevK < 1 && std::fabs(hb.x - e.box.x) < LOGICAL_W) playSfx("guillotine.ogg");
                e.box.h = std::max(20.0f, e.h0 * (0.25f + 0.75f * k));
                break;
            }
            case EK_STALACTITE: {
                // Como EnemyStalactite (0x43ff40..0x440060): cae cuando el jugador pasa debajo, entre ella
                // y el primer bloque sólido que tiene abajo (calculado al cargar).
                if (e.dy == 0) {
                    float y = e.box.y + e.box.h;
                    while (y < map.height * map.tileH && !solidAt(e.box.x + e.box.w * 0.5f, y)) y += map.tileH;
                    e.dy = y - e.box.y;
                }
                if (!e.falling && std::fabs(hero.cx() - (e.box.x + e.box.w * 0.5f)) < std::max(60.0f, e.offX) &&
                    hb.y + hb.h > e.box.y && hb.y < e.box.y + e.dy) e.falling = true;
                if (e.falling) {
                    e.vy += e.ay * dt; e.box.y += e.vy * dt;
                    if (solidAt(e.box.x + e.box.w * 0.5f, e.box.y + e.box.h)) { e.dead = true; spawnEffect("ENEMY_DEATH", e.box.x + e.box.w * 0.5f, e.box.y + e.box.h * 0.5f); }
                }
                break;
            }
        }
        // Contacto con el jugador.
        if (hero.state == HS_DEAD || hero.state == HS_WIN || e.dead) continue;
        Box eb = e.box;
        if (e.kind == EK_GUILLOTINE) eb = { e.box.x + 8, e.box.y, e.box.w - 16, e.box.h };
        if (!overlap(hb, eb)) continue;
        const bool fromAbove = hero.vy > 0 && hb.y + hb.h - hero.vy * (1.0f / 60.0f) <= eb.y + 18;
        if (powerT > 0 && e.stompable) {
            e.dead = true;
            spawnEffect("ENEMY_DEATH", eb.x + eb.w * 0.5f, eb.y + eb.h * 0.5f); playSfx("explosion.ogg");
        } else if (fromAbove && e.stompable) {
            e.dead = true;
            hero.vy = STOMP; hero.state = HS_AIR;
            spawnEffect("ENEMY_DEATH", eb.x + eb.w * 0.5f, eb.y + eb.h * 0.5f); playSfx("explosion.ogg");
        } else hurtHero(eb.x + eb.w * 0.5f);
    }

    for (auto& s : shots) {
        s.x += s.vx * dt; s.y += s.vy * dt;
        if (solidAt(s.x + (s.vx > 0 ? 20 : -20), s.y - s.cy) || std::fabs(s.x - hero.cx()) > LOGICAL_W * 1.5f) { s.alive = false; continue; }
        if (hero.state != HS_DEAD && overlap(hero.box, { s.x - 14, s.y - s.cy - 14, 28, 28 })) { s.alive = false; hurtHero(s.x); }
    }
    shots.erase(std::remove_if(shots.begin(), shots.end(), [](const Shot& s) { return !s.alive; }), shots.end());
}

// ---------------- Cuadro ----------------
// Lluvia de meteoritos del original: en las pantallas marcadas en el mapa (pMeteorite(x,y), contando
// desde 1) cae uno cada 3 a 4 s desde arriba, en diagonal a 250 de velocidad con gravedad 400;
// rebota dos veces en el piso (-650 y -600, gravedad 900 después) y mata al tocar al jugador.
void World::updateMeteorites(float dt) {
    const float SW = 16.0f * map.tileW, SH = 12.0f * map.tileH;
    if (meteorScreens.count(screenX * 1000 + screenY)) {
        meteorT -= dt;
        if (meteorT < 0) {
            meteorT = frand(3.0f, 4.0f);
            Meteor m;
            // En el video caen siempre de arriba hacia abajo y a la derecha.
            const float a = frand(0.6f, 1.2f);
            m.x = screenX * SW + frand(-150, 700);
            m.y = screenY * SH - 65;
            m.vx = 250 * std::cos(a);
            m.vy = 250 * std::sin(a);
            m.ay = 400;
            meteors.push_back(m);
        }
    }
    for (auto& m : meteors) {
        m.vy += m.ay * dt;
        m.x += m.vx * dt; m.y += m.vy * dt;
        if (m.vy > 0 && solidAt(m.x, m.y + 20) && m.bounces < 2) {
            m.y = std::floor((m.y + 20) / map.tileH) * map.tileH - 20;
            m.vy = m.bounces * 50.0f - 650.0f; m.ay = 900; m.bounces++;
        }
        if (hero.state != HS_DEAD && hero.state != HS_WIN && overlap(hero.box, { m.x - 22, m.y - 22, 44, 44 })) killHero();
        if (m.y > (screenY + 1) * SH + 100 || m.x < screenX * SW - 400 || m.x > (screenX + 1) * SW + 400) m.alive = false;
    }
    meteors.erase(std::remove_if(meteors.begin(), meteors.end(), [](const Meteor& m) { return !m.alive; }), meteors.end());
}

void World::update(float dt) {
    prevCamX = camX; prevCamY = camY; prevHeroX = hero.box.x; prevHeroY = hero.box.y;
    updatePlatforms(dt);
    updateHero(dt);
    updateEnemies(dt);
    updateMeteorites(dt);

    if (messageT > 0) messageT -= dt;
    if (savedT > 0) savedT -= dt;
    for (auto& ef : effects) ef.anim.update(dt);
    effects.erase(std::remove_if(effects.begin(), effects.end(), [](const Effect& e) { return e.anim.finished(); }), effects.end());

    if (hero.state != HS_DEAD) {
        for (auto& it : items) {
            it.bob += dt;
            if (!it.taken && overlap(hero.box, it.box)) { it.taken = true; award(it); }
        }
        for (auto& d : doors) {
            if (d.gone) continue;
            if (d.opening) { d.off += 260 * dt; if (d.off >= d.box.h) d.gone = true; continue; }
            const Box near = { d.box.x - 10, d.box.y, d.box.w + 20, d.box.h };
            if (!overlap(hero.box, near)) continue;
            int* k = d.key == "YELLOW_KEY" ? &cnt.keys[0] : d.key == "RED_KEY" ? &cnt.keys[1] : d.key == "GREEN_KEY" ? &cnt.keys[2] : d.key == "BLUE_KEY" ? &cnt.keys[3] : nullptr;
            if (k && *k > 0) { (*k)--; d.opening = true; playSfx("door.ogg"); }
        }
        for (auto& s : saves)
            if (!s.active && overlap(hero.box, s.box)) {
                for (auto& o : saves) o.active = false;
                s.active = true; spawnX = s.box.x; spawnY = s.box.y + s.box.h - hero.box.h;
                savedT = 2.5f;
                playSfx("powerup.ogg");
            }
        activeSign = nullptr;
        for (auto& s : signs) if (overlap(hero.box, s.box)) activeSign = &s;
        if (activeSign && activeSign != lastSign) showMessage(activeSign->text, SIGN_LINGER);
        if (activeSign) messageT = std::max(messageT, SIGN_LINGER);
        lastSign = activeSign;
    }

    if (hero.state != HS_DEAD) {
        if (g_cameraFixed) {
            roomBounds();
            cam.update(dt, hero.cx(), hero.bottom(), hero.left ? -1 : 1, hero.vy, hero.box.h);
            centerSmallRoom();
            camX = cam.x; camY = cam.y;
        } else screenCamera(false);
    }
}

// ---------------- Dibujo ----------------
void World::drawBackground() { plat::fillScreen({ 20, 16, 26, 255 }); }

void World::draw() {
    const float rx = camX, ry = camY, hx = hero.box.x, hy = hero.box.y;
    if (interp < 1.0f) {
        auto lerp = [&](float a, float b) { return std::fabs(b - a) > 96.0f ? b : a + (b - a) * interp; };
        camX = lerp(prevCamX, camX); camY = lerp(prevCamY, camY);
        hero.box.x = lerp(prevHeroX, hero.box.x); hero.box.y = lerp(prevHeroY, hero.box.y);
    }
    drawFrameContents();
    camX = rx; camY = ry; hero.box.x = hx; hero.box.y = hy;
}

void World::drawFrameContents() {
    const bool hd = plat::hdGraphics();
    drawBackground();
    const int cx = (int)std::lround(camX), cy = (int)std::lround(camY);
    if (hd) { hdBackdrop(cx, cy); hdTileShadows(cx, cy); }
    if (tiles) {
        const int tx0 = (int)std::floor(cx / (float)map.tileW), ty0 = (int)std::floor(cy / (float)map.tileH);
        for (int ty = ty0; ty <= ty0 + LOGICAL_H / map.tileH + 1; ty++)
            for (int tx = tx0; tx <= tx0 + LOGICAL_W / map.tileW + 1; tx++) {
                const uint32_t g = map.gidAt(tx, ty);
                if (!g || g == filler || (map.flagsAt(tx, ty) & TF_INVISIBLE)) continue;
                plat::drawTile(tiles, (int)g - map.firstGid, SDL_FRect{ (float)(tx * map.tileW - cx), (float)(ty * map.tileH - cy), (float)map.tileW, (float)map.tileH });
            }
    }
    if (hd) hdTileRims(cx, cy);
    auto onScreen = [&](float x, float y) { return x > cx - 300 && x < cx + LOGICAL_W + 300 && y > cy - 400 && y < cy + LOGICAL_H + 400; };

    const ImageMap* signImg = imageMap("SIGN", "AssetsTilemap");
    const ImageMap* saveImg = imageMap("SAVE", "AssetsTilemap");
    for (auto& s : signs) if (signImg && onScreen(s.box.x, s.box.y)) drawFrame(signImg->frames[0], s.box.x + s.box.w * 0.5f - cx, s.box.y + s.box.h - cy);
    for (auto& s : saves) if (saveImg && onScreen(s.box.x, s.box.y)) drawFrame(saveImg->frames[0], s.box.x + s.box.w * 0.5f - cx, s.box.y + s.box.h - cy, false, s.active ? 255 : 170);
    for (auto& d : doors) {
        if (d.gone || !d.img || d.img->frames.empty() || !onScreen(d.box.x, d.box.y)) continue;
        const Frame& f = d.img->frames[0];
        const float visible = std::min(f.h, d.box.h) - d.off;
        if (visible > 0) drawFrameRect(f, 0, 0, f.w, visible, d.box.x + d.box.w * 0.5f - f.w * 0.5f - cx, d.box.y + d.off - cy);
    }
    for (auto& p : platforms)
        if (p.img && !p.img->frames.empty() && onScreen(p.box.x, p.box.y)) {
            const Frame& f = p.img->frames[0];
            drawFrame(f, p.box.x + p.box.w * 0.5f - cx, p.box.y + f.pivotY - cy);
        }
    for (auto& it : items) {
        if (it.taken || !it.img || it.img->frames.empty() || !onScreen(it.box.x, it.box.y)) continue;
        const float bob = std::sin(it.bob * 3.0f) * 5.0f;
        const float x = it.box.x + it.box.w * 0.5f - cx, y = it.box.y + it.box.h + bob - cy;
        if (hd) { hdAddLight(x + cx, y + cy - 38, 110, 1.0f, 0.85f, 0.45f, 0.22f, 0.24f, false); hdSpriteShadow(it.img->frames[0], x, y, false); }
        drawFrame(it.img->frames[0], x, y);
    }
    for (auto& e : enemies) {
        if (e.dead || !onScreen(e.box.x, e.box.y)) continue;
        const Frame* f = e.anim.a ? e.anim.current() : (e.still && !e.still->frames.empty() ? &e.still->frames[0] : nullptr);
        if (!f) continue;
        const float x = e.box.x + e.box.w * 0.5f - cx, y = e.box.y + e.box.h - cy;
        if (e.kind == EK_GUILLOTINE) {   // la cuchilla baja desde arriba
            const float vis = std::min(f->h, e.box.h);
            drawFrameRect(*f, 0, f->h - vis, f->w, vis, x - f->w * 0.5f, e.box.y - cy);
            continue;
        }
        if (e.kind == EK_PIRANHA && e.timer > 0) continue;
        // Como en el original (Enemy 0x43e1e0): la escala X del dibujo es el signo de la velocidad;
        // todos los dibujos miran a la derecha, así que se espeja solo cuando va a la izquierda.
        const bool flip = e.left;
        if (hd) hdSpriteShadow(*f, x, y, flip);
        drawFrame(*f, x, y, flip);
    }
    if (meteorImg && !meteorImg->frames.empty())
        for (auto& m : meteors) drawFrame(meteorImg->frames[0], m.x - cx, m.y + 20 - cy, false);
    for (auto& s : shots) if (s.img && !s.img->frames.empty()) {
        if (hd) hdAddLight(s.x, s.y, 90, 1.0f, 0.75f, 0.4f, 0.4f, 0.4f, true);
        drawFrame(s.img->frames[0], s.x - cx, s.y - cy, s.vx < 0);
    }

    // Jugador (parpadea mientras es invulnerable).
    const Frame* hf = hero.anim.current();
    if (hero.state == HS_DEAD) { const ImageMap* d = imageMap(std::string(mount) + "VASCOLET_DEAD", "AssetsVascolet"); hf = d && !d->frames.empty() ? &d->frames[0] : hf; }
    const bool blink = hero.invuln > 0 && ((int)(hero.invuln * 16) & 1);
    if (hd) {
        hdAddLight(hero.cx(), hero.box.y + 20, 360, 1.0f, 0.9f, 0.74f, 0.34f, 0.08f, true);
        if (hf) { hdContactShadow(hero.cx(), hero.bottom(), hero.onGround, 34, cx, cy); hdSpriteShadow(*hf, hero.cx() - cx, hero.bottom() - cy, hero.left); }
    }
    if (hf && !blink && hero.state != HS_WIN) drawFrame(*hf, hero.cx() - cx, hero.bottom() - cy, hero.left);
    plat::setDrawAdditive(true);   // el destello (coinEffect.png) tiene fondo negro: se dibuja sumado
    if (powerT > 0) if (const Frame* pf = powerAnim.current()) drawFrame(*pf, hero.cx() - cx, hero.box.y + hero.box.h * 0.5f - cy, false, (Uint8)(powerT < 2 ? 120 : 200));

    for (auto& ef : effects) if (ef.additive) if (const Frame* f = ef.anim.current()) drawFrame(*f, ef.x - cx, ef.y - cy);
    plat::setDrawAdditive(false);
    for (auto& ef : effects) if (!ef.additive) if (const Frame* f = ef.anim.current()) drawFrame(*f, ef.x - cx, ef.y - cy);
    if (hd) hdLighting(cx, cy);
}

void World::drawHud() {
    // El HUD va en el ancho actual (pantalla completa sin deformar, o la pantalla 4:3 del original).
    const int hudW = plat::SCREEN_W;
    // Igual que el original (800x600): sin fondo, cara y vidas, puntos, 4 vasos, 4 bowls y 4 llaves.
    vc::Font& f = vc::uiFont();
    const SDL_Color white = { 255, 255, 255, 255 };
    char b[32];
    // El HUD se midió sobre 800x600; la vista es de 900 de alto (como el original, 1200x900): se escala.
    const float u = 912.0f / 600.0f;   // el HUD va en unidades del mundo, como en el original
    {
        SDL_Renderer* r = plat::renderer();
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 128);
        const SDL_FRect hb = { 0, 0, (float)hudW, 92.0f };
        SDL_RenderFillRectF(r, &hb);
    }
    // Posiciones y tamaños medidos sobre el video del juego original (pantalla de 800x600).
    if (const ImageMap* h = imageMap("HUD_HERO", "AssetsGeneral")) drawFrameScaled(h->frames[0], 52 * u, 28 * u, 0.6f * u);
    snprintf(b, sizeof b, "%d", cnt.lives); f.draw(b, 70 * u, 34 * u, 0.5f * u, white);
    f.drawCentered("Puntos", 180 * u, 8 * u, 0.42f * u, white);
    snprintf(b, sizeof b, "%06d", cnt.points); f.drawCentered(b, 180 * u, 26 * u, 0.6f * u, white);
    if (const ImageMap* g = imageMap("HUD_MILK_GLASSES", "AssetsGeneral"); g && g->frames.size() >= 2)
        for (int i = 0; i < HUD_SLOTS; i++) drawFrameScaled(g->frames[i < cnt.vascolets ? 0 : 1], (288 + i * 33.0f) * u, 32 * u, 0.65f * u);
    if (const ImageMap* c = imageMap("HUD_CEREALS", "AssetsGeneral"); c && c->frames.size() >= 2)
        for (int i = 0; i < HUD_SLOTS; i++) drawFrameScaled(c->frames[i < cnt.cereals ? 0 : 1], (430 + i * 48.0f) * u, 32 * u, 0.68f * u);
    if (const ImageMap* k = imageMap("HUD_KEYS", "AssetsGeneral"); k && k->frames.size() >= 4)
        for (int i = 0; i < 4; i++) {
            const float x = (float)hudW - (152 - i * 39.0f) * u;
            drawFrameScaled(k->frames[i], x, 28 * u, 0.62f * u);
            snprintf(b, sizeof b, "%d", cnt.keys[i]); f.draw(b, x + 8 * u, 36 * u, 0.42f * u, white);
        }
#ifdef HOST
    if (getenv("V1_DEBUG")) {   // diagnóstico (solo PC)
        char d[160];
        snprintf(d, sizeof d, "st %d x %.0f bot %.0f scr %d,%d met %d/%d t %.1f", (int)hero.state, hero.box.x, hero.bottom(), screenX, screenY, (int)meteors.size(), (int)meteorScreens.size(), meteorT);
        f.draw(d, 10, (float)LOGICAL_H - 40, 0.45f, white);
    }
#endif
    // Franjas negras con texto blanco centrado, medidas en el video (vista de 900 de alto):
    // la de los carteles empieza en 152 y la de "Juego Salvado" en 257; cada renglón mide 30.
    const float v = 1.0f, lh = 30 * v, sc = lh / f.height(1.0f);
    auto band = [&](const std::string& msg, float top) {
        std::vector<std::string> lines; std::string cur;
        for (char c : msg) { if (c == '\n') { lines.push_back(cur); cur.clear(); } else cur += c; }
        lines.push_back(cur);
        SDL_Renderer* r = plat::renderer();
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        const SDL_FRect rc = { 0, top, (float)hudW, lines.size() * lh + 16 * v };
        SDL_RenderFillRectF(r, &rc);
        for (size_t i = 0; i < lines.size(); i++) f.drawCentered(lines[i], hudW * 0.5f, top + 8 * v + i * lh, sc, white);
    };
    if (messageT > 0 && !message.empty()) band(message, 152 * v);
    if (savedT > 0) band("Juego Salvado", 257 * v);
}

}
