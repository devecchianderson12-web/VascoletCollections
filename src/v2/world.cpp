#include "world.h"
#include "../launcher/settings.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace v2 {

// ---------------------------------------------------------------------------------------------
// Descriptores (0x465a10): gravedad, vida y caja de cada personaje.
// ---------------------------------------------------------------------------------------------
static const CharDef DEFS[] = {
    { "Vascolet", "vascolet", "stand", 900, 5, 6, 4, 60 },
    { "VascoletAndWilly", "vascolet", "flyingWithWilly", 450, 5, 6, 4, 60 },
    { "Ogre", "ogro", "statue", 900, 5, 32, 20, 100 },
    { "Iguana", "iguana", "original", 0, 5, 32, 20, 100 },
    { "Dragon", "dragon", "original", 0, 70, 64, 63, 100 },
    { "Golem", "golem", "original", 0, 5, 24, 23, 75 },
    { "Rocks", "rocas", "bigRock", 0, 3, 12, 11, 75 },
    { "Malmark", "malmark", "normal", 0, 5, 16, 15, 120 },
    { "MalmarkFireball", "fireball2", "normal", 0, 999999, 24, 16, 16 },
    { "Scorpion", "escorpion", "walk", 900, 1, 32, 20, 20 },
    { "FeetBall", "feetball", "normal", 900, 1, 16, 15, 48 },
    { "Hedgehog", "puercoespin", "normal", 900, 1, 20, 18, 32 },
    { "Bird", "pajaro", "flying", 0, 1, 32, 20, 32 },
    { "Boomerang", "boomerang", "fast", 0, 1, 14, 13, 16 },
    { "Effect", "particles", "getPowerUp", 0, 1, 16, 16, 32 },
    { "MalmarkFx", "malmark", "normal", 0, 1, 16, 15, 120 },
    { "Fireball", "fireball", "normal", 0, 1, 16, 16, 32 },
    { "Spyke", "bola-de-pinchos", "normal", 0, 1, 20, 19, 40 },
    { "Witch", "bruja", "normal", 0, 1, 30, 20, 58 },
    { "Piranha", "pirana", "normal", 0, 1, 23, 20, 32 },
    { "Anguila", "anguila", "normal", 0, 1, 34, 32, 28 },
    { "PterodactilRed", "pterodactilo", "flying", 0, 1, 30, 20, 40 },
    { "PterodactilGreen", "pterodactilo-verde", "flying", 0, 1, 30, 20, 40 },
    { "UglyDragon", "uglyDragon", "flying", 0, 1, 30, 20, 40 },
    { "Blowfish", "blowfish", "normal", 0, 1, 20, 18, 36 },
    { "Fish", "fish", "normal", 0, 1, 20, 18, 24 },
    { "Fenix", "fenix", "flying", 0, 1, 24, 20, 40 },
    { "Ghost", "ghost", "normal", 0, 1, 24, 20, 48 },
    { "Bat", "bat", "normal", 0, 1, 20, 18, 32 },
    { "Axe", "axe", "normal", 0, 1, 16, 14, 32 },
    { "Fireball2", "fireball2", "normal", 0, 1, 16, 14, 32 },
    { "Head", "head", "Stand", 0, 1, 24, 20, 48 },
    { "Body", "body", "Stand", 0, 1, 24, 20, 64 },
    // Ítems y objetos (sin vida): caja de 12x24 alrededor del dibujo.
    { "Coin", "moneda", "normal", 0, 1, 12, 12, 28 },
    { "Cereals", "spring", "normal", 0, 1, 14, 14, 34 },
    { "ItemVascolet", "sneaker", "normal", 0, 1, 14, 14, 34 },
    { "RedKey", "llave-roja", "normal", 0, 1, 14, 14, 40 },
    { "GreenKey", "llave-verde", "normal", 0, 1, 14, 14, 40 },
    { "BlueKey", "llave-azul", "normal", 0, 1, 14, 14, 40 },
    { "SkullKey", "skullkey", "normal", 600, 1, 14, 14, 34 },
    { "Energy", "vaso", "normal", 0, 1, 14, 14, 30 },
    { "Energy2", "itemCereals", "normal", 0, 1, 16, 14, 30 },
    { "ExtraLive", "vida-extra", "normal", 0, 1, 18, 16, 36 },
    { "RedDoor", "puerta-roja", "normal", 0, 1, 16, 16, 128 },
    { "GreenDoor", "puerta-verde", "normal", 0, 1, 16, 16, 128 },
    { "BlueDoor", "puerta-azul", "normal", 0, 1, 16, 16, 128 },
    { "SkullDoor", "puerta-calavera", "normal", 0, 1, 16, 16, 128 },
    { "Platform", "plataforma", "normal", 0, 1, 46, 46, 32 },
    { "Info", "carteles-ayuda-guardar", "info", 0, 1, 16, 16, 60 },
    { "Save", "carteles-ayuda-guardar", "saveStale", 0, 1, 16, 16, 60 },
    { "Portal", "portal", "stand", 0, 1, 24, 24, 96 },
};

const CharDef* charDef(const std::string& name) {
    for (auto& d : DEFS) if (name == d.name) return &d;
    return nullptr;
}

bool Actor::has(const char* c) const {
    const size_t n = strlen(c);
    for (size_t i = 0; (i = cmd.find(c, i)) != std::string::npos; i += n) {
        const bool a = i == 0 || cmd[i - 1] == ',', b = i + n == cmd.size() || cmd[i + n] == ',';
        if (a && b) return true;
    }
    return false;
}

