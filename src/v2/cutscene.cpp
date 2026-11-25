#include "cutscene.h"
#include "world.h"
#include "../platform.h"
#include <algorithm>
#include <cmath>

namespace v2 {

namespace {

enum Op { SPAWN, ANIM, ANIMWAIT, MOVE, MOVENB, JUMP, FLY, SAY, TITLE, WAIT, FACE, HIDE, MUSIC, SFX, FLASH, SHAKE, END };
const float GROUND = 640;

}

Cutscene::CA* Cutscene::find(const std::string& id) {
    for (auto& a : actors) if (a.id == id) return &a;
    return nullptr;
}

// Guiones de las escenas (orden de 0x450810: quién habla, qué hace cada personaje y los textos cs*-msg*).
void Cutscene::script(int n) {
    auto spawn = [&](const char* id, const char* folder, const char* anim, float x, float y, bool left = false) { steps.push_back({ SPAWN, id, folder, anim, x, y, left ? 1.0f : 0.0f }); };
    auto anim = [&](const char* id, const char* a) { steps.push_back({ ANIM, id, a }); };
    auto animWait = [&](const char* id, const char* a, float max = 3) { steps.push_back({ ANIMWAIT, id, a, "", max }); };
    auto move = [&](const char* id, const char* a, float x, float speed, bool wait = true) { steps.push_back({ wait ? MOVE : MOVENB, id, a, "", x, speed }); };
    auto jump = [&](const char* id, const char* a, float vx, float vy, const char* land = "") { steps.push_back({ JUMP, id, a, land, vx, vy }); };
    auto fly = [&](const char* id, const char* a, float x, float y, float speed) { steps.push_back({ FLY, id, a, "", x, y, speed }); };
    auto say = [&](const char* who, const char* key) { steps.push_back({ SAY, who, key }); };
    auto title = [&](const char* key) { steps.push_back({ TITLE, "", key }); };
    auto wait = [&](float s) { steps.push_back({ WAIT, "", "", "", s }); };
    auto face = [&](const char* id, bool left) { steps.push_back({ FACE, id, "", "", left ? 1.0f : 0.0f }); };
    auto hide = [&](const char* id) { steps.push_back({ HIDE, id }); };
    auto mus = [&](const char* f, bool loop = true) { steps.push_back({ MUSIC, f, "", "", loop ? 1.0f : 0.0f }); };
    auto snd = [&](const char* f) { steps.push_back({ SFX, f }); };
    auto flash = [&](float s) { steps.push_back({ FLASH, "", "", "", s }); };
    auto shk = [&](float s) { steps.push_back({ SHAKE, "", "", "", s }); };
    const char* ALE = "dialogBannerAle"; const char* F1 = "dialogBannerFriend01"; const char* F2 = "dialogBannerFriend02";
    const char* F3 = "dialogBannerFriend03"; const char* MAL = "dialogBannerMalmark"; const char* WIL = "dialogBannerWilly";
    switch (n) {
        case 1:   // La escuela: llega Alejandro en patineta, aparece Malmark, se lleva el libro y se abre el portal.
            mus("cs_1A.ogg");
            spawn("f1", "friends", "Amigo1_stand", 432, GROUND);
            spawn("f2", "friends", "Amigo2_stand", 496, 576);
            spawn("f3", "friends", "Amigo3_stand", 752, GROUND, true);
            title("cs1-msg001");
            say(F3, "cs1-msg002");
            say(F2, "cs1-msg003");
            spawn("ale", "vascolet", "skate", 330, GROUND);
            spawn("skate", "patineta", "stand", 330, GROUND);
            move("ale", "skate", 560, 220, false);
            move("skate", "stand", 560, 220, false);
            anim("f1", "Amigo1_llegaAle"); anim("f2", "Amigo2_llegaAle"); anim("f3", "Amigo3_llegaAle");
            say(F1, "cs1-msg004");
            move("ale", "skate", 560, 220);
            jump("ale", "runJumpMidBackpack", 90, -320, "touchGroundBackpack");
            move("skate", "stand", 1000, 300, false);
            anim("f1", "Amigo1_frenaAle"); anim("f2", "Amigo2_frenaAle"); anim("f3", "Amigo3_frenaAle");
            animWait("ale", "touchGroundBackpack", 1);
            anim("ale", "standBackpack");
            hide("skate");
            say(ALE, "cs1-msg005");
            say(ALE, "cs1-msg006");
            anim("f1", "Amigo1_hablaAle");
            say(F1, "cs1-msg007");
            say(ALE, "cs1-msg008");
            mus("cs_1thunder.ogg", false);
            flash(0.25f); shk(0.8f);
            say(F3, "cs1-msg009");
            mus("cs_1B.ogg");
            spawn("mal", "malmark", "normal", 860, 600, true);
            anim("f1", "Amigo1_malmark"); anim("f2", "Amigo2_malmark"); anim("f3", "Amigo3_malmark");
            spawn("exp", "explosion", "explosion1", 680, GROUND);
            jump("ale", "damage", -120, -260, "damageFall");
            animWait("ale", "damageFall", 1);
            spawn("book", "libro", "stand", 600, GROUND);
            animWait("ale", "damageRaise", 1.5f);
            anim("ale", "stand");
            say(MAL, "cs1-msg012");
            say(MAL, "cs1-msg013");
            animWait("mal", "attack", 1.5f);
            anim("mal", "normal");
            fly("book", "stand", 830, 560, 240);
            hide("book");
            say(MAL, "cs1-msg014");
            say(MAL, "cs1-msg015");
            spawn("exp2", "explosion", "explosion2", 860, 600);
            hide("mal");
            wait(0.6f);
            spawn("portal", "portal", "stand", 820, GROUND);
            say(F1, "cs1-msg010");
            say(ALE, "cs1-msg011");
            move("ale", "run", 700, 300);
            jump("ale", "runJump", 260, -420);
            hide("ale");
            flash(0.3f);
            wait(0.8f);
            break;
        case 2:   // El lago: Willy deja caer un esnórquel; Alejandro lo toma y se tira al agua.
            mus("cs_2.ogg");
            spawn("ale", "vascolet", "stand", 352, GROUND);
            title("cs2-msg001");
            spawn("willy", "willy", "flying", 300, 420);
            spawn("snork", "snorkel", "stale", 540, 420);
            fly("willy", "flying", 900, 380, 200);
            hide("willy");
            fly("snork", "stale", 540, GROUND, 260);
            move("ale", "run", 510, 220);
            animWait("ale", "runBreak", 0.6f);
            anim("ale", "stand");
            say(ALE, "cs2-msg002");
            hide("snork");
            say(ALE, "cs2-msg004");
            say(ALE, "cs2-msg003");
            move("ale", "run", 680, 260);
            jump("ale", "runJump", 200, -420);
            hide("ale");
            wait(0.6f);
            break;
        case 3:   // Willy: Alejandro sale del agua, le da Vascolet al dragón y se suben a volar.
            mus("cs_3A.ogg");
            spawn("willy", "willy", "sleeping", 720, GROUND);
            spawn("ale", "vascolet", "swimming", 380, 700);
            title("cs3-msg001");
            jump("ale", "runJumpMid", 120, -520, "fallGround");
            animWait("ale", "fallGround", 0.8f);
            anim("ale", "stand");
            say(ALE, "cs3-msg002");
            say(ALE, "cs3-msg003");
            say(ALE, "cs3-msg004");
            move("ale", "walk", 600, 90);
            anim("ale", "stand");
            say(ALE, "cs3-msg005");
            animWait("willy", "awake", 1);
            anim("willy", "talking");
            say(WIL, "cs3-msg006");
            say(ALE, "cs3-msg007");
            say(WIL, "cs3-msg008");
            say(ALE, "cs3-msg009");
            say(WIL, "cs3-msg010");
            say(ALE, "cs3-msg011");
            animWait("ale", "standAttack", 0.6f);
            anim("ale", "stand");
            spawn("vaso", "vaso", "normal", 660, GROUND - 30);
            fly("vaso", "normal", 712, GROUND - 40, 160);
            hide("vaso");
            mus("cs_3B.ogg");
            anim("willy", "flying");
            say(WIL, "cs3-msg012");
            say(WIL, "cs3-msg013");
            say(WIL, "cs3-msg014");
            say(WIL, "cs3-msg015");
            jump("ale", "runJumpMid", 110, -380);
            hide("willy");
            anim("ale", "flyingWithWilly");
            fly("ale", "flyingWithWilly", 1000, 420, 260);
            break;
        case 4:   // La Montaña del Mal: Willy deja a Alejandro y se despide.
            mus("cs_4.ogg");
            spawn("ale", "vascolet", "flyingWithWilly", 300, 470);
            title("cs4-msg001");
            fly("ale", "flyingWithWilly", 600, 560, 200);
            say(ALE, "cs4-msg002");
            spawn("willy", "willy", "flying", 600, 560);
            jump("ale", "runJumpMid", 80, -300, "fallGround");
            animWait("ale", "fallGround", 0.8f);
            anim("ale", "stand");
            face("ale", false);
            say(WIL, "cs4-msg003");
            say(ALE, "cs4-msg004");
            say(ALE, "cs4-msg005");
            say(WIL, "cs4-msg006");
            say(ALE, "cs4-msg007");
            say(WIL, "cs4-msg008");
            fly("willy", "flying", 640, 300, 200);
            fly("willy", "flying", 1000, 200, 300);
            hide("willy");
            face("ale", true);
            move("ale", "run", 300, 260);
            break;
        case 5:   // El castillo: Malmark espera a Alejandro.
            mus("cs_5A.ogg");
            spawn("ale", "vascolet", "run", 300, GROUND);
            title("cs5-msg001");
            move("ale", "run", 520, 260);
            animWait("ale", "runBreak", 0.6f);
            anim("ale", "stand");
            say(ALE, "cs5-msg002");
            say(ALE, "cs5-msg003");
            say(ALE, "cs5-msg004");
            mus("cs_5B.ogg");
            spawn("exp", "explosion", "explosion1", 700, GROUND);
            jump("ale", "damage", -100, -240, "damageFall");
            animWait("ale", "damageFall", 1);
            animWait("ale", "damageRaise", 1.5f);
            anim("ale", "stand");
            spawn("mal", "malmark", "normal", 780, 600, true);
            say(MAL, "cs5-msg005");
            say(MAL, "cs5-msg006");
            say(MAL, "cs5-msg007");
            say(MAL, "cs5-msg008");
            spawn("exp2", "explosion", "explosion2", 780, 600);
            hide("mal");
            wait(0.5f);
            move("ale", "run", 900, 260);
            break;
        case 6:   // De nuevo en la escuela: Willy trae el libro.
            mus("cs_6A.ogg");
            spawn("f1", "friends", "Amigo1_stand", 432, GROUND);
            spawn("f2", "friends", "Amigo2_stand", 496, 576);
            spawn("f3", "friends", "Amigo3_stand", 752, GROUND, true);
            spawn("portal", "portal", "stand", 820, GROUND);
            title("cs6-msg001");
            spawn("ale", "vascolet", "runJumpMid", 800, 520, true);
            jump("ale", "runJumpMid", -160, -260, "damageFall");
            hide("portal");
            animWait("ale", "damageFall", 1);
            say(ALE, "cs6-msg002");
            say(F1, "cs6-msg003");
            animWait("ale", "damageRaise", 1.5f);
            anim("ale", "stand");
            say(ALE, "cs6-msg004");
            say(ALE, "cs6-msg005");
            say(ALE, "cs6-msg006");
            say(ALE, "cs6-msg007");
            mus("cs_6B.ogg");
            spawn("willy", "willy", "flying", 1000, 420, true);
            fly("willy", "flying", 720, 520, 220);
            anim("f1", "Amigo1_malmark"); anim("f2", "Amigo2_malmark"); anim("f3", "Amigo3_malmark");
            face("ale", false);
            say(ALE, "cs6-msg008");
            say(WIL, "cs6-msg009");
            spawn("book", "libro", "stand", 700, 540);
            fly("book", "stand", 680, GROUND, 200);
            say(ALE, "cs6-msg010");
            say(WIL, "cs6-msg011");
            say(WIL, "cs6-msg012");
            say(WIL, "cs6-msg013");
            say(ALE, "cs6-msg014");
            say(WIL, "cs6-msg015");
            fly("willy", "flying", 1000, 300, 240);
            hide("willy");
            wait(1);
            break;
    }
    steps.push_back({ END });
}

bool Cutscene::load(int n) {
    num = std::max(1, std::min(6, n));
    char path[64]; snprintf(path, sizeof path, "v2/assets/data/maps/cutscene%02d.tmx", num);
    if (!map.load(path)) return false;
    const std::string bg = map.prop("background"), xo = map.prop("backgroundXO");
    if (!bg.empty()) back.load("v2/assets/data/maps/backgrounds/" + bg + ".tmx");
    if (!xo.empty()) backXO.load("v2/assets/data/maps/backgrounds/" + xo + ".tmx");
    // Cámara fija entre las líneas del grupo "Camera" (x 384..896) con el piso a 3/4 de la pantalla.
    camX = 384; camY = GROUND - VIEW_H * 0.83f;
    script(num);
    return true;
}

void Cutscene::update(float dt) {
    if (finished) return;
    t += dt;
    if (flash > 0) flash -= dt;
    if (shake > 0) shake -= dt;
    for (auto& a : actors) {
        a.anim.update(dt);
        if (a.moving) {
            const float d = a.tx - a.x, st = a.speed * dt;
            if (std::fabs(d) <= st) { a.x = a.tx; a.moving = false; }
            else { a.x += d > 0 ? st : -st; a.left = d < 0; }
        }
        if (a.g > 0) {
            a.vy += a.g * dt; a.x += a.vx * dt; a.y += a.vy * dt;
            if (a.vy > 0 && a.y >= a.ground) { a.y = a.ground; a.g = 0; a.vx = a.vy = 0; }
        }
    }
    // Ejecuta pasos hasta encontrar uno que espera.
    for (int guard = 0; guard < 64 && pc < steps.size(); guard++) {
        Step& s = steps[pc];
        CA* a = s.a.empty() ? nullptr : find(s.a);
        bool next = true;
        switch (s.op) {
            case SPAWN: {
                CA c; c.id = s.a; c.anim.set = animSet(s.b); c.anim.play(s.c, true);
                c.x = s.x; c.y = s.y; c.ground = s.y >= GROUND - 70 ? GROUND : s.y; c.left = s.z != 0;
                if (CA* old = find(s.a)) *old = c; else actors.push_back(c);
                break;
            }
            case ANIM: if (a) a->anim.play(s.b, true); break;
            case ANIMWAIT:
                if (a) {
                    if (sayT == 0) { a->anim.play(s.b, true); sayT = 0.0001f; }
                    sayT += dt;
                    next = a->anim.ended || sayT > s.x;
                    if (next) sayT = 0;
                }
                break;
            case MOVE: case MOVENB:
                if (a) {
                    if (!a->moving && std::fabs(a->x - s.x) > 0.5f && sayT == 0) { a->tx = s.x; a->speed = s.y; a->moving = true; a->anim.play(s.b); sayT = 0.0001f; }
                    next = s.op == MOVENB || !a->moving;
                    if (s.op == MOVENB) sayT = 0;
                    if (next) sayT = 0;
                }
                break;
            case JUMP:
                if (a) {
                    if (sayT == 0) { a->g = 900; a->vx = s.x; a->vy = s.y; a->ground = GROUND; a->anim.play(s.b, true); a->left = s.x < 0; sayT = 0.0001f; next = false; }
                    else { next = a->g == 0; if (next) { sayT = 0; if (!s.c.empty()) a->anim.play(s.c, true); } }
                }
                break;
            case FLY:
                if (a) {
                    a->anim.play(s.b);
                    const float dx = s.x - a->x, dy = s.y - a->y, d = std::hypot(dx, dy), st = s.z * dt;
                    if (d <= st) { a->x = s.x; a->y = s.y; }
                    else { a->x += dx / d * st; a->y += dy / d * st; if (std::fabs(dx) > 1) a->left = dx < 0; next = false; }
                }
                break;
            case SAY: case TITLE:
                // Cada cartel se lee ~2,5 s (o se pasa tocando la pantalla / Enter).
                if (sayKey != s.b) { sayKey = s.b; speaker = s.a; sayT = 0; }
                sayT += dt;
                next = sayT > 1.6f + text(s.b).size() * 0.035f || (sayT > 0.3f && (plat::pressed(plat::BTN_JUMP)));
                if (next) { sayKey.clear(); speaker.clear(); sayT = 0; }
                break;
            case WAIT: sayT += dt; next = sayT >= s.x; if (next) sayT = 0; break;
            case FACE: if (a) a->left = s.x != 0; break;
            case HIDE: actors.erase(std::remove_if(actors.begin(), actors.end(), [&](const CA& c) { return c.id == s.a; }), actors.end()); break;
            case MUSIC: v2::music(s.a, s.x != 0); break;
            case SFX: sfx(s.a); break;
            case FLASH: flash = s.x; break;
            case SHAKE: shake = s.x; break;
            case END: finished = true; break;
        }
        if (!next) break;
        pc++;
    }
    // Las explosiones desaparecen al terminar.
    actors.erase(std::remove_if(actors.begin(), actors.end(), [](const CA& c) { return c.id.rfind("exp", 0) == 0 && c.anim.ended; }), actors.end());
}

void Cutscene::draw() {
    plat::fillScreen({ 0, 0, 0, 255 });
    const float sx = shake > 0 ? (float)(rand() % 7 - 3) : 0, sy = shake > 0 ? (float)(rand() % 7 - 3) : 0;
    drawParallax(back, backXO, camX, camY, VIEW_W, VIEW_H);
    if (const Layer* l = map.layer("Tiles")) map.drawView(*l, camX + sx, camY + sy, VIEW_W, VIEW_H);
    for (auto& a : actors) if (a.visible) drawFrame(a.anim.cur(), std::round(a.x - camX - sx), std::round(a.y - camY - sy), a.left);
    if (const Layer* l = map.layer("foreground")) map.drawView(*l, camX + sx, camY + sy, VIEW_W, VIEW_H);
    if (flash > 0) { plat::setDrawTint({ 255, 255, 255, 255 }); plat::fillScreen({ 255, 255, 255, (Uint8)std::min(255.0f, flash * 900) }); }
}

void Cutscene::drawUi(float uiW) {
    const SpriteLib* hud = spriteLib("v2/assets/images/hud/hud.xml");
    if (!sayKey.empty()) {
        const std::string msg = text(sayKey);
        if (speaker.empty()) {
            // Título del capítulo: cartel rojo como los de los carteles de ayuda.
            const float bx = uiW * 0.5f, by = 70;
            drawSprite(hud->get("cartelDialogos"), bx, by);
            drawTextBox(msg, bx, by + 23, 470, 44);
        } else {
            // Diálogo: cartel con la cara del que habla a la izquierda (dialogBanner*).
            const float bx = uiW * 0.5f + 24, by = 8;
            drawSprite(hud->get("dialogBanner"), bx, by);
            drawSprite(hud->get(speaker), bx - 218 + 20, by + 20);
            const float cx = bx + 10;
            drawTextBox(msg, cx, by + 24, 400, 48);
        }
    }
    const SpriteLib* skip = spriteLib("v2/assets/gui/menu/skip.xml");
    drawSprite(skip->get("skip"), uiW - 130, VIEW_H - 18);
}

}
