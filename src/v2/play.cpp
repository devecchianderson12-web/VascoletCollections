// Vascolet 2: pantallas (menú, selección de capítulo, carga, juego, pausa, fin) y bucle principal.
#include "game.h"
#include "world.h"
#include "cutscene.h"
#include "../launcher/font.h"
#include "../launcher/settings.h"
#include "../launcher/launcher.h"
#include "../platform.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace v2 {

namespace {

int UI_W = 512;
const int UI_H = VIEW_H;

enum Scene { MENU, HELP, CREDITS, SELECT, CUTSCENE, LOADING, PLAY, GAMEOVER, ENDING };
const char* const MENU_LBL[5] = { "Jugar", "Opciones", "Ayuda", "Créditos", "Salir" };
const char* const CHAPTER[5] = { "Capítulo I: El Bosque Encantado", "Capítulo II: El Lago del Monstruo", "Capítulo III: El Mundo de los Dragones",
                                 "Capítulo IV: La Montaña del Mal", "Capítulo V: El Castillo de Malmark" };

// Mismo diseño que el inicio de la colección y que Vascolet 1 (medidas pensadas para 384 de alto).
struct MenuBtn { float cx, cy, w, h; };
MenuBtn menuButton(int i) {
    const float w = std::min(230.0f, UI_W * 0.3f);
    return { UI_W - w * 0.5f - 18, UI_H * 0.5f + (i - 2) * 58, w, 46 };
}

struct Tap {
    bool down = false, was = false, tapped = false; float sx = 0, sy = 0, tx = 0, ty = 0, move = 0;
    void poll() {
        tapped = false; float x, y;
        down = plat::pointerDown(&x, &y);
        if (down) { if (!was) { sx = x; sy = y; move = 0; } move = std::max(move, std::hypot(x - sx, y - sy)); }
        else if (was && move < 30) { tapped = true; tx = sx; ty = sy; }
        was = down;
    }
    bool in(float x, float y, float w, float h) const { return tapped && tx >= x && tx < x + w && ty >= y && ty < y + h; }
} g_tap;

plat::Image* img(const std::string& name) {
    static std::vector<std::pair<std::string, plat::Image*>> cache;
    for (auto& c : cache) if (c.first == name) return c.second;
    plat::Image* i = plat::loadImage(name);
    cache.push_back({ name, i });
    return i;
}

// Pantallas de 512x384 del original: estiradas a toda la pantalla ("Estirar pantalla") o en 4:3.
struct Frame43 { float x = 0, y = 0, sx = 1, sy = 1; float X(float v) const { return x + v * sx; } float Y(float v) const { return y + v * sy; } };
Frame43 frame43() {
    Frame43 f;
    if (vc::settings().stretch) { f.sx = UI_W / 512.0f; f.sy = UI_H / 384.0f; return f; }
    const float s = std::min(UI_W / 512.0f, UI_H / 384.0f);
    f.sx = f.sy = s; f.x = (UI_W - 512 * s) * 0.5f; f.y = (UI_H - 384 * s) * 0.5f;
    return f;
}
void drawFull(const std::string& name) {
    plat::fillScreen({ 150, 10, 12, 255 });
    const Frame43 f = frame43();
    if (plat::Image* i = img(name)) plat::drawImage(i, SDL_Rect{ 0, 0, 512, 384 }, SDL_FRect{ f.x, f.y, 512 * f.sx, 384 * f.sy });
}

void centerText(const std::string& s, float cx, float y, float sc, SDL_Color c = { 255, 255, 255, 255 }) {
    drawText(s, cx - textWidth(s, sc) * 0.5f, y, sc, c);
}

// Botones rojos del original (gui/menu/menu.xml): "jugarUp"/"jugarOver"...
void guiButton(const char* key, float cx, float cy, bool over, float scale = 1.0f) {
    const SpriteLib* m = spriteLib("v2/assets/gui/menu/menu.xml");
    drawSprite(m->get(std::string(key) + (over ? "Over" : "Up")), cx, cy, false, 255, scale);
}

// Paneles de la selección de capítulo (gui/level/background.jpg, en píxeles de 512x384).
const float PANEL_X[5] = { 61, 158, 256, 353, 450 };
const float PANEL_Y = 197, PANEL_W = 92, PANEL_H = 232;

}