namespace {

enum PState {
    P_STAND, P_WALK, P_PREP_RUN, P_RUN, P_BRAKE, P_FALL, P_RUNFALL, P_JUMP, P_RUNJUMP, P_SECOND, P_RUNSECOND,
    P_LAND, P_ATTACK, P_DAMAGE, P_DAMAGE_FALL, P_DAMAGE_RAISE, P_LOSING, P_SWIM, P_FLY, P_FLY_DAMAGE, P_GONE
};

// Botones del original: flechas, salto (Z/S) y ataque (A/X).
int dirHeld() { return (plat::held(plat::BTN_RIGHT) ? 1 : 0) - (plat::held(plat::BTN_LEFT) ? 1 : 0); }
bool jumpPressed() { return plat::pressed(plat::BTN_JUMP); }
bool jumpHeld() { return plat::held(plat::BTN_JUMP); }
bool attackPressed() { return plat::pressed(plat::BTN_SHOOT); }

bool overlap(const SDL_FRect& a, const SDL_FRect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

const char* KEY_ITEM[4] = { "RedKey", "GreenKey", "BlueKey", "SkullKey" };
const char* DOOR_ITEM[4] = { "RedDoor", "GreenDoor", "BlueDoor", "SkullDoor" };

bool isEnemy(const std::string& t) {
    static const char* const E[] = { "Scorpion", "FeetBall", "Hedgehog", "Bird", "Spyke", "Witch", "Piranha", "Anguila",
        "PterodactilRed", "PterodactilGreen", "UglyDragon", "Blowfish", "Fish", "Fenix", "Ghost", "Bat", "Axe",
        "Fireball2", "Head", "Body", "Ogre", "Iguana", "Dragon", "Golem", "Rocks", "Malmark", "MalmarkFireball" };
    for (auto e : E) if (t == e) return true;
    return false;
}
bool isBoss(const std::string& t) { return t == "Ogre" || t == "Iguana" || t == "Dragon" || t == "Golem" || t == "Malmark"; }
bool isItem(const std::string& t) {
    static const char* const I[] = { "Coin", "Cereals", "ItemVascolet", "RedKey", "GreenKey", "BlueKey", "SkullKey", "Energy", "Energy2", "ExtraLive" };
    for (auto e : I) if (t == e) return true;
    return false;
}

const char* const LEVEL_FILE[6] = { "", "level1", "level2", "level3", "level4", "level5" };

} // namespace

// ---------------------------------------------------------------------------------------------
// Carga
// ---------------------------------------------------------------------------------------------
Actor* World::spawn(const std::string& type, float x, float y, const std::string& cmd, const Props* props) {
    auto a = std::make_unique<Actor>();
    a->type = type;
    a->def = charDef(type);
    a->cmd = cmd;
    if (props) a->props = *props;
    a->x = a->x0 = a->px = x; a->y = a->y0 = a->py = y;
    if (a->def) {
        a->gravity = a->def->gravity;
        a->life = a->def->life;
        a->anim.set = animSet(a->def->folder);
        a->anim.play(a->def->anim);
    }
    a->left = a->has("left") && !a->has("right");
    actors.push_back(std::move(a));
    return actors.back().get();
}

bool World::load(int level, const Progress& p) {
    lvl = std::max(1, std::min(5, level));
    prog = p;
    prog.energy = 5;
    if (!map.load(std::string("v2/assets/data/maps/") + LEVEL_FILE[lvl] + ".tmx")) return false;
    const std::string bg = map.prop("background"), bgxo = map.prop("backgroundXO");
    if (!bg.empty()) back.load("v2/assets/data/maps/backgrounds/" + bg + ".tmx");
    if (!bgxo.empty()) backXO.load("v2/assets/data/maps/backgrounds/" + bgxo + ".tmx");

    attackEnabled = lvl >= 2;
    // Objetos del mapa: el rectángulo de Tiled tiene arriba a la izquierda (x, y); los personajes se
    // paran en el borde de abajo, al centro.
    auto add = [&](const MapObject& o) {
        std::string name = o.name;
        if (!name.empty() && name[0] == '*') name = name.substr(1);
        const float w = o.w > 0 ? o.w : 32, h = o.h > 0 ? o.h : 32;
        if (name == "Coin") {
            // Un rectángulo de monedas: una por cada celda de 32x32 (así suman 300/900/400/600/900).
            for (int cy = 0; cy < std::max(1, (int)(h / 32)); cy++)
                for (int cx = 0; cx < std::max(1, (int)(w / 32)); cx++) { spawn("Coin", o.x + cx * 32 + 16, o.y + cy * 32 + 32); coinTotal++; }
            return;
        }
        if (name == "Start") {
            player = spawn(map.prop("player", "Vascolet") == "VascoletAndWilly" ? "VascoletAndWilly" : "Vascolet", o.x + w * 0.5f, o.y + h);
            return;
        }
        if (name == "SecretZone") { secrets.push_back({ o.x, o.y, w, h, 1 }); return; }
        if (name.rfind("Portal", 0) == 0) { spawn("Portal", o.x + w * 0.5f, o.y + h, "", &o.props); return; }
        if (name == "Circle") {   // plataformas girando en círculo
            const int n = std::max(1, atoi(o.prop("count", "1").c_str()));
            for (int i = 0; i < n; i++) {
                Actor* pl = spawn(map.prop("platforms", "Platform") == "Platform" ? "Platform" : "Platform", o.x + w * 0.5f, o.y + h * 0.5f);
                pl->a = o.x + w * 0.5f; pl->b = o.y + h * 0.5f; pl->c = (float)i / n * 6.2831853f;
                pl->props["rx"] = std::to_string(w * 0.5f); pl->props["ry"] = std::to_string(h * 0.5f);
                pl->phase = 1;
            }
            return;
        }
        if (!charDef(name)) return;
        if (isBoss(name)) {
            Actor* b = spawn(name, o.x + w * 0.5f, o.y + h, o.type, &o.props);
            b->left = true;
            return;
        }
        Actor* a = spawn(name, o.x + w * 0.5f, o.y + h, o.type, &o.props);
        if (name == "Info") a->props["info"] = o.prop("info");
    };
    for (const char* g : { "Platforms", "Items", "General", "Enemies" })
        for (const MapObject& o : map.group(g)) add(o);
    if (!player) player = spawn("Vascolet", 192, 96);
    // Que el jugador se dibuje encima de todo: va al final.
    for (size_t i = 0; i + 1 < actors.size(); i++)
        if (actors[i].get() == player) { std::swap(actors[i], actors.back()); break; }
    camLines = map.group("Camera");
    // Nivel 3 (volando con Willy): la cámara recorre sola los puntos numerados del grupo "Camera".
    if (player->type == "VascoletAndWilly") {
        std::vector<std::pair<int, SDL_FPoint>> pts;
        for (const MapObject& o : camLines) if (!o.name.empty() && isdigit((unsigned char)o.name[0])) pts.push_back({ atoi(o.name.c_str()), { o.x, o.y } });
        std::stable_sort(pts.begin(), pts.end(), [](auto& a, auto& b) { return a.first < b.first; });
        for (auto& q : pts) camPath.push_back(q.second);
        camLines.clear();
        autoCam = camPath.size() >= 2;
    }
    for (const MapObject& o : map.group("dataExt")) if (o.name == "water") waterRects.push_back({ o.x, o.y, o.w, o.h });
#ifdef HOST
    if (const char* pos = getenv("V2_POS")) { float x, y; if (sscanf(pos, "%f,%f", &x, &y) == 2) { player->x = player->px = x; player->y = player->py = y; } }
    if (getenv("V2_ITEMS")) { runEnabled = doubleJumpEnabled = true; keys[0] = keys[1] = keys[2] = keys[3] = 3; }
#endif
    saveX = player->x; saveY = player->y; saveCamD = 0;
    playerState(player->type == "VascoletAndWilly" ? P_FLY : P_STAND);
    music = map.prop("music,normal", "level1.ogg");
    v2::music(music);
    updateCamera(true);
    return true;
}

// ---------------------------------------------------------------------------------------------
// Mapa y colisiones
// ---------------------------------------------------------------------------------------------
uint8_t World::tileFlags(float x, float y) const {
    const int tx = (int)std::floor(x / 32), ty = (int)std::floor(y / 32);
    if (tx < 0 || tx >= map.w) return T_SOLID;
    if (ty < 0 || ty >= map.h) return 0;
    const Layer* l = map.layer("Tiles");
    return l ? map.flagsOfGid(l->at(tx, ty)) : 0;
}

bool World::solidAt(float x, float y, bool forFloor, float prevBottom) const {
    const uint8_t f = tileFlags(x, y);
    if (f & T_SOLID) return true;
    if (forFloor && (f & T_UP)) {
        const float top = std::floor(y / 32) * 32;
        return prevBottom <= top + 0.5f;
    }
    return false;
}

bool World::inWater(const Actor& a) const {
    const float cy = a.y - a.h() * 0.5f;
    if (tileFlags(a.x, cy) & T_WATER) return true;
    for (auto& r : waterRects) if (a.x >= r.x && a.x < r.x + r.w && cy >= r.y && cy < r.y + r.h) return true;
    return false;
}

bool World::cliffAhead(const Actor& a) const {
    const float fx = a.x + (a.left ? -a.fw() - 2 : a.fw() + 2);
    return !solidAt(fx, a.y + 4, true, a.y);
}

// Física del motor: fuerza/fricción horizontal con tope de velocidad, gravedad y choque contra tiles
// (medio ancho contra paredes y medio ancho contra el piso del descriptor).
void World::physics(Actor& a, float dt) {
    a.px = a.x; a.py = a.y;
    a.hitWall = a.hitFloor = a.hitCeil = false;
    if (a.ax != 0) a.vx += a.ax * dt;
    else if (a.friction > 0) {
        const float d = a.friction * dt;
        a.vx = std::fabs(a.vx) <= d ? 0 : a.vx - d * (a.vx > 0 ? 1 : -1);
    }
    a.vx = std::max(-a.maxVx, std::min(a.maxVx, a.vx));
    a.ax = 0;
    a.vy = std::min(a.maxFall, a.vy + a.gravity * dt);
    if (!a.solidCollide) { a.x += a.vx * dt; a.y += a.vy * dt; a.onFloor = false; return; }

    const float hw = a.w(), fw = a.fw(), h = a.h();
    // Horizontal
    float nx = a.x + a.vx * dt;
    if (a.vx != 0) {
        const float side = nx + (a.vx > 0 ? hw : -hw);
        bool hit = false;
        for (float yy = a.y - h + 2; yy < a.y - 1; yy += 16) if (solidAt(side, yy)) { hit = true; break; }
        if (!hit && solidAt(side, a.y - 2)) hit = true;
        if (hit) {
            const float tx = std::floor(side / 32) * 32;
            nx = a.vx > 0 ? tx - hw - 0.01f : tx + 32 + hw + 0.01f;
            a.hitWall = true;
        }
    }
    a.x = nx;
    // Vertical
    const float prevBottom = a.y;
    float ny = a.y + a.vy * dt;
    a.onFloor = false;
    if (a.vy >= 0) {
        bool hit = false;
        for (float xx : { a.x - fw, a.x, a.x + fw }) if (solidAt(xx, ny, true, prevBottom)) { hit = true; break; }
        if (hit) { ny = std::floor(ny / 32) * 32; if (a.vy > 0) a.hitFloor = true; a.vy = 0; a.onFloor = true; }
    } else {
        bool hit = false;
        for (float xx : { a.x - fw, a.x, a.x + fw }) if (solidAt(xx, ny - h)) { hit = true; break; }
        if (hit) { ny = (std::floor((ny - h) / 32) + 1) * 32 + h; a.vy = 0; a.hitCeil = true; }
    }
    a.y = ny;
    if (!a.onFloor && a.vy >= 0)
        for (float xx : { a.x - fw, a.x, a.x + fw }) if (solidAt(xx, a.y + 1, true, a.y)) { a.onFloor = true; break; }
    // Plataformas móviles: se apoya si cae sobre su borde de arriba.
    if (!a.onFloor && a.vy >= 0 && &a == player)
        for (auto& p : actors) {
            if (p->type != "Platform" || p->removed) continue;
            const float top = p->y - 28;
            if (a.x + fw > p->x - p->w() && a.x - fw < p->x + p->w() && prevBottom <= top + 2 + std::max(0.0f, p->y - p->py) && a.y >= top) {
                a.y = top; a.vy = 0; a.onFloor = true; a.hitFloor = true;
                a.x += p->x - p->px;
                break;
            }
        }
}

int World::enemyFire() const {
    int n = 0;
    for (auto& a : actors) if (a->type == "Fireball" && !a->removed && a->props.count("enemy")) n++;
    return n;
}

int World::countOf(const std::string& type) const {
    int n = 0;
    for (auto& a : actors) if (a->type == type && !a->removed) n++;
    return n;
}

Actor* World::find(const std::string& type) {
    for (auto& a : actors) if (a->type == type && !a->removed) return a.get();
    return nullptr;
}

void World::showMsg(const std::string& key, bool sign) {
    msg = text(key);
    msgT = 1.2f;
    msgLinger = sign;
}

// ---------------------------------------------------------------------------------------------
// Jugador (VASCOLET_*, reglas de 0x42d170)
// ---------------------------------------------------------------------------------------------
void World::playerState(int s) {
    Actor& p = *player;
    p.state = s; p.stateT = 0;
    switch (s) {
        case P_STAND: p.friction = 400; p.maxVx = 200; p.anim.play("stand"); break;
        case P_WALK: p.friction = 400; p.maxVx = 200; p.anim.play("walk"); break;
        case P_PREP_RUN: p.friction = 400; p.maxVx = 100; p.anim.play("stand"); break;
        case P_RUN: p.friction = 400; p.maxVx = 360; p.anim.play("run"); sfx("fall1"); break;
        case P_BRAKE: p.friction = 800; p.maxVx = 360; p.anim.play("runBreak"); break;
        case P_FALL: p.friction = 0; p.maxVx = 200; p.anim.play("fall"); break;
        case P_RUNFALL: p.friction = 0; p.maxVx = 360; p.anim.play("fall"); break;
        case P_JUMP: p.friction = 0; p.maxVx = 200; p.anim.play("standJump", true); canDoubleJump = true; p.vy = -500; break;
        case P_RUNJUMP: p.friction = 0; p.maxVx = 360; p.anim.play("runJump", true); canDoubleJump = true; p.vy = -500; break;
        case P_SECOND: case P_RUNSECOND:
            p.friction = 0; p.maxVx = s == P_SECOND ? 200 : 360; p.anim.play("runJumpMid", true);
            sfx("double_jump"); p.vy = -300; canDoubleJump = false; break;
        case P_LAND: p.friction = 800; p.maxVx = 100; p.anim.play("fallGround", true); sfx("fall1"); break;
        case P_ATTACK: p.friction = 100; p.maxVx = 200; p.anim.play("standAttack", true); p.phase = 0; break;
        case P_DAMAGE:
            p.friction = 0; p.maxVx = 240;
            p.vy = -240; p.vx = p.left ? 240 : -240;
            prog.energy = std::max(0, prog.energy - 1);
            sfx("v_death1");
            invT = 2.0f;
            p.anim.play(inWater(p) ? "damageUnderWater" : "damage", true);
            break;
        case P_DAMAGE_FALL: p.friction = 800; p.maxVx = 200; p.vx = 0; p.anim.play("damageFall", true); sfx("fall1"); break;
        case P_DAMAGE_RAISE: p.friction = 800; p.anim.play("damageRaise", true); break;
        case P_LOSING:
            p.friction = 0; p.vy = -480; p.vx = p.left ? 60 : -60; p.gravity = 1800; p.solidCollide = false;
            p.anim.play("damage", true);
            break;
        case P_SWIM: p.friction = 200; p.maxVx = 200; p.anim.play("swimming"); break;
        case P_FLY: p.friction = 200; p.maxVx = 360; p.maxFall = 300; p.anim.play("flyingWithWilly"); break;
        case P_FLY_DAMAGE:
            p.vx = p.left ? 60.0f : -60.0f; p.vy = -120;
            prog.energy = std::max(0, prog.energy - 1);
            invT = 1.5f; sfx("v_death1");
            p.anim.play("flyingWithWillyDamage", true);
            break;
        default: break;
    }
}

void World::hurtPlayer(bool fromTile) {
    (void)fromTile;
    Actor& p = *player;
    if (invT > 0 || p.state == P_DAMAGE || p.state == P_LOSING || p.state == P_GONE || winT >= 0) return;
    if (vc::settings().ghost) return;   // modo fantasma: nada le hace daño
    playerState(P_DAMAGE);
}

void World::updatePlayer(float dt) {
    Actor& p = *player;
    p.stateT += dt;
    if (invT > 0) invT -= dt;
    const int dir = dirHeld();
    const bool dirPressed = plat::pressed(plat::BTN_LEFT) || plat::pressed(plat::BTN_RIGHT);
    const int pdir = plat::pressed(plat::BTN_RIGHT) ? 1 : plat::pressed(plat::BTN_LEFT) ? -1 : 0;
    auto face = [&] { if (dir) p.left = dir < 0; };
    auto force = [&](float f) { if (dir) { p.left = dir < 0; p.ax = (p.left ? -f : f); } };
    auto cutJump = [&] { if (!jumpHeld() && p.vy < -240) p.vy = -240; };
    auto airControl = [&](float f) { force(f); };
    const bool water = inWater(p) && p.state != P_LOSING && p.state != P_GONE;
    if (doubleTapT > 0) doubleTapT -= dt;

    // Reglas persistentes (VASCOLET_STATIC_STATE)
    const bool canAct = p.state != P_DAMAGE && p.state != P_DAMAGE_FALL && p.state != P_DAMAGE_RAISE && p.state != P_LOSING && p.state != P_GONE && winT < 0;
    if (canAct && p.state != P_SWIM && p.state != P_FLY) {
        if (attackPressed() && attackEnabled && p.state != P_ATTACK && countOf("Boomerang") < 2) playerState(P_ATTACK);
        if (water) { playerState(P_SWIM); p.vy *= 0.5f; }
        if (!water && (tileFlags(p.x, p.y - 4) & T_DAMAGE || tileFlags(p.x - p.fw(), p.y - 4) & T_DAMAGE || tileFlags(p.x + p.fw(), p.y - 4) & T_DAMAGE ||
                       tileFlags(p.x, p.y + 1) & T_DAMAGE)) hurtPlayer(true);
    }

    if (p.state == P_FLY || p.state == P_FLY_DAMAGE) {
        // VASCOLET_FLYING_WITH_WILLY: flechas para avanzar, salto para aletear (-250), ataque = fuego de Willy.
        if (p.state == P_FLY) {
            force(800);
            if (jumpPressed()) { p.vy = -250; sfx("wings"); }
            if (attackPressed() && countOf("Fireball") - enemyFire() < 3) {
                p.anim.play("flyingWithWillyAttack", true); p.a = 0.25f;
                Actor* f = spawn("Fireball", p.x + (p.left ? -16.0f : 16.0f), p.y - 30); f->left = p.left;
                sfx("willyFire");
            }
            if (p.a > 0) { p.a -= dt; if (p.a <= 0) p.anim.play("flyingWithWilly"); }
            const bool dmgTile = (tileFlags(p.x, p.y - 4) & T_DAMAGE) || (tileFlags(p.x, p.y - p.h() + 4) & T_DAMAGE);
            if (dmgTile && invT <= 0 && !vc::settings().ghost) playerState(P_FLY_DAMAGE);
        } else if (p.stateT > 0.6f) {
            if (prog.energy <= 0) { p.state = P_LOSING; p.stateT = 0; p.gravity = 900; p.solidCollide = false; sfx("vasco_death"); plat::stopMusic(); }
            else playerState(P_FLY);
        }
        if (p.state == P_FLY || p.state == P_FLY_DAMAGE) { p.gravity = p.def->gravity; physics(p, dt); }
        // La cámara automática empuja al jugador; si queda aplastado contra una pared, pierde.
        if (autoCam && p.state != P_LOSING) {
            const float minX = camX + p.w(), maxX = camX + VIEW_W - p.w();
            if (p.x < minX) { p.x = minX; if (solidAt(p.x + p.w() + 1, p.y - p.h() * 0.5f) && !vc::settings().ghost) { prog.energy = 0; playerState(P_FLY_DAMAGE); p.state = P_FLY_DAMAGE; p.stateT = 0.6f; } }
            if (p.x > maxX) p.x = maxX;
            if (p.y - p.h() < camY) { p.y = camY + p.h(); if (p.vy < 0) p.vy = 0; }
            if (p.y > camY + VIEW_H) { p.y = camY + VIEW_H; p.vy = std::min(p.vy, 0.0f); }
        }
        if (p.state == P_LOSING) hasLosing = true;
        p.anim.update(dt);
        return;
    }
    switch (p.state) {
        case P_STAND:
            face();
            if (dir) playerState(P_WALK);
            else if (jumpPressed() && p.onFloor) { sfx("jumpAir"); playerState(P_JUMP); }
            else if (!p.onFloor) playerState(P_FALL);
            break;
        case P_WALK:
            force(800);
            if (jumpPressed() && p.onFloor) { sfx("jumpAir"); playerState(P_JUMP); break; }
            if (!p.onFloor) { canDoubleJump = true; playerState(P_FALL); break; }
            if (!dir) { if (runEnabled) playerState(P_PREP_RUN); else { playerState(P_STAND); p.maxVx = 60; } }
            if (p.anim.flagFirst("PlaySound")) sfx(rand() & 1 ? "footStep1" : "footStep2");
            break;
        case P_PREP_RUN:
            // Doble toque de flecha: correr (con la zapatilla).
            if (pdir && runEnabled) { p.left = pdir < 0; playerState(P_RUN); break; }
            if (p.stateT > 0.25f || dir) { playerState(dir ? P_WALK : P_STAND); break; }
            if (jumpPressed() && p.onFloor) { sfx("jumpAir"); playerState(P_JUMP); }
            break;
        case P_RUN:
            force(1200);
            if (jumpPressed() && p.onFloor) { sfx("jumpAir"); playerState(P_RUNJUMP); break; }
            if (!p.onFloor) { canDoubleJump = true; playerState(P_RUNFALL); break; }
            if (!dir || (dir < 0) != p.left) playerState(p.stateT > 0.5f ? P_BRAKE : P_PREP_RUN);
            if (p.anim.flagFirst("PlaySound")) sfx(rand() & 1 ? "footStep1" : "footStep2");
            break;
        case P_BRAKE:
            if (p.anim.ended) { playerState(P_STAND); break; }
            if (p.stateT > 0.25f && dirPressed) { p.left = pdir < 0; playerState(P_WALK); break; }
            if (p.stateT <= 0.25f && dirPressed && runEnabled) { p.left = pdir < 0; playerState(P_RUN); break; }
            if (jumpPressed() && p.onFloor) { sfx("jumpAir"); playerState(P_JUMP); }
            if (!p.onFloor) playerState(P_FALL);
            break;
        case P_FALL: case P_RUNFALL: case P_JUMP: case P_RUNJUMP: case P_SECOND: case P_RUNSECOND: {
            const bool run = p.state == P_RUNFALL || p.state == P_RUNJUMP || p.state == P_RUNSECOND;
            airControl(run && p.state != P_RUNSECOND ? 1200 : 800);
            if (p.state == P_JUMP && p.vy > -120 && p.anim.name == "standJump") p.anim.play("standJumpMid");
            if (p.state == P_RUNJUMP && p.vy > -120 && p.anim.name == "runJump") p.anim.play("runJumpMid");
            if ((p.state == P_FALL || p.state == P_RUNFALL || p.state == P_JUMP || p.state == P_RUNJUMP) && jumpPressed() && canDoubleJump && doubleJumpEnabled) {
                playerState(run ? P_RUNSECOND : P_SECOND);
                break;
            }
            if (p.state != P_SECOND && p.state != P_RUNSECOND) cutJump(); else cutJump();
            if (p.onFloor && p.vy >= 0 && p.stateT > 0.02f) {
                if (run && dir && runEnabled && (dir < 0) == p.left) playerState(P_RUN);
                else playerState(P_LAND);
            }
            break;
        }
        case P_LAND:
            if (dirPressed) { p.left = pdir < 0; playerState(P_WALK); break; }
            if (dir) p.ax = (dir < 0) == p.left ? (p.left ? -800.0f : 800.0f) : (p.left ? 400.0f : -400.0f);
            if (jumpPressed() && p.onFloor) { sfx("jumpAir"); playerState(P_JUMP); break; }
            if (p.anim.ended) playerState(P_STAND);
            if (!p.onFloor) playerState(P_FALL);
            break;
        case P_ATTACK:
            if (p.anim.flag("Custom") && p.phase == 0) {
                p.phase = 1;
                Actor* b = spawn("Boomerang", p.x + (p.left ? -24.0f : 24.0f), p.y - 30, "");
                b->left = p.left;
                b->vx = p.left ? -320.0f : 320.0f;
                sfx("boomerang");
            }
            if (dir) face();
            if (p.onFloor && jumpPressed()) { canDoubleJump = true; p.vy = -500; sfx("jumpAir"); }
            else if (!p.onFloor && jumpPressed() && canDoubleJump && doubleJumpEnabled) { canDoubleJump = false; p.vy = -300; sfx("double_jump"); }
            cutJump();
            if (p.anim.ended) playerState(p.onFloor ? P_STAND : P_FALL);
            break;
        case P_DAMAGE:
            if (p.stateT > 0.5f && water) { playerState(P_SWIM); break; }
            if (prog.energy <= 0 && !hasLosing) {
                hasLosing = true;
                playerState(P_LOSING);
                sfx("vasco_death");
                plat::stopMusic();
                break;
            }
            if (p.onFloor && p.stateT > 0.05f && !water) playerState(P_DAMAGE_FALL);
            break;
        case P_DAMAGE_FALL:
            if (p.stateT > 0.4f) playerState(P_DAMAGE_RAISE);
            break;
        case P_DAMAGE_RAISE:
            if (p.anim.ended) playerState(P_STAND);
            break;
        case P_LOSING:
            if (p.y > camY + VIEW_H + 64 && p.stateT > 0.5f) { p.state = P_GONE; p.stateT = 0; }
            break;
        case P_GONE:
            if (p.stateT > 1.0f) {
                // Pierde una vida y vuelve al último punto de guardado.
                prog.lives--;
                if (prog.lives <= 0) { overT = 0; p.state = P_GONE; p.stateT = -999; plat::stopMusic(); v2::music("gameover.ogg", false); break; }
                hasLosing = false;
                prog.energy = 5;
                p.x = p.px = saveX; p.y = p.py = saveY; p.vx = p.vy = 0;
                p.gravity = p.def->gravity; p.solidCollide = true;
                invT = 2.0f;
                camD = saveCamD;
                playerState(p.type == "VascoletAndWilly" ? P_FLY : P_STAND);
                v2::music(music);
                updateCamera(true);
            }
            break;
        case P_SWIM: {
            // Nado (VASCOLET_SWIMMING): todo a media velocidad, cada salto impulsa hacia arriba.
            p.gravity = p.def->gravity * 0.25f;
            p.maxFall = 120;
            force(800);
            if (jumpPressed()) {
                p.vy = p.b > 0 ? -375.0f * 0.5f : -250.0f * 0.5f;
                p.b = 0.4f;
                p.anim.play("swimming", true);
                sfx("swim");
            }
            if (p.b > 0) p.b -= dt;
            if (attackPressed() && attackEnabled && countOf("Boomerang") < 2) {
                Actor* b = spawn("Boomerang", p.x + (p.left ? -24.0f : 24.0f), p.y - 30, "");
                b->left = p.left; b->vx = p.left ? -320.0f : 320.0f;
                sfx("boomerang");
            }
            if (!water) {
                p.gravity = p.def->gravity; p.maxFall = 900;
                if (p.vy < -60) { sfx("jumpWater"); playerState(P_JUMP); p.vy = -500; }
                else if (p.onFloor) playerState(P_STAND);
                else playerState(P_FALL);
            }
            if ((tileFlags(p.x, p.y - 4) & T_DAMAGE) || (tileFlags(p.x, p.y - p.h() + 4) & T_DAMAGE)) hurtPlayer(true);
            break;
        }
        default: break;
    }
    if (p.state == P_DAMAGE && water) { p.gravity = p.def->gravity * 0.25f; p.maxFall = 120; }
    else if (p.state != P_SWIM && p.state != P_LOSING && p.state != P_GONE) { p.gravity = p.def->gravity; p.maxFall = 900; }

    if (p.state != P_GONE) {
        const float scale = (p.state == P_SWIM || (water && p.state == P_DAMAGE)) ? 0.5f : 1.0f;
        if (p.state == P_SWIM) { p.ax *= 1.0f; p.maxVx = 200 * 1.0f; }
        physics(p, dt * (scale < 1 ? 1.0f : 1.0f));
        (void)scale;
    }
    // Se cae por un pozo: pierde la vida directamente.
    if (p.state != P_LOSING && p.state != P_GONE && p.y - p.h() > map.h * 32 + 16 && vc::settings().ghost) {
        // Modo fantasma: vuelve al último punto de guardado sin perder la vida.
        p.x = p.px = saveX; p.y = p.py = saveY; p.vx = p.vy = 0; camD = saveCamD;
        updateCamera(true);
    }
    if (p.state != P_LOSING && p.state != P_GONE && p.y - p.h() > map.h * 32 + 16) {
        prog.energy = 0; hasLosing = true;
        p.state = P_GONE; p.stateT = 0;
        sfx("vasco_death");
    }
    p.anim.update(dt);
}

// ---------------------------------------------------------------------------------------------
// Resto de los personajes
// ---------------------------------------------------------------------------------------------
void World::killEnemy(Actor& e, bool stomp) {
    if (e.dead) return;
    e.dead = true;
    e.state = 100; e.stateT = 0;
    e.vx = 0; e.vy = 0;
    if (e.anim.set && e.anim.set->get("damage")) e.anim.play("damage", true);
    if (stomp) { Actor* fx = spawn("Effect", e.x, e.y - e.h() * 0.5f); fx->anim.play("miniExplosion"); fx->props["fx"] = "1"; }
}

void World::updateActor(Actor& a, float dt) {
    a.stateT += dt; a.t += dt;
    const std::string& t = a.type;
    Actor& p = *player;
    if (a.flashT > 0) a.flashT -= dt;

    // Efectos: desaparecen al terminar la animación.
    if (t == "Effect") { a.px = a.x; a.py = a.y; a.anim.update(dt); if (a.anim.ended) a.removed = true; return; }

    // Enemigo vencido (ENEMY_DEAD): se sacude 0.1 s, salta y cae fuera de la pantalla.
    if (a.dead) {
        if (a.state == 100 && a.stateT > 0.1f) {
            a.state = 101; a.stateT = 0;
            if (a.anim.set && a.anim.set->get("dead")) a.anim.play("dead", true);
            a.vx = a.left ? 60.0f : -60.0f; a.vy = -120 * 2.5f; a.gravity = 560 * 1.5f; a.solidCollide = false;
        }
        if (a.state == 101) { a.px = a.x; a.py = a.y; a.vy += a.gravity * dt; a.x += a.vx * dt; a.y += a.vy * dt; }
        if (a.y - a.h() > camY + VIEW_H + 32 || a.stateT > 4) a.removed = true;
        a.anim.update(dt);
        return;
    }

    if (t == "Platform") {
        a.px = a.x; a.py = a.y;
        if (a.phase == 1) {
            a.c += dt * 0.9f;
            const float rx = (float)atof(a.props["rx"].c_str()), ry = (float)atof(a.props["ry"].c_str());
            a.x = a.a + std::cos(a.c) * rx; a.y = a.b + std::sin(a.c) * ry;
        }
        a.anim.update(dt);
        return;
    }
    if (isItem(t) || t == "Info" || t == "Save" || t == "Portal" || t.find("Door") != std::string::npos) {
        a.px = a.x; a.py = a.y;
        if (t == "SkullKey" && a.gravity > 0) physics(a, dt);
        a.anim.update(dt);
        return;
    }
    if (t == "Boomerang") {
        // BOOMERANG_FAST → BACK (0.35 s o pared) → vuelve buscando al jugador; BOUNCE / DEAD.
        a.solidCollide = true;
        switch (a.state) {
            case 0:
                a.vx = a.left ? -320.0f : 320.0f; a.vy = 0;
                physics(a, dt);
                if (a.hitWall) { a.state = 2; a.stateT = 0; a.vy = -120; a.vx = a.left ? 60.0f : -60.0f; a.gravity = 960; sfx("boomerang_wall_hit"); }
                else if (a.stateT > 0.35f) { a.state = 1; a.stateT = 0; }
                break;
            case 1: {
                // SeekObject('player', 240, 1, 960): acelera hacia el jugador.
                const float dx = p.x - a.x, dy = (p.y - 30) - a.y, d = std::max(1.0f, std::hypot(dx, dy));
                a.vx += dx / d * 960 * dt; a.vy += dy / d * 960 * dt;
                const float sp = std::hypot(a.vx, a.vy);
                if (sp > 240) { a.vx *= 240 / sp; a.vy *= 240 / sp; }
                a.solidCollide = false;
                physics(a, dt);
                if (overlap(a.box(), p.box())) { a.removed = true; break; }
                if (a.stateT > 4) a.removed = true;
                break;
            }
            case 2:
                physics(a, dt);
                if (a.onFloor) { a.state = 3; a.stateT = 0; a.vx = a.vy = 0; }
                break;
            case 3:
                a.px = a.x; a.py = a.y;
                if (a.stateT > 0.25f) a.removed = true;
                break;
        }
        if (a.x < camX - 64 || a.x > camX + VIEW_W + 64 || a.y < camY - 64 || a.y > camY + VIEW_H + 64) a.removed = true;
        a.anim.update(dt);
        return;
    }

    // Solo se mueven los enemigos cerca de la cámara (IsInActiveArea).
    const bool active = a.x > camX - 160 && a.x < camX + VIEW_W + 160 && a.y > camY - 160 && a.y < camY + VIEW_H + 220;
    if (!active && !isBoss(t) && t != "MalmarkFx" && t != "Fireball" && t != "MalmarkFireball") { a.px = a.x; a.py = a.y; return; }

    // SeekObject del motor: acelera hacia un punto hasta una velocidad máxima.
    auto seek = [&](float tx, float ty, float accel, float maxV) {
        const float dx = tx - a.x, dy = ty - a.y, d = std::max(1.0f, std::hypot(dx, dy));
        a.vx += dx / d * accel * dt; a.vy += dy / d * accel * dt;
        const float sp = std::hypot(a.vx, a.vy);
        if (sp > maxV) { a.vx *= maxV / sp; a.vy *= maxV / sp; }
    };
    auto fly = [&] { a.px = a.x; a.py = a.y; a.x += a.vx * dt; a.y += a.vy * dt; };
    auto facePlayer = [&] { a.left = p.x < a.x; };
    const float pcx = p.x, pcy = p.y - p.h() * 0.5f;          // centro del jugador
    const float dist = std::hypot(pcx - a.x, pcy - (a.y - a.h() * 0.5f));
    // Jefes: esperan quietos hasta que el jugador llega (a 192 y en pantalla); Malmark aparece, vuela
    // hasta el jefe y lo despierta (MALMARK_GOING_TO_BOSS); ahí empieza la música del jefe.
    auto bossWake = [&](const char* icon) -> bool {
        if (a.phase == 0) {
            if (std::fabs(pcx - a.x) < 192 + a.w() && a.x > camX && a.x < camX + VIEW_W && a.y > camY && a.y - a.h() < camY + VIEW_H) {
                a.phase = 1;
                Actor* m = spawn("MalmarkFx", camX + (a.x > camX + VIEW_W * 0.5f ? 40.0f : VIEW_W - 40.0f), camY + 80);
                m->props["boss"] = t;
                music = map.prop("music,boss", "boss2.ogg"); v2::music(music);
            }
            return false;
        }
        if (a.phase == 1) {
            if (countOf("MalmarkFx") > 0) return false;
            a.phase = 2; a.stateT = 0;
            a.anim.play("transformation", true);
            if (t == "Ogre") a.anim.play("statueTransform", true);
            return false;
        }
        if (a.phase == 2) {
            if (a.stateT < 1.5f) return false;
            a.phase = 3; bossBar = 1; bossIcon = icon;
            return true;
        }
        return true;
    };
    auto bossDefeated = [&] {
        bossBar = -1;
        if (winT < 0) { winT = 0; plat::stopMusic(); sfx("boss_death"); v2::music(map.prop("music,win", "win_level.ogg"), false); }
    };

    a.friction = 0;
    if (t == "MalmarkFx") {
        // Malmark vuela hasta el jefe, hace el hechizo y desaparece.
        Actor* boss = nullptr;
        for (auto& o : actors) if (o->type == a.props["boss"] && !o->removed) boss = o.get();
        a.anim.play(a.phase == 0 ? "normal" : "attack");
        if (!boss) { a.removed = true; return; }
        if (a.phase == 0) {
            seek(boss->x, boss->y - boss->h() - 20, 60 * 4, 120 * 1.5f);
            a.left = a.vx < 0;
            fly();
            if (std::hypot(boss->x - a.x, boss->y - boss->h() - 20 - a.y) < 32 || a.stateT > 6) { a.phase = 1; a.stateT = 0; a.vx = a.vy = 0; a.anim.play("attack", true); }
        } else {
            a.px = a.x; a.py = a.y;
            if (a.anim.ended || a.stateT > 2) {
                for (int i = 0; i < 3; i++) { Actor* fx = spawn("Effect", a.x + (i - 1) * 20.0f, a.y - 40 + i * 10.0f); fx->anim.play("getPowerUp"); }
                a.removed = true;
            }
        }
        a.anim.update(dt);
        return;
    }
    if (t == "Scorpion" || t == "Hedgehog") {
        // Camina a 60; da vuelta contra una pared o (con dontFall) al borde de un precipicio.
        if (t == "Hedgehog") {
            if (a.state == 0) { a.vx = 0; a.anim.play("normal"); if (a.stateT > 2.0f) { a.state = 1; a.stateT = 0; } }
            else { a.anim.play("walking"); if (a.stateT > 1.5f) { a.state = 0; a.stateT = 0; } }
            a.stompable = false;
        }
        if (t == "Scorpion" || a.state == 1) {
            if (a.onFloor && (a.hitWall || (a.has("dontFall") && cliffAhead(a)))) a.left = !a.left;
            a.vx = a.left ? -60.0f : 60.0f;
        }
        physics(a, dt);
    } else if (t == "Bird") {
        // Espera hasta que el jugador esté a 240; después baja 0.5 s y avanza subiendo.
        if (a.state == 0) {
            a.vx = a.vy = 0;
            if (dist < 240) { a.state = 1; a.stateT = 0; facePlayer(); a.anim.play("flyingDown"); }
        } else {
            if (a.stateT < 0.5f) { a.vy = std::min(240.0f, a.vy + 240 * 4 * dt); a.vx = 0; }
            else { a.anim.play("flying"); a.vx = a.left ? -240.0f : 240.0f; a.vy = std::max(-120.0f, a.vy - 480 * dt); }
        }
        fly();
    } else if (t == "Spyke") {
        // Bola de pinchos: va y viene (rebota contra los tiles) a 120.
        if (a.state == 0) {
            a.state = 1;
            if (a.has("up")) a.vy = -120;
            if (a.has("down")) a.vy = 120;
            if (a.has("left")) a.vx = -120;
            if (a.has("right")) a.vx = 120;
        }
        a.stompable = false;
        a.px = a.x; a.py = a.y;
        float nx = a.x + a.vx * dt, ny = a.y + a.vy * dt;
        if (a.vx != 0 && solidAt(nx + (a.vx > 0 ? a.w() : -a.w()), a.y - a.h() * 0.5f)) { a.vx = -a.vx; nx = a.x; }
        if (a.vy > 0 && solidAt(a.x, ny)) { a.vy = -a.vy; ny = a.y; }
        else if (a.vy < 0 && solidAt(a.x, ny - a.h())) { a.vy = -a.vy; ny = a.y; }
        a.x = nx; a.y = ny;
    } else if (t == "FeetBall") {
        // Pelota con patas: persigue al jugador (hasta 400) y salta contra las paredes.
        a.stompable = false;
        facePlayer();
        if (a.state == 0) {
            a.anim.play("normal");
            a.vx += (a.left ? -400.0f : 400.0f) * dt; a.vx = std::max(-120.0f, std::min(120.0f, a.vx));
            if (a.onFloor && (a.hitWall || pcy < a.y - 64)) { a.state = 1; a.vy = -360; a.anim.play("jump", true); }
        } else if (a.onFloor && a.vy >= 0 && a.stateT > 0.1f) { a.state = 0; a.vx = 0; }
        physics(a, dt);
    } else if (t == "PterodactilRed" || t == "PterodactilGreen" || t == "UglyDragon") {
        // Esperan volando; cuando el jugador se acerca lo persiguen (rojo/feo) o avanzan (verde).
        a.anim.play("flying");
        facePlayer();
        if (a.state == 0) { a.vx = a.vy = 0; if (std::fabs(pcx - a.x) < 40 + VIEW_W * 0.5f && dist < 300) { a.state = 1; a.stateT = 0; } }
        else if (t == "PterodactilGreen") { a.vx = std::max(-100.0f, std::min(100.0f, a.vx + (a.left ? -100.0f : 100.0f) * dt)); a.vy = 0; }
        else seek(pcx, pcy, 60 * 3, 80);
        fly();
    } else if (t == "Blowfish") {
        // Pez globo: se infla cuando el jugador está a 200 y lo sigue despacio; se desinfla lejos.
        facePlayer();
        if (a.state == 0) { a.anim.play("normal"); a.vx = a.vy = 0; if (dist < 200) { a.state = 1; a.anim.play("activated", true); } }
        else if (a.state == 1) { seek(pcx, pcy, 50 * 2, 50); if (dist > 250) { a.state = 2; a.stateT = 0; } }
        else { a.vx *= 0.95f; a.vy *= 0.95f; if (a.stateT > 2) a.state = 0; }
        fly();
    } else if (t == "Fish") {
        a.vx = a.left ? -100.0f : 100.0f; a.vy = 0; a.gravity = 0;
        physics(a, dt);
        if (a.hitWall) a.left = !a.left;
    } else if (t == "Bat") {
        seek(pcx, pcy, 60 * 3, 120);
        facePlayer();
        fly();
    } else if (t == "Ghost") {
        if (a.state == 0) { a.anim.play("normal"); facePlayer(); a.vx = a.vy = 0; if (dist < 200) { a.state = 1; a.anim.play("attack"); } }
        else { seek(pcx, pcy, 120 * 2, 240); a.left = a.vx < 0; }
        fly();
    } else if (t == "Axe" || t == "Fireball2") {
        // Hacha / bola de fuego del castillo: invisible hasta que el jugador se acerca (hacha a 250,
        // fuego cada 2 s), aparece, después cae sobre el jugador y desaparece a los 7 s.
        switch (a.state) {
            case 0: a.c = 0; a.vx = a.vy = 0; if ((t == "Axe" && dist < 250) || (t == "Fireball2" && a.stateT > 2.0f && dist < 400)) { a.state = 1; a.stateT = 0; a.anim.play("appear", true); } break;
            case 1: if (a.anim.ended) { a.state = 2; a.stateT = 0; a.anim.play("normal"); } break;
            case 2:
                if (a.stateT > (t == "Axe" ? 0.5f : 2.0f)) { a.vy += 960 * 0.25f * dt; a.vx += (pcx > a.x ? 100.0f : -100.0f) * dt; a.vx = std::max(-100.0f, std::min(100.0f, a.vx)); }
                if (a.stateT > 7) { a.state = 3; a.anim.play("disappear", true); }
                break;
            case 3: if (a.anim.ended) { a.state = 0; a.stateT = 0; a.x = a.x0; a.y = a.y0; a.anim.play("normal"); } break;
        }
        a.harmful = a.state == 2; a.stompable = false;
        fly();
    } else if (t == "Head" || t == "Body") {
        // Armaduras del castillo: el cuerpo es decorado; la cabeza se tira sobre el jugador a 150.
        if (t == "Body") { a.harmful = false; a.stompable = false; a.px = a.x; a.py = a.y; }
        else {
            facePlayer();
            if (a.state == 0) { a.anim.play("Stand"); a.harmful = false; a.px = a.x; a.py = a.y; if (dist < 150) { a.state = 1; a.anim.play("Attack"); } }
            else { a.harmful = true; a.vx = (pcx > a.x ? 100.0f : -100.0f); a.gravity = 960; physics(a, dt); }
        }
    } else if (t == "Fenix" || t == "Witch") {
        // Avanzan volando hacia adelante (fuerza 240).
        if (a.state == 0) { a.state = 1; a.left = a.has("left") || (!a.has("right") && pcx < a.x); }
        if (a.stateT > 0.2f || t == "Fenix") a.vx = std::max(-240.0f, std::min(240.0f, a.vx + (a.left ? -240.0f : 240.0f) * dt));
        a.vy = 0;
        fly();
        a.stompable = t == "Witch";
    } else if (t == "Piranha" || t == "Anguila") {
        // En el agua nadan (piraña ataca a 128); fuera del agua saltan buscando volver.
        const bool wet = inWater(a);
        if (t == "Anguila" && a.state == 3) {        // electrificada (le pegó el boomerang): daña al jefe
            a.vx = a.vy = 0; a.anim.play("attack");
            if (a.stateT > 2) { a.state = 0; a.anim.play("normal"); }
            a.px = a.x; a.py = a.y;
        } else if (wet) {
            a.gravity = 0;
            if (a.state == 0 || a.state == 2) { a.state = 1; a.stateT = 0; a.vx = a.left ? -60.0f : 60.0f; a.vy = t == "Anguila" ? 15.0f : 0.0f; if (t == "Piranha") a.anim.play("normal"); }
            if (t == "Piranha" && a.state == 1 && a.stateT > 1 && dist < 128) { a.state = 4; a.stateT = 0; facePlayer(); a.anim.play("attack", true); a.vx = a.left ? 30.0f : -30.0f; }
            if (a.state == 4) { if (a.stateT > 0.5f) seek(pcx, pcy, 180 * 3, 180); if (a.stateT > 1.5f) { a.state = 1; a.stateT = 0; } }
            if (a.state == 1 && a.stateT > 2) { a.vx = a.left ? -60.0f : 60.0f; a.stateT = 0; }
            physics(a, dt);
            if (a.hitWall) a.left = !a.left;
            if (a.state == 1 && t == "Anguila") { if (a.onFloor) a.vy = -15; else if (a.hitCeil) a.vy = 15; a.left = a.vx < 0; }
        } else {
            a.state = 2; a.gravity = 400;
            if (a.onFloor) { facePlayer(); a.vx = a.left ? -30.0f : 30.0f; a.vy = -120; if (t == "Piranha") a.anim.play("pain"); }
            physics(a, dt);
        }
        a.stompable = t == "Piranha";
        a.harmful = true;
    } else if (t == "Fireball") {
        // Fuego de Willy (480 hacia adelante); se apaga contra un tile o fuera de la pantalla.
        a.vx = a.left ? -480.0f : 480.0f; a.vy = 0;
        fly();
        if (solidAt(a.x, a.y - 8) || a.x < camX - 32 || a.x > camX + VIEW_W + 32) a.removed = true;
    } else if (t == "MalmarkFireball") {
        // Bolas de fuego de Malmark: persiguen al jugador 5 s; si se las pisa se congelan 2 s (sirven de trampolín).
        if (a.state == 0) { if (a.anim.ended || a.anim.name != "appear") a.anim.play("normal"); seek(pcx, pcy, 60 * 3, 60); if (a.stateT > 5) { a.state = 2; a.anim.play("disappear", true); } }
        else if (a.state == 1) { a.vx = a.vy = 0; a.flashT = 2 - a.stateT; if (a.stateT > 2) { a.state = 2; a.anim.play("disappear", true); } }
        else { a.vx = a.vy = 0; if (a.anim.ended) a.removed = true; }
        a.harmful = a.state == 0; a.stompable = a.state != 2;
        fly();
    } else if (t == "Rocks") {
        a.gravity = 600;
        if (a.state == 0) { a.state = 1; a.vy = 60; }
        a.solidCollide = false;
        physics(a, dt);
        if (a.y > a.y0 + 600 || solidAt(a.x, a.y)) { Actor* fx = spawn("Effect", a.x, a.y); fx->anim.play("miniExplosion"); a.removed = true; }
    } else if (t == "Ogre") {
        // Jefe del bosque (OGRE_*): estatua → se transforma → camina 3 s hacia el jugador →
        // ataca (golpe con temblor) → queda aturdido 2 s (ahí se lo pisa) → se recupera.
        if (a.phase < 3) { if (a.phase == 0) a.anim.play("statue"); if (bossWake("headOgre")) { a.state = 2; a.stateT = 0; } }
        else switch (a.state) {
            case 2:
                a.anim.play("walk");
                if (a.hitWall) a.left = !a.left;
                if (std::fabs(p.x - a.x) > 64) a.left = p.x < a.x;
                a.vx = a.left ? -60.0f : 60.0f;
                if (a.stateT > 3) { a.state = 3; a.stateT = 0; a.vx = 0; a.anim.play("attack", true); }
                break;
            case 3:
                a.vx = 0;
                if (a.anim.flagFirst("MakeEffect")) { sfx("v_explosion"); shakeT = 0.5f; }
                if (a.anim.ended) { a.state = 4; a.stateT = 0; a.anim.play("fail"); }
                break;
            case 4: a.vx = 0; if (a.stateT > 2) { a.state = 5; a.stateT = 0; a.anim.play("recovery", true); } break;
            case 5: a.vx = 0; if (a.anim.ended || a.stateT > 1.0f) { a.state = 2; a.stateT = 0; } break;
            case 6: if (a.stateT > 0.5f) { if (a.life > 0) { a.state = 5; a.stateT = 0; a.anim.play("recovery", true); } else { a.state = 7; a.stateT = 0; a.anim.play("die", true); shakeT = 3; } } break;
            case 7: a.vx = 0; if (a.stateT > 3 && a.c == 0) { a.c = 1; a.anim.play("dieStatue"); bossDefeated(); } break;
        }
        a.stompable = a.phase == 3 && (a.state == 4 || a.state == 5);
        a.harmful = a.phase == 3 && (a.state == 2 || a.state == 3);
        physics(a, dt);
    } else if (t == "Iguana") {
        // Jefe del lago: nada buscando al jugador (120, enojado 240). Solo lo lastiman las anguilas
        // electrificadas (que se activan con el boomerang).
        if (a.phase < 3) { if (a.phase == 0) { a.anim.play("original"); facePlayer(); } if (bossWake("headTriton")) { a.state = 0; a.stateT = 0; } a.px = a.x; a.py = a.y; }
        else {
            const bool wet = inWater(a);
            if (a.state == 9) { a.vx = a.vy = 0; if (a.stateT > 0.5f) { a.state = 1; a.stateT = 0; a.anim.play("angry"); } }
            else if (a.state == 10) { a.gravity = 900; a.solidCollide = false; physics(a, dt); if (a.stateT > 3 && a.c == 0) { a.c = 1; bossDefeated(); } }
            else {
                facePlayer();
                a.anim.play(a.state == 1 ? "angry" : "normal");
                seek(pcx, pcy, a.state == 1 ? 180.0f * 3 : 30.0f * 3, a.state == 1 ? 240.0f : 120.0f);
                a.gravity = wet ? 0 : 900;
                if (a.state == 1 && a.stateT > 4) { a.state = 0; a.stateT = 0; }
                physics(a, dt);
                if (a.onFloor) a.vy = -60;
                if (a.anim.flagFirst("PlaySound")) sfx("boomerang_wall_hit");
                for (auto& o : actors)
                    if (o->type == "Anguila" && o->state == 3 && !o->removed && overlap(a.box(), o->box())) {
                        a.life--; bossBar = std::max(0.0f, a.life / 5.0f); sfx("v_explosion"); a.anim.play("damage", true);
                        o->state = 0;
                        if (a.life <= 0) { a.state = 10; a.stateT = 0; a.vy = -300; a.dead = false; }
                        else { a.state = 9; a.stateT = 0; }
                        break;
                    }
            }
        }
        a.harmful = a.phase == 3 && a.state != 10; a.stompable = false;
    } else if (t == "Dragon") {
        // Jefe volador del mundo de los dragones: persigue al jugador y escupe fuego cada 2 s; lo
        // lastima el fuego de Willy (70 de vida).
        if (a.phase < 3) { if (a.phase == 0) { a.anim.play("original"); facePlayer(); } if (bossWake("headDragon")) { a.state = 0; a.stateT = 0; a.b = 2; } a.px = a.x; a.py = a.y; }
        else if (a.state == 10) { a.anim.play("dead"); a.vy += 900 * dt; fly(); if (a.stateT > 3 && a.c == 0) { a.c = 1; bossDefeated(); } }
        else {
            facePlayer();
            seek(pcx, pcy, 120 * 2, 120);
            fly();
            a.b -= dt;
            if (a.b <= 0) {
                a.b = 2; a.anim.play("flyingAttack", true); a.a = 0.5f;
                Actor* f = spawn("Fireball", a.x + (a.left ? -52.0f : 52.0f), a.y - a.h() * 0.5f); f->left = a.left; f->props["enemy"] = "1";
                sfx("willyFire");
            }
            if (a.a > 0) { a.a -= dt; if (a.a <= 0) a.anim.play("flying"); }
        }
        a.harmful = a.phase == 3 && a.state != 10; a.stompable = false;
    } else if (t == "Golem") {
        // Jefe de la montaña: la cabeza (se la pisa), el torso y el puño que cae y suelta rocas.
        if (a.phase < 3) {
            if (a.phase == 0) a.anim.play("original");
            if (bossWake("headGolem")) { a.state = 0; a.stateT = 0; a.b = 0; a.anim.play("head"); a.c = 0; a.props["fistY"] = std::to_string(a.y - 220); }
            a.px = a.x; a.py = a.y;
        } else {
            // Puño (a.a = x, a.b = estado): sube, se ubica sobre el jugador y cae a 640.
            float& fx = a.a; float& fs = a.b;
            const float fyTop = (float)atof(a.props["fistY"].c_str());
            if (fx == 0) fx = a.x - 120;
            float& fy = a.c;
            if (fy == 0) fy = fyTop;
            if (a.state == 10) { if (a.stateT > 3 && a.props["won"].empty()) { a.props["won"] = "1"; bossDefeated(); } }
            else if (a.state == 9) { if (a.stateT > 0.6f) { a.state = 0; a.stateT = 0; a.anim.play("head"); } }
            else if (fs == 0) { fx += std::max(-120 * dt, std::min(120 * dt, pcx - fx)); fy = fyTop; if (a.stateT > 2.5f) { fs = 1; a.stateT = 0; } }
            else if (fs == 1) {
                fy += 640 * dt;
                if (solidAt(fx, fy + 4) || fy > a.y) {
                    fs = 2; a.stateT = 0; shakeT = 0.5f; sfx("v_explosion");
                    for (int i = 0; i < 4; i++) { Actor* r = spawn("Rocks", camX + 60 + (rand() % (VIEW_W - 120)), camY - 20); r->anim.play("bigRock"); }
                }
            } else if (fs == 2) { if (a.stateT > 2) { fs = 3; a.stateT = 0; } }
            else { fy -= 240 * dt; if (fy <= fyTop) { fy = fyTop; fs = 0; a.stateT = 0; } }
            a.props["fx"] = std::to_string(fx);
            // El puño lastima al caer.
            const SDL_FRect fist = { fx - 40, fy - 70, 80, 70 };
            if (fs == 1 && overlap(fist, p.box())) hurtPlayer(false);
            a.px = a.x; a.py = a.y;
        }
        a.harmful = false; a.stompable = a.phase == 3 && a.state < 9;
    } else if (t == "Malmark") {
        // Jefe final: vuela sobre el jugador, tira bolas de fuego (que congeladas sirven de trampolín).
        if (a.phase == 0) { a.anim.play("normal"); if (a.x > camX && a.x < camX + VIEW_W && a.y > camY && a.y - a.h() < camY + VIEW_H) { a.phase = 3; bossBar = 1; bossIcon = "headMalmark"; music = map.prop("music,finalBoss", "boss.ogg"); v2::music(music); a.state = 0; a.stateT = 0; } a.px = a.x; a.py = a.y; }
        else if (a.state == 10) { a.vx = a.vy = 0; a.flashT = 7 - a.stateT; shakeT = std::max(shakeT, 0.2f); a.px = a.x; a.py = a.y; if (a.stateT > 7 && a.c == 0) { a.c = 1; bossDefeated(); } }
        else {
            facePlayer();
            seek(pcx, pcy - 140, 60 * 3, a.state == 2 ? 120.0f : 120.0f);
            fly();
            if (a.state == 0 && a.stateT > 2 && countOf("MalmarkFireball") < 4) { a.state = 1; a.stateT = 0; a.anim.play("attack", true); a.b = 0; }
            if (a.state == 1) {
                if (a.anim.flag("Custom") && a.b == 0) { a.b = 1; Actor* f = spawn("MalmarkFireball", a.x + (a.left ? -32.0f : 32.0f), a.y - 40); f->anim.play("appear", true); }
                if (a.anim.ended) { a.state = 0; a.stateT = 0; a.anim.play("normal"); }
            }
            if (a.state == 2) { if (a.stateT > 2) { a.state = 0; a.stateT = 0; a.anim.play("normal"); } }
        }
        a.harmful = false; a.stompable = a.phase == 3 && a.state != 2 && a.state != 10;
    } else {
        a.px = a.x; a.py = a.y;
    }
    a.anim.update(dt);
}

// Golpe de boomerang / fuego de Willy sobre un enemigo.
bool World::shotHits(Actor& s, bool fire) {
    for (auto& eu : actors) {
        Actor& e = *eu;
        if (e.removed || e.dead || !isEnemy(e.type) || &e == &s) continue;
        const std::string& et = e.type;
        if (!overlap(s.box(), e.box())) continue;
        if (et == "Spyke" || et == "Hedgehog" || et == "Body" || et == "Rocks" || et == "MalmarkFireball") continue;
        if ((et == "Axe" || et == "Fireball2") && e.state != 2) continue;
        if (et == "Anguila") { if (!fire) { e.state = 3; e.stateT = 0; sfx("v_explosion"); return true; } continue; }
        if (isBoss(et)) {
            if (et == "Dragon" && fire && e.phase == 3 && e.state != 10) {
                e.life--; e.flashT = 0.2f; bossBar = std::max(0.0f, e.life / 70.0f); sfx("v_explosion");
                if (e.life <= 0) { e.state = 10; e.stateT = 0; e.vy = -200; }
                return true;
            }
            if (!fire) return true;   // el boomerang rebota en los jefes
            continue;
        }
        killEnemy(e, false);
        sfx(fire ? "enemy_hit" : "enemy_boomerang_hit");
        return true;
    }
    return false;
}

// Contacto jugador ↔ personajes: pisotones, golpes, ítems, carteles, puertas.
void World::interactions() {
    Actor& p = *player;
    if (p.state == P_LOSING || p.state == P_GONE) return;
    const SDL_FRect pb = p.box();
    bool onInfo = false;
    for (size_t i = 0; i < actors.size(); i++) {
        Actor& a = *actors[i];
        if (&a == player || a.removed) continue;
        const std::string& t = a.type;
        const SDL_FRect ab = a.box();
        if (t == "Boomerang") {
            if ((a.state == 0 || a.state == 1) && shotHits(a, false)) {
                Actor* fx = spawn("Effect", a.x, a.y); fx->anim.play("getPowerUp");
                a.state = 2; a.stateT = 0; a.vy = -120; a.vx = a.left ? 60.0f : -60.0f; a.gravity = 960;
            }
            continue;
        }
        if (t == "Fireball") {
            if (a.props.count("enemy")) { if (overlap(pb, ab)) { hurtPlayer(false); a.removed = true; } }
            else if (shotHits(a, true)) { a.removed = true; Actor* fx = spawn("Effect", a.x, a.y); fx->anim.play("miniExplosion"); }
            continue;
        }
        if (t == "Info") {
            // Se ve mientras se toca el cartel y queda 1,2 s al alejarse (como en Vascolet 1).
            if (overlap(pb, ab)) { onInfo = true; if (msgSign != &a || msgT <= 0) { showMsg(a.props["info"], true); msgSign = &a; } msgT = 1.2f; }
            continue;
        }
        if (t == "Save") {
            // Al tocar un punto nuevo: "Juego salvado.", se guarda el lugar y su disquete queda animado.
            if (overlap(pb, ab) && (std::fabs(saveX - a.x) > 1 || std::fabs(saveY - a.y) > 1)) {
                saveX = a.x; saveY = a.y; saveCamD = camD;
                msg = "Juego salvado."; msgT = 2.0f; msgLinger = false; msgSign = &a;
                sfx("save");
                for (auto& o : actors) if (o->type == "Save") o->anim.play("saveStale");
                a.anim.play("savePoint");
            }
            continue;
        }
        if (t.find("Door") != std::string::npos) {
            // Puerta: bloquea su lugar hasta que el jugador llega cerca (32 de costado, 96 de alto) con su llave.
            int k = -1;
            for (int j = 0; j < 4; j++) if (t == DOOR_ITEM[j]) k = j;
            const SDL_FRect db = { a.x - 16, a.y - 128, 32, 128 };
            const bool near = std::fabs(p.x - a.x) < 32 + 16 + p.w() && p.y > db.y && p.y - p.h() < a.y + 96;
            if (near && k >= 0 && keys[k] > 0) {
                keys[k]--;
                Actor* fx = spawn(t, a.x, a.y); fx->anim.play("open", true); fx->props["opening"] = "1"; fx->phase = 9;
                sfx("door");
                a.removed = true;
                continue;
            }
            if (a.phase != 9 && overlap({ pb.x - 1, pb.y, pb.w + 2, pb.h }, db)) {
                if (p.x < a.x) p.x = std::min(p.x, db.x - p.w() - 0.01f); else p.x = std::max(p.x, db.x + db.w + p.w() + 0.01f);
                if (p.vx != 0 && ((p.vx > 0) == (p.x < a.x))) p.vx = 0;
            }
            if (a.phase == 9 && a.anim.ended) a.removed = true;
            continue;
        }
        if (t == "Portal" || t == "Platform" || t == "Effect" || t == "MalmarkFx") continue;
        if (t == "Golem" && a.phase == 3 && a.state < 9) {
            // Cabeza del gólem: se la pisa desde arriba.
            const SDL_FRect head = { a.x - 28, a.y - 150, 56, 50 };
            if (overlap(pb, head)) {
                if ((p.vy > 0 || p.py < p.y) && p.py <= head.y + 20) {
                    sfx("enemy_hit"); Actor* fx = spawn("Effect", a.x, head.y); fx->anim.play("miniExplosion");
                    p.vy = -500; canDoubleJump = true; playerState(P_JUMP); p.vy = -500;
                    a.life--; bossBar = std::max(0.0f, a.life / 5.0f); a.anim.play("headDamage", true);
                    if (a.life <= 0) { a.state = 10; a.stateT = 0; shakeT = 2; } else { a.state = 9; a.stateT = 0; }
                } else hurtPlayer(false);
            }
            continue;
        }
        if (!overlap(pb, ab)) continue;
        if (isItem(t)) {
            a.removed = true;
            Actor* fx = spawn("Effect", a.x, a.y - 16); fx->anim.play("getPowerUp");
            if (t == "Coin") { prog.coins++; static const char* C[4] = { "coin", "coin2", "coin3", "coin4" }; sfx(C[rand() & 3]); continue; }
            if (t == "Energy") { prog.energy = std::min(5, prog.energy + 1); sfx("energy"); }
            else if (t == "Energy2") { prog.energy = std::min(5, prog.energy + 2); sfx("energy"); }
            else if (t == "ExtraLive") { prog.lives = std::min(99, prog.lives + 1); sfx("extraLife"); }
            else if (t == "ItemVascolet") { runEnabled = true; sfx("vascolet"); }
            else if (t == "Cereals") { doubleJumpEnabled = true; sfx("vascolet"); }
            else for (int j = 0; j < 4; j++) if (t == KEY_ITEM[j]) { keys[j] = std::min(9, keys[j] + 1); sfx("key"); }
            continue;
        }
        if (!isEnemy(t) || a.dead) continue;
        // Pisotón: el jugador cae y sus pies venían por encima de la mitad de arriba del enemigo.
        const bool falling = p.vy > 0 || p.py < p.y;
        const bool fromAbove = falling && p.py <= ab.y + std::min(24.0f, ab.h * 0.5f);
        if (fromAbove && a.stompable) {
            sfx("enemy_hit");
            Actor* fx = spawn("Effect", a.x, ab.y); fx->anim.play("miniExplosion");
            canDoubleJump = true;
            playerState(P_JUMP); p.vy = -500;
            if (t == "Ogre") {
                a.life--; a.state = 6; a.stateT = 0; a.anim.play("damage", true); a.vx = 0; a.vy = -120;
                bossBar = std::max(0.0f, a.life / 5.0f); shakeT = 0.5f;
            } else if (t == "Malmark") {
                a.life--; a.state = a.life <= 0 ? 10 : 2; a.stateT = 0; a.anim.play("damage", true); a.flashT = 0.2f;
                bossBar = std::max(0.0f, a.life / 5.0f); shakeT = 0.2f;
            } else if (t == "MalmarkFireball") {
                if (a.state == 0) { a.state = 1; a.stateT = 0; }
            } else killEnemy(a, true);
            continue;
        }
        if (a.harmful) hurtPlayer(false);
    }
    (void)onInfo;
}

// ---------------------------------------------------------------------------------------------
// Cámara: sigue al jugador, sin salirse del mapa ni cruzar las líneas del grupo "Camera".
// ---------------------------------------------------------------------------------------------
void World::updateCamera(bool snap) {
    Actor& p = *player;
    if (p.state == P_LOSING || p.state == P_GONE) { if (!snap) return; }
    if (autoCam) {
        // Recorre el camino a velocidad fija (el punto es el centro de la vista).
        float d = camD, total = 0;
        SDL_FPoint pt = camPath.back();
        for (size_t i = 0; i + 1 < camPath.size(); i++) {
            const float seg = std::hypot(camPath[i + 1].x - camPath[i].x, camPath[i + 1].y - camPath[i].y);
            if (d <= seg) { const float k = seg > 0 ? d / seg : 0; pt = { camPath[i].x + (camPath[i + 1].x - camPath[i].x) * k, camPath[i].y + (camPath[i + 1].y - camPath[i].y) * k }; total = -1; break; }
            d -= seg;
        }
        (void)total;
        pcamX = snap ? pt.x - VIEW_W * 0.5f : camX; pcamY = snap ? pt.y - VIEW_H * 0.5f : camY;
        camX = std::max(0.0f, std::min(pt.x - VIEW_W * 0.5f, (float)map.w * 32 - VIEW_W));
        camY = std::max(0.0f, std::min(pt.y - VIEW_H * 0.5f, (float)map.h * 32 - VIEW_H));
        if (snap) { pcamX = camX; pcamY = camY; }
        return;
    }
    float tx = p.x - VIEW_W * 0.5f;
    // Como en el video: los pies del jugador quedan a unos 3/4 de la altura de la pantalla.
    float ty = p.y - VIEW_H * 0.74f;
    for (int it = 0; it < 2; it++) {
        for (const MapObject& l : camLines) {
            if (l.w == 0 && l.h > 0) {           // pared vertical
                if (ty + VIEW_H <= l.y || ty >= l.y + l.h) continue;
                if (p.x < l.x) tx = std::min(tx, l.x - VIEW_W); else tx = std::max(tx, l.x);
            } else if (l.h == 0 && l.w > 0) {    // techo/piso horizontal
                if (tx + VIEW_W <= l.x || tx >= l.x + l.w) continue;
                if (p.y - p.h() * 0.5f < l.y) ty = std::min(ty, l.y - VIEW_H); else ty = std::max(ty, l.y);
            }
        }
        tx = std::max(0.0f, std::min(tx, (float)map.w * 32 - VIEW_W));
        ty = std::max(0.0f, std::min(ty, (float)map.h * 32 - VIEW_H));
    }
    pcamX = snap ? tx : camX; pcamY = snap ? ty : camY;
    if (snap) { camX = tx; camY = ty; return; }
    // Seguimiento suave (como el original, que alcanza al jugador en pocos cuadros).
    camX += (tx - camX) * 0.25f;
    camY += (ty - camY) * 0.18f;
    if (std::fabs(tx - camX) < 0.5f) camX = tx;
    if (std::fabs(ty - camY) < 0.5f) camY = ty;
}

// ---------------------------------------------------------------------------------------------
void World::update(float dt) {
    if (overT >= 0) { overT += dt; return; }
    if (winT >= 0) winT += dt;
    if (msgT > 0) msgT -= dt;
    if (shakeT > 0) shakeT -= dt;
    if (autoCam && player->state != P_LOSING && player->state != P_GONE && winT < 0) camD += AUTO_CAM_SPEED * dt;
    for (auto& z : secrets) {
        const bool in = player->x >= z.x && player->x < z.x + z.w && player->y - 20 >= z.y && player->y - 20 < z.y + z.h;
        z.alpha = std::max(0.0f, std::min(1.0f, z.alpha + (in ? -dt * 3 : dt * 3)));
    }
    if (winT < 0 || winT < 1.0f) updatePlayer(dt);
    else { player->px = player->x; player->py = player->y; player->anim.play("stand"); player->anim.update(dt); }
    for (size_t i = 0; i < actors.size(); i++) if (actors[i].get() != player && !actors[i]->removed) updateActor(*actors[i], dt);
    interactions();
    actors.erase(std::remove_if(actors.begin(), actors.end(), [&](const std::unique_ptr<Actor>& a) {
        if (!a->removed) return false;
        if (msgSign == a.get()) msgSign = nullptr;
        return true;
    }), actors.end());
    updateCamera(false);
}

// ---------------------------------------------------------------------------------------------
// Dibujo
// ---------------------------------------------------------------------------------------------
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

void World::drawBackground() {
    plat::fillScreen({ 0, 0, 0, 255 });
    drawParallax(back, backXO, lerpf(pcamX, camX, interp), lerpf(pcamY, camY, interp), VIEW_W, VIEW_H);
}

void World::drawLayer(const Layer& l, bool fg) {
    const float cx = lerpf(pcamX, camX, interp) + sx, cy = lerpf(pcamY, camY, interp) + sy;
    if (!fg) { map.drawView(l, cx, cy, VIEW_W, VIEW_H); return; }
    // Paredes secretas: el frente se vuelve transparente mientras el jugador está adentro.
    map.drawView(l, cx, cy, VIEW_W, VIEW_H, [&](int tx, int ty) -> uint8_t {
        for (auto& z : secrets)
            if (tx * 32 + 16 >= z.x && tx * 32 + 16 < z.x + z.w && ty * 32 + 16 >= z.y && ty * 32 + 16 < z.y + z.h)
                return (uint8_t)(255 * z.alpha);
        return 255;
    });
}

void World::draw() {
    if (shakeT > 0) { sx = (float)(rand() % 7 - 3); sy = (float)(rand() % 7 - 3); } else sx = sy = 0;
    drawBackground();
    if (const Layer* l = map.layer("Tiles")) drawLayer(*l, false);
    const float cx = std::floor(lerpf(pcamX, camX, interp)) + sx, cy = std::floor(lerpf(pcamY, camY, interp)) + sy;
    auto drawActor = [&](const Actor& a) {
        if (a.removed) return;
        if ((a.type == "Axe" || a.type == "Fireball2") && a.state == 0) return;   // invisibles hasta aparecer
        if (a.flashT > 0 && ((int)(a.flashT * 30) & 1)) return;
        if (a.type == "Golem" && a.phase == 3) {
            // Gólem armado: torso, cabeza encima y el puño que cae.
            const float x = a.x - cx, y = a.y - cy;
            const AnimSet* g = a.anim.set;
            static Animator torso, fist;
            torso.set = fist.set = g;
            torso.play("torso"); torso.update(1.0f / 60);
            fist.play("fist");
            drawFrame(torso.cur(), std::round(x), std::round(y - 20), false);
            drawFrame(a.anim.cur(), std::round(x), std::round(y - 96), false);
            const float fx = (float)atof(a.props.count("fx") ? a.props.at("fx").c_str() : "0");
            drawFrame(fist.cur(), std::round(fx - cx), std::round(a.c - cy), false);
            return;
        }
        if (&a == player && (a.state == P_GONE)) return;
        if (&a == player && invT > 0 && a.state != P_DAMAGE && ((int)(invT * 30) & 1)) return;
        const float x = lerpf(a.px, a.x, interp) - cx, y = lerpf(a.py, a.y, interp) - cy;
        if (x < -200 || x > VIEW_W + 200 || y < -200 || y > VIEW_H + 300) return;
        bool flip = a.left;
        if (a.type == "Boomerang" || isItem(a.type) || a.type == "Info" || a.type == "Save" || a.type == "Effect" || a.type.find("Door") != std::string::npos || a.type == "Platform") flip = false;
        if (a.type == "Effect" || a.type == "Boomerang") plat::setDrawAdditive(false);
        drawFrame(a.anim.cur(), std::round(x), std::round(y), flip);
    };
    for (auto& a : actors) if (a.get() != player && (a->type == "Info" || a->type == "Save" || a->type.find("Door") != std::string::npos || a->type == "Portal")) drawActor(*a);
    for (auto& a : actors) if (a.get() != player && !(a->type == "Info" || a->type == "Save" || a->type.find("Door") != std::string::npos || a->type == "Portal") && a->type != "Effect") drawActor(*a);
    drawActor(*player);
    for (auto& a : actors) if (a->type == "Effect") drawActor(*a);
    if (const Layer* l = map.layer("foreground")) drawLayer(*l, true);
}

// HUD del original (images/hud): vidas, energía, monedas x/total, zapatilla y resorte, llaves.
void World::drawHud(float uiW, bool messages) {
    const SpriteLib* hud = spriteLib("v2/assets/images/hud/hud.xml");
    auto spr = [&](const char* k, float x, float y) { drawSprite(hud->get(k), x, y); };
    const float s = VIEW_H / 384.0f; (void)s;
    // Vidas
    spr("lives", 44, 20);
    char buf[32];
    snprintf(buf, sizeof buf, "%d", prog.lives);
    drawText(buf, 80 - textWidth(buf, 0.8f, FONT_HUD) * 0.5f, 19 - textHeight(0.8f, FONT_HUD) * 0.5f, 0.8f, { 255, 255, 255, 255 }, FONT_HUD);
    for (int i = 0; i < 5; i++) spr(i < prog.energy ? "colorHeart" : "blackHeart", 18 + i * 13, 46);
    // Monedas
    spr("coins", 138, 20);
    snprintf(buf, sizeof buf, "%d/%d", prog.coins, coinTotal);
    drawText(buf, 155 - textWidth(buf, 0.8f, FONT_HUD) * 0.5f, 19 - textHeight(0.8f, FONT_HUD) * 0.5f, 0.8f, { 255, 255, 255, 255 }, FONT_HUD);
    // Zapatilla (correr) y resorte (doble salto)
    const float mid = uiW * 0.5f;
    spr(runEnabled ? "colorSneaker" : "greySneaker", mid + 5, 26);
    spr(doubleJumpEnabled ? "colorSpring" : "greySpring", mid + 46, 28);
    // Llaves
    static const char* const KEY[3] = { "redKey", "greenKey", "blueKey" };
    for (int i = 0; i < 3; i++) {
        const float kx = uiW - 96 + i * 37;
        spr(KEY[i], kx, 31);
        snprintf(buf, sizeof buf, "%d", keys[i]);
        drawText(buf, kx - textWidth(buf, 0.8f, FONT_HUD) * 0.5f, 41 - textHeight(0.8f, FONT_HUD) * 0.5f, 0.8f, { 255, 255, 255, 255 }, FONT_HUD);
    }
    // Barra del jefe
    if (bossBar >= 0) {
        spr("bossEnergyBase", uiW - 150, VIEW_H - 30);
        const Sprite* e = hud->get("bossEnergy");
        if (e && e->img) {
            const int w = (int)(e->r.w * std::max(0.0f, std::min(1.0f, bossBar)));
            plat::drawImage(e->img, SDL_Rect{ e->r.x, e->r.y, w, e->r.h }, SDL_FRect{ uiW - 150 - e->ax, (float)(VIEW_H - 30 - e->ay), (float)w, (float)e->r.h });
        }
        spr(bossIcon.c_str(), uiW - 18, VIEW_H - 22);
    }
#ifdef HOST
    if (getenv("V2_DEBUG")) {
        char d[160];
        snprintf(d, sizeof d, "x %.0f y %.0f vx %.0f vy %.0f st %d fl %d cam %.0f,%.0f", player->x, player->y, player->vx, player->vy, player->state, player->onFloor, camX, camY);
        drawText(d, 4, VIEW_H - 16, 0.6f, { 255, 255, 255, 255 }, FONT_SMALL);
    }
#endif
    // Cartel rojo de mensajes (tips, "Juego salvado.")
    if (messages && msgT > 0 && !msg.empty()) {
        const Sprite* bn = hud->get("cartelDialogos");
        const float bx = uiW * 0.5f, by = 70;
        if (bn) drawSprite(bn, bx, by);
        drawTextBox(msg, bx, by + 23, 470, 44);
    }
}

}
