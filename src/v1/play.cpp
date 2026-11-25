// Vascolet 1: pantallas (elegir época, presentación, juego, pausa, final) y bucle principal.
#include "game.h"
#include "world.h"
#include "lighting.h"
#include "../launcher/font.h"
#include "../launcher/settings.h"
#include "../launcher/launcher.h"
#include "../platform.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace v1 {

int LOGICAL_W = 800, LOGICAL_H = 600;

namespace {

// Las imágenes de pantalla completa del juego original son 1200x900 dentro de una textura más grande.
const SDL_Rect FULL = { 0, 0, 1200, 900 };
const char* const INTRO_IMG[LEVELS] = { "intro_prehistoria", "intro_egipto", "intro_castillo", "intro_pirata", "intro_futuro" };

enum Scene { SELECT, STORY, INTRO, PLAY, END_WIN, END_LOSE };
// Botones del cartel de pausa (alto 548, centrado): Jugar, Opciones, Salir.
const float PAUSE_BTN_Y[3] = { 62, 118, 174 };

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

// Dibuja una imagen 4:3 entera, centrada, con bordes rojos a los costados. Devuelve dónde quedó.
SDL_FRect drawFull(const std::string& name) {
    plat::fillScreen({ 150, 10, 12, 255 });
    const float s = std::min(LOGICAL_W / (float)FULL.w, LOGICAL_H / (float)FULL.h);
    const SDL_FRect d = { (LOGICAL_W - FULL.w * s) * 0.5f, (LOGICAL_H - FULL.h * s) * 0.5f, FULL.w * s, FULL.h * s };
    if (plat::Image* i = img(name)) plat::drawImage(i, FULL, d);
    return d;
}

void text(const std::string& s, float cx, float y, float sc, SDL_Color c = { 255, 255, 255, 255 }) { vc::uiFont().drawCentered(s, cx, y, sc, c); }

void button(const std::string& s, float cx, float cy, float w, float h, bool hot) {
    plat::Image* b = img("v1/assets/images/menuelements/buttons_tile.png");
    const SDL_Rect src = hot ? SDL_Rect{ 258, 11, 232, 53 } : SDL_Rect{ 8, 11, 232, 53 };
    const float x = cx - w * 0.5f, y = cy - h * 0.5f, cap = 26.0f * h / src.h;
    if (b) {
        plat::drawImage(b, SDL_Rect{ src.x, src.y, 26, src.h }, SDL_FRect{ x, y, cap, h });
        plat::drawImage(b, SDL_Rect{ src.x + 26, src.y, src.w - 52, src.h }, SDL_FRect{ x + cap, y, w - 2 * cap, h });
        plat::drawImage(b, SDL_Rect{ src.x + src.w - 26, src.y, 26, src.h }, SDL_FRect{ x + w - cap, y, cap, h });
    }
    const float sc = h / 40.0f * 0.75f;
    text(s, cx, cy - vc::uiFont().height(sc) * 0.5f, sc);
}

// Zonas de las 5 épocas en fondo_seleccionar_nivel.jpg (en píxeles de la imagen de 1200x900).
const SDL_FRect EPOCH[LEVELS] = { { 48, 200, 200, 540 }, { 272, 200, 200, 540 }, { 496, 200, 200, 540 }, { 720, 200, 200, 540 }, { 944, 200, 200, 540 } };
// El orden de los paneles del fondo es: prehistoria, egipto, castillo, piratas, futuro.

}