void run() {
    vc::Settings& st = vc::settings();
    const int oldW = plat::SCREEN_W, oldH = plat::SCREEN_H;
    UI_W = std::max(512, (int)std::lround(384.0f * oldW / std::max(1, oldH)));
    auto uiSpace = [&] { plat::setStretch(false); plat::setLogicalSize(UI_W, UI_H); };
    auto worldSpace = [&] {
        if (st.stretch) { plat::setStretch(true); plat::setLogicalSize(VIEW_W, VIEW_H); }
        else { plat::setStretch(false); plat::setLogicalSize(VIEW_W, VIEW_H); }
    };
    uiSpace();
    plat::setView43(false, UI_W);
    plat::setAssetPrefix("v2/");
    plat::setTouchShoot(true);

    Scene scene = MENU, prevScene = MENU;
    int menuSel = 0, sel = 0, level = 1, pauseSel = 0;
    bool quit = false, paused = false;
    float sceneT = 0;
    Progress carry;
    World* world = nullptr;
    Cutscene* cut = nullptr;
    int cutNum = 1;
    // Cada capítulo empieza con su escena (que tiene el título); la 6 es el final.
    auto startCut = [&](int n) { delete cut; cut = new Cutscene; cutNum = n; if (!cut->load(n)) { delete cut; cut = nullptr; } scene = CUTSCENE; };
    music("menu.ogg");
#ifdef HOST
    if (const char* l = getenv("V2_LEVEL")) { level = std::max(1, std::min(5, atoi(l))); scene = LOADING; }
    if (const char* c = getenv("V2_CUT")) { level = std::max(1, std::min(5, atoi(c))); startCut(atoi(c)); }
#endif

    const float STEP = 1.0f / 60.0f;
    float acc = 0;
    uint32_t last = plat::ticksMs();
    while (!quit && !plat::quitRequested()) {
        const uint32_t now = plat::ticksMs();
        acc = std::min(0.25f, acc + (now - last) / 1000.0f); last = now;
        while (acc >= STEP && !quit) {
            acc -= STEP;
            plat::inputPoll();
            g_tap.poll();
            plat::setTouchOverlay(scene == PLAY && !paused);
            const bool accept = plat::pressed(plat::BTN_ACCEPT) || plat::pressed(plat::BTN_START);
            if (scene != prevScene) { prevScene = scene; sceneT = 0; }
            sceneT += STEP;
            switch (scene) {
                case MENU: {
                    if (plat::pressed(plat::BTN_DOWN)) menuSel = (menuSel + 1) % 5;
                    if (plat::pressed(plat::BTN_UP)) menuSel = (menuSel + 4) % 5;
                    bool go = accept;
                    for (int i = 0; i < 5; i++) {
                        const MenuBtn b = menuButton(i);
                        if (g_tap.in(b.cx - b.w * 0.5f, b.cy - b.h * 0.5f, b.w, b.h)) { menuSel = i; go = true; }
                    }
                    if (plat::pressed(plat::BTN_BACK)) { quit = true; break; }
                    if (!go) break;
                    sfx("coin");
                    if (menuSel == 0) {
                        bool played = st.v2Unlocked > 1;
                        for (int c : st.v2Pct) played = played || c > 0;
                        if (played) { scene = SELECT; sel = std::max(0, std::min(st.v2Unlocked - 1, 4)); }
                        else { level = 1; carry = Progress(); startCut(1); }
                    } else if (menuSel == 1) {
                        plat::setStretch(false); plat::setLogicalSize(oldW, oldH);
                        vc::runOptionsMenu();
                        uiSpace();
                        acc = 0; last = plat::ticksMs();
                    } else if (menuSel == 2) scene = HELP;
                    else if (menuSel == 3) scene = CREDITS;
                    else quit = true;
                    break;
                }
                case HELP: case CREDITS:
                    if (accept || g_tap.tapped || plat::pressed(plat::BTN_BACK) || plat::pressed(plat::BTN_JUMP)) scene = MENU;
                    break;
                case SELECT: {
                    const int open = std::max(1, std::min(5, st.v2Unlocked));
                    if (sel >= open) sel = open - 1;
                    if (plat::pressed(plat::BTN_RIGHT)) sel = (sel + 1) % open;
                    if (plat::pressed(plat::BTN_LEFT)) sel = (sel + open - 1) % open;
                    const Frame43 f = frame43();
                    bool go = accept;
                    for (int i = 0; i < open; i++)
                        if (g_tap.in(f.X(PANEL_X[i] - PANEL_W * 0.5f), f.Y(PANEL_Y - PANEL_H * 0.5f), PANEL_W * f.sx, PANEL_H * f.sy)) { sel = i; go = true; }
                    if (g_tap.in(f.X(410), f.Y(340), 100 * f.sx, 40 * f.sy) || plat::pressed(plat::BTN_BACK)) { scene = MENU; break; }
                    if (go) { level = sel + 1; carry = Progress(); sfx("coin"); startCut(level); }
                    break;
                }
                case CUTSCENE: {
                    if (!cut) { scene = cutNum >= 6 ? ENDING : LOADING; break; }
                    cut->update(STEP);
                    const float bx = UI_W - 250, by = UI_H - 34;
                    const bool skip = plat::pressed(plat::BTN_START) || plat::pressed(plat::BTN_ACCEPT) && !plat::pressed(plat::BTN_JUMP) ||
                                      (plat::pressed(plat::BTN_BACK) && !plat::held(plat::BTN_SHOOT)) || g_tap.in(bx, by, 250, 34);
                    if (skip || cut->done()) {
                        delete cut; cut = nullptr;
                        if (cutNum >= 6) { scene = ENDING; music("win_game.ogg"); }
                        else scene = LOADING;
                    }
                    break;
                }
                case LOADING:
                    if (sceneT > 0.05f && !world) {
                        world = new World;
                        if (!world->load(level, carry)) { delete world; world = nullptr; scene = MENU; music("menu.ogg"); break; }
                    }
                    if (world && sceneT > 1.2f) { scene = PLAY; paused = false; acc = 0; last = plat::ticksMs(); }
                    break;
                case PLAY: {
                    if (!world) { scene = MENU; break; }
                    if (!paused && (plat::pressed(plat::BTN_START) || (plat::pressed(plat::BTN_BACK) && !plat::held(plat::BTN_SHOOT)))) { paused = true; pauseSel = 0; plat::pauseMusic(true); break; }
                    if (paused) {
                        if (plat::pressed(plat::BTN_DOWN)) pauseSel = (pauseSel + 1) % 3;
                        if (plat::pressed(plat::BTN_UP)) pauseSel = (pauseSel + 2) % 3;
                        bool act = plat::pressed(plat::BTN_ACCEPT);
                        const float cx = UI_W * 0.5f, cy = UI_H * 0.5f;
                        for (int i = 0; i < 3; i++) if (g_tap.in(cx - 60, cy + i * 40 - 15, 120, 30)) { pauseSel = i; act = true; }
                        if (plat::pressed(plat::BTN_START) && !plat::pressed(plat::BTN_ACCEPT)) { pauseSel = 0; act = true; }
                        if (act && pauseSel == 1) {
                            plat::beginFrame();
                            worldSpace(); world->draw(); uiSpace(); world->drawHud((float)UI_W);
                            plat::captureBackdrop();
                            plat::setStretch(false); plat::setLogicalSize(oldW, oldH);
                            vc::runOptionsMenu();
                            plat::clearBackdrop();
                            uiSpace();
                            acc = 0; last = plat::ticksMs();
                            break;
                        }
                        if (act) {
                            paused = false; plat::pauseMusic(false);
                            if (pauseSel == 2) { delete world; world = nullptr; scene = MENU; music("menu.ogg"); }
                        }
                        break;
                    }
                    world->update(STEP);
                    if (world->won()) {
                        carry = world->progress();
                        const int pct = world->totalCoins() > 0 ? world->coins() * 100 / world->totalCoins() : 100;
                        st.v2Pct[level - 1] = std::max(st.v2Pct[level - 1], pct);
                        if (level + 1 > st.v2Unlocked) st.v2Unlocked = std::min(5, level + 1);
                        st.save();
                        delete world; world = nullptr;
                        carry.coins = 0;
                        ++level;
                        startCut(std::min(6, level));
                    } else if (world->gameOver()) {
                        delete world; world = nullptr; scene = GAMEOVER;
                    }
                    break;
                }
                case GAMEOVER: case ENDING:
                    if (sceneT > 1.0f && (accept || g_tap.tapped || plat::pressed(plat::BTN_BACK) || plat::pressed(plat::BTN_JUMP))) {
                        scene = scene == ENDING ? SELECT : MENU; music("menu.ogg");
                    }
                    break;
            }
        }

        if (scene != PLAY) uiSpace();
        plat::beginFrame();
        switch (scene) {
            case MENU: {
                // Arte del menú del original a la izquierda y los botones como en el inicio de la colección.
                plat::fillScreen({ 150, 10, 12, 255 });
                const float bw = menuButton(0).w;
                const float area = UI_W - bw - 36;
                const float s = std::max(area / 512.0f, UI_H / 352.0f);
                if (plat::Image* bg = img("v2/assets/gui/menu/background.jpg"))
                    plat::drawImage(bg, SDL_Rect{ 0, 0, 512, 352 }, SDL_FRect{ std::min(0.0f, (area - 512 * s) * 0.5f), (UI_H - 352 * s) * 0.5f, 512 * s, 352 * s });
                for (int i = 0; i < 5; i++) {
                    const MenuBtn b = menuButton(i);
                    vc::titleButton(MENU_LBL[i], b.cx, b.cy, b.w, b.h, menuSel == i);
                }
                break;
            }
            case HELP: drawFull("v2/assets/gui/help/help.jpg"); break;
            case CREDITS: drawFull("v2/assets/gui/credits/credits.jpg"); break;
            case SELECT: {
                drawFull("v2/assets/gui/level/background.jpg");
                const Frame43 f = frame43();
                const SpriteLib* lv = spriteLib("v2/assets/gui/level/level.xml");
                const int open = std::max(1, std::min(5, st.v2Unlocked));
                for (int i = 0; i < open; i++) {
                    const Sprite* p = lv->get("level" + std::to_string(i + 1) + (i == sel ? "Over" : "Up"));
                    if (p && p->img) plat::drawImage(p->img, p->r, SDL_FRect{ f.X(PANEL_X[i] - p->ax), f.Y(PANEL_Y - p->ay), p->r.w * f.sx, p->r.h * f.sy });
                }
                for (int i = 0; i < 5; i++) {
                    char b[16]; snprintf(b, sizeof b, "%d%%", st.v2Pct[i]);
                    const float sc = 0.36f * f.sy;
                    drawText(b, f.X(PANEL_X[i] + 12) - textWidth(b, sc) * 0.5f, f.Y(328) - textHeight(sc) * 0.5f, sc, { 40, 40, 40, 255 });
                }
                if (const Sprite* v = lv->get("volverUp"); v && v->img)
                    plat::drawImage(v->img, v->r, SDL_FRect{ f.X(460 - v->ax), f.Y(362 - v->ay), v->r.w * f.sx, v->r.h * f.sy });
                break;
            }
            case CUTSCENE:
                if (cut) {
                    worldSpace(); cut->draw();
                    uiSpace(); cut->drawUi((float)UI_W);
                }
                break;
            case LOADING: {
                drawFull("v2/assets/images/transition.jpg");
                plat::dimScreen(0.35f);
                centerText(CHAPTER[std::max(0, std::min(4, level - 1))], UI_W * 0.5f, UI_H * 0.5f - 20, 0.6f, { 255, 220, 60, 255 });
                centerText("Cargando...", UI_W * 0.5f, UI_H * 0.5f + 20, 0.45f);
                break;
            }
            case PLAY:
                if (world) {
                    world->setInterp(plat::highFps() && !paused ? acc / STEP : 1.0f);
                    worldSpace();
                    world->draw();
                    uiSpace();
                    world->drawHud((float)UI_W);
                } else uiSpace();
                if (paused) {
                    plat::dimScreen(0.4f);
                    const float cx = UI_W * 0.5f, cy = UI_H * 0.5f;
                    const SpriteLib* q = spriteLib("v2/assets/gui/hud/quit-confirmation.xml");
                    drawSprite(q->get("pause"), cx, cy);
                    static const char* const K[3] = { "continuar", "opciones", "salir" };
                    for (int i = 0; i < 3; i++) guiButton(K[i], cx, cy + i * 40, pauseSel == i);
                }
                break;
            case GAMEOVER: drawFull("v2/assets/gui/gameover/gameover.png"); break;
            case ENDING: drawFull("v2/assets/gui/win/win.png"); break;
        }
        plat::endFrame();
    }

    delete world;
    delete cut;
    plat::stopMusic();
    plat::setTouchOverlay(false);
    plat::setTouchShoot(false);
    plat::setAssetPrefix("");
    plat::setStretch(false);
    plat::setLogicalSize(oldW, oldH);
}

}