void run() {
    vc::Settings& st = vc::settings();
    g_hdOff = (uint8_t)st.hdOff;
    g_cameraFixed = st.camFixed != 0;

    const int oldW = plat::SCREEN_W, oldH = plat::SCREEN_H;
    LOGICAL_H = 600;
    LOGICAL_W = std::max(800, (int)std::lround(600.0f * oldW / std::max(1, oldH)));
    plat::setLogicalSize(LOGICAL_W, LOGICAL_H);
    plat::setView43(false, LOGICAL_W);
    plat::setAssetPrefix("v1/");
    static bool defsLoaded = false;
    if (!defsLoaded) defsLoaded = loadDefs();

    Scene scene = SELECT;
    int sel = 0, storyPage = 1, level = 0;
    bool quit = false, paused = false;
    int pauseSel = 0;
    Counters carry;
    World* world = nullptr;
    plat::playMusic("assets/audio/music/andy.ogg");

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
            switch (scene) {
                case SELECT: {
                    // Solo se puede elegir hasta la última época habilitada.
                    const int open = std::max(1, std::min(LEVELS, st.v1Unlocked));
                    if (sel >= open) sel = open - 1;
                    if (plat::pressed(plat::BTN_RIGHT)) sel = (sel + 1) % open;
                    if (plat::pressed(plat::BTN_LEFT)) sel = (sel + open - 1) % open;
                    const float s = std::min(LOGICAL_W / 1200.0f, LOGICAL_H / 900.0f), ox = (LOGICAL_W - 1200 * s) * 0.5f, oy = (LOGICAL_H - 900 * s) * 0.5f;
                    bool go = accept;
                    for (int i = 0; i < LEVELS; i++)
                        if (g_tap.in(ox + EPOCH[i].x * s, oy + EPOCH[i].y * s, EPOCH[i].w * s, EPOCH[i].h * s)) {
                            if (i < open) { sel = i; go = true; }
                        }
                    if (g_tap.in(10, LOGICAL_H - 70.0f, 200, 60) || plat::pressed(plat::BTN_BACK)) { quit = true; break; }
                    if (go) { level = sel; carry = Counters(); scene = level == 0 ? STORY : INTRO; storyPage = 1; playSfx("item.ogg"); }
                    break;
                }
                case STORY:   // historia del comienzo (4 páginas)
                    if (accept || g_tap.tapped || plat::pressed(plat::BTN_JUMP)) { if (++storyPage > 4) scene = INTRO; }
                    if (plat::pressed(plat::BTN_BACK)) scene = SELECT;
                    break;
                case INTRO:
                    if (accept || g_tap.tapped || plat::pressed(plat::BTN_JUMP)) {
                        delete world; world = new World;
                        if (!world->load(level, carry)) { delete world; world = nullptr; scene = SELECT; break; }
                        scene = PLAY; paused = false; acc = 0; last = plat::ticksMs();
                    }
                    if (plat::pressed(plat::BTN_BACK)) scene = SELECT;
                    break;
                case PLAY: {
                    if (!world) { scene = SELECT; break; }
                    if (!paused && (plat::pressed(plat::BTN_START) || plat::pressed(plat::BTN_BACK))) { paused = true; pauseSel = 0; plat::pauseMusic(true); break; }
                    if (paused) {
                        // Pausa: Continuar, Opciones (las de la colección) y Salir.
                        if (plat::pressed(plat::BTN_DOWN)) pauseSel = (pauseSel + 1) % 3;
                        if (plat::pressed(plat::BTN_UP)) pauseSel = (pauseSel + 2) % 3;
                        const float cx = LOGICAL_W * 0.5f, cy = LOGICAL_H * 0.5f;
                        bool act = plat::pressed(plat::BTN_ACCEPT);
                        for (int i = 0; i < 3; i++) if (g_tap.in(cx - 110, cy + PAUSE_BTN_Y[i] - 30, 220, 60)) { pauseSel = i; act = true; }
                        if (plat::pressed(plat::BTN_START) && !plat::pressed(plat::BTN_ACCEPT)) { pauseSel = 0; act = true; }
                        if (act && pauseSel == 1) {
                            plat::setLogicalSize(oldW, oldH);
                            vc::runOptionsMenu();
                            plat::setLogicalSize(LOGICAL_W, LOGICAL_H);
                            g_hdOff = (uint8_t)st.hdOff;
                            g_cameraFixed = st.camFixed != 0;
                            acc = 0; last = plat::ticksMs();
                            break;
                        }
                        if (act) {
                            paused = false; plat::pauseMusic(false);
                            if (pauseSel == 2) { delete world; world = nullptr; scene = SELECT; plat::playMusic("assets/audio/music/andy.ogg"); }
                        }
                        break;
                    }
                    world->update(STEP);
                    if (world->won()) {
                        carry = world->counters();
                        if (level + 2 > st.v1Unlocked) { st.v1Unlocked = std::min(LEVELS, level + 2); st.save(); }
                        if (++level >= LEVELS) { scene = END_WIN; delete world; world = nullptr; plat::playMusic("assets/audio/music/andy.ogg"); }
                        else { scene = INTRO; delete world; world = nullptr; }
                    } else if (world->gameOver()) { scene = END_LOSE; delete world; world = nullptr; plat::stopMusic(); }
                    break;
                }
                case END_WIN: case END_LOSE:
                    if (accept || g_tap.tapped || plat::pressed(plat::BTN_BACK)) { scene = SELECT; plat::playMusic("assets/audio/music/andy.ogg"); }
                    break;
            }
        }

        plat::beginFrame();
        plat::setView43(false, LOGICAL_W);
        switch (scene) {
            case SELECT: {
                const SDL_FRect d = drawFull("assets/lang/images/seleccionar_nivel/fondo_seleccionar_nivel.jpg");
                const float s = d.w / 1200.0f;
                // Épocas habilitadas: panel de color con su personaje (boton_*.png); las bloqueadas quedan
                // como en el fondo (oscuras). La elegida usa el panel resaltado.
                static const char* const BTN[LEVELS] = { "prehistoria", "egipto", "castillo", "piratas", "futuro" };
                static const SDL_Rect CHAR[LEVELS] = { { 442, 18, 246, 522 }, { 442, 4, 230, 536 }, { 446, 4, 194, 616 }, { 442, 2, 252, 438 }, { 442, 8, 258, 392 } };
                const int open = std::max(1, std::min(LEVELS, st.v1Unlocked));
                for (int i = 0; i < open; i++) {
                    plat::Image* b = img(std::string("assets/lang/images/seleccionar_nivel/boton_") + BTN[i] + ".png");
                    if (!b) continue;
                    const SDL_FRect r = { d.x + EPOCH[i].x * s, d.y + EPOCH[i].y * s, EPOCH[i].w * s, EPOCH[i].h * s };
                    plat::drawImage(b, i == sel ? SDL_Rect{ 224, 4, 212, 534 } : SDL_Rect{ 4, 4, 212, 534 }, r);
                    const SDL_Rect& c = CHAR[i];
                    const float k = r.w / 212.0f, cw = c.w * k, ch = c.h * k;
                    plat::drawImage(b, c, SDL_FRect{ r.x + r.w * 0.5f - cw * 0.5f, r.y + r.h - ch, cw, ch });
                }
                button("Volver", 110, LOGICAL_H - 40.0f, 190, 52, false);
                break;
            }
            case STORY: drawFull("assets/lang/images/intros/intro_page" + std::to_string(storyPage) + ".jpg"); break;
            case INTRO: drawFull(std::string("assets/lang/images/intros/") + INTRO_IMG[level] + ".jpg"); break;
            case PLAY:
                if (world) { world->setInterp(plat::highFps() && !paused ? acc / STEP : 1.0f); world->draw(); }
                if (paused) {
                    const float cx = LOGICAL_W * 0.5f, cy = LOGICAL_H * 0.5f;
                    // Cartel de pausa del original (emblema + "Pausa") con sus botones rojos.
                    const ImageMap* pd = imageMap("PAUSE_DIALOG", "AssetsGeneral");
                    const ImageMap* mb = imageMap("MENU_BUTTON", "AssetsGeneral");
                    if (pd && !pd->frames.empty()) drawFrame(pd->frames[0], cx, cy);
                    static const char* const LBL[3] = { "Jugar", "Opciones", "Salir" };
                    for (int i = 0; i < 3; i++) {
                        const float by = cy + PAUSE_BTN_Y[i];
                        if (mb && mb->frames.size() >= 2) drawFrameScaled(mb->frames[pauseSel == i ? 1 : 0], cx, by, 0.85f);
                        text(LBL[i], cx, by - vc::uiFont().height(0.62f) * 0.5f, 0.62f);
                    }
                }
                break;
            case END_WIN: drawFull("assets/lang/images/intros/fin.jpg"); break;
            case END_LOSE: drawFull("assets/lang/images/intros/perdiste.jpg"); break;
        }
        plat::endFrame();
    }

    delete world;
    plat::stopMusic();
    plat::setTouchOverlay(false);
    plat::setAssetPrefix("");
    plat::setLogicalSize(oldW, oldH);
}

}
