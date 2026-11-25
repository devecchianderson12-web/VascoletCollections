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

int LOGICAL_W = 1216, LOGICAL_H = 912;
int UI_W = 1216;   // ancho de la interfaz (HUD, pausa, menús) en la resolución sin deformar

namespace {

// Las imágenes de pantalla completa del juego original son 1200x900 dentro de una textura más grande.
const SDL_Rect FULL = { 0, 0, 1200, 900 };
const char* const INTRO_IMG[LEVELS] = { "intro_prehistoria", "intro_egipto", "intro_castillo", "intro_pirata", "intro_futuro" };

const float INTRO_LOADING = 0.6f;   // segundos con "Cargando..." antes de poder seguir
enum Scene { MENU, HELP, CREDITS, SELECT, STORY, INTRO, PLAY, END_WIN, END_LOSE };
// Menú del juego original (sobre menu.jpg, en píxeles de la imagen de 1200x900).
const char* const MENU_LBL[5] = { "Jugar", "Opciones", "Ayuda", "Créditos", "Salir" };
// Mismo diseño que el inicio de la colección: arte a la izquierda y botones a la derecha, con su
// misma separación (medidas del inicio, pensadas para 384 de alto).
struct MenuBtn { float cx, cy, w, h; };
MenuBtn menuButton(int i) {
    const float k = LOGICAL_H / 384.0f;
    const float w = std::min(230.0f, UI_W / k * 0.3f) * k;
    return { UI_W - w * 0.5f - 18 * k, LOGICAL_H * 0.5f + (i - 2) * 58 * k, w, 46 * k };
}
// Botones del cartel de pausa (alto 548, centrado): Jugar, Opciones, Salir.
const float PAUSE_BTN_Y[3] = { 62, 118, 174 };
// El cartel (548 de alto) ocupa el 92% del alto de la pantalla.
inline float pauseScale() { return LOGICAL_H * 0.86f / 548.0f; }

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

// Dónde van las pantallas de 1200x900 (menú, selección, historia...): estiradas a toda la pantalla
// (opción "Estirar pantalla", por defecto) o enteras en 4:3 con bordes rojos a los costados.
struct ContentFrame {
    float x = 0, y = 0, sx = 1, sy = 1;
    float X(float v) const { return x + v * sx; }
    float Y(float v) const { return y + v * sy; }
};
ContentFrame contentFrame() {
    ContentFrame f;
    if (vc::settings().stretch) { f.sx = UI_W / 1200.0f; f.sy = LOGICAL_H / 900.0f; return f; }
    const float s = std::min(UI_W / 1200.0f, LOGICAL_H / 900.0f);
    f.sx = f.sy = s; f.x = (UI_W - 1200 * s) * 0.5f; f.y = (LOGICAL_H - 900 * s) * 0.5f;
    return f;
}

SDL_FRect drawFull(const std::string& name) {
    plat::fillScreen({ 150, 10, 12, 255 });
    const ContentFrame f = contentFrame();
    const SDL_FRect d = { f.x, f.y, 1200 * f.sx, 900 * f.sy };
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
    g_cameraFixed = false;

    const int oldW = plat::SCREEN_W, oldH = plat::SCREEN_H;
    // Igual que el original: se ve siempre una pantalla entera del juego (16x12 bloques = 1216x912) y
    // la cámara salta de pantalla en pantalla. En el celular se estira a toda la pantalla ("Estirar
    // pantalla", por defecto) o se muestra en 4:3.
    LOGICAL_W = 1216; LOGICAL_H = 912;
    UI_W = std::max(1216, (int)std::lround(912.0f * oldW / std::max(1, oldH)));
    // Por defecto se trabaja en la resolución de la interfaz (sin deformar); solo el mundo se dibuja
    // en la pantalla 4:3 del original, estirada a lo ancho si está activado "Estirar pantalla".
    auto uiSpace = [&] { plat::setStretch(false); plat::setLogicalSize(st.stretch ? UI_W : LOGICAL_W, LOGICAL_H); };
    auto worldSpace = [&] { plat::setStretch(st.stretch != 0); plat::setLogicalSize(LOGICAL_W, LOGICAL_H); };
    auto menuSpace = [&] { plat::setStretch(false); plat::setLogicalSize(UI_W, LOGICAL_H); };
    menuSpace();
    plat::setView43(false, LOGICAL_W);
    plat::setAssetPrefix("v1/");
    static bool defsLoaded = false;
    if (!defsLoaded) defsLoaded = loadDefs();

    Scene scene = MENU;
    int menuSel = 0;
    bool backHot = false;
    int sel = 0, storyPage = 1, level = 0;
    bool quit = false, paused = false;
    int pauseSel = 0;
    float introT = 0;   // tiempo en la intro de la época ("Cargando..." y después "Pulsa cualquier tecla")
    Scene prevScene = scene;
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
            if (scene != prevScene) { prevScene = scene; introT = 0; }
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
                    playSfx("item.ogg");
                    if (menuSel == 0) {
                        // Primera vez: historia y directo a la Prehistoria; después, elegir época.
                        bool played = st.v1Unlocked > 1;
                        for (int c : st.v1Coins) played = played || c > 0;
                        if (played) { scene = SELECT; sel = 0; }
                        else { level = 0; carry = Counters(); scene = STORY; storyPage = 1; }
                    } else if (menuSel == 1) {
                        plat::setStretch(false); plat::setLogicalSize(oldW, oldH);
                        vc::runOptionsMenu();
                        menuSpace();
                        g_hdOff = (uint8_t)st.hdOff; g_cameraFixed = false;
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
                    // Solo se puede elegir hasta la última época habilitada.
                    const int open = std::max(1, std::min(LEVELS, st.v1Unlocked));
                    if (sel >= open) sel = open - 1;
                    if (plat::pressed(plat::BTN_RIGHT)) sel = (sel + 1) % open;
                    if (plat::pressed(plat::BTN_LEFT)) sel = (sel + open - 1) % open;
                    const ContentFrame f = contentFrame();
                    bool go = accept;
                    for (int i = 0; i < LEVELS; i++)
                        if (g_tap.in(f.X(EPOCH[i].x), f.Y(EPOCH[i].y), EPOCH[i].w * f.sx, EPOCH[i].h * f.sy)) {
                            if (i < open) { sel = i; go = true; }
                        }
                    { float hx, hy; backHot = plat::pointerHover(&hx, &hy) && hx >= f.X(534) && hx < f.X(666) && hy >= f.Y(835) && hy < f.Y(889); }
                    if (g_tap.in(f.X(534), f.Y(835), 132 * f.sx, 54 * f.sy) || plat::pressed(plat::BTN_BACK)) { scene = MENU; break; }
                    if (go) { level = sel; carry = Counters(); scene = level == 0 ? STORY : INTRO; storyPage = 1; playSfx("item.ogg"); }
                    break;
                }
                case STORY:   // historia del comienzo (4 páginas)
                    if (accept || g_tap.tapped || plat::pressed(plat::BTN_JUMP)) { if (++storyPage > 4) scene = INTRO; }
                    if (plat::pressed(plat::BTN_BACK)) scene = SELECT;
                    break;
                case INTRO:
                    introT += STEP;
                    if (introT > INTRO_LOADING && (accept || g_tap.tapped || plat::pressed(plat::BTN_JUMP))) {
                        delete world; world = new World;
                        if (!world->load(level, carry)) { delete world; world = nullptr; scene = SELECT; break; }
                        scene = PLAY; paused = false; acc = 0; last = plat::ticksMs();
                        uiSpace();
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
                        const float cx = (st.stretch ? UI_W : LOGICAL_W) * 0.5f, cy = LOGICAL_H * 0.5f;
                        bool act = plat::pressed(plat::BTN_ACCEPT);
                        const float k = pauseScale();
                        for (int i = 0; i < 3; i++) if (g_tap.in(cx - 110 * k, cy + (PAUSE_BTN_Y[i] - 30) * k, 220 * k, 60 * k)) { pauseSel = i; act = true; }
                        if (plat::pressed(plat::BTN_START) && !plat::pressed(plat::BTN_ACCEPT)) { pauseSel = 0; act = true; }
                        if (act && pauseSel == 1) {
                            plat::setStretch(false); plat::setLogicalSize(oldW, oldH);
                            vc::runOptionsMenu();
                            uiSpace();
                            g_hdOff = (uint8_t)st.hdOff;
                            g_cameraFixed = false;
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
                        if (level + 2 > st.v1Unlocked) st.v1Unlocked = std::min(LEVELS, level + 2);
                        st.v1Coins[level] = std::max(st.v1Coins[level], world->counters().coins);
                        st.save();
                        if (++level >= LEVELS) { scene = END_WIN; delete world; world = nullptr; plat::playMusic("assets/audio/music/andy.ogg"); }
                        else { scene = INTRO; delete world; world = nullptr; }
                    } else if (world->gameOver()) { scene = END_LOSE; delete world; world = nullptr; plat::stopMusic(); }
                    break;
                }
                case END_WIN: case END_LOSE:
                    // Como en el original: al terminar se ve la selección de épocas (con las monedas); al perder, el menú.
                    if (accept || g_tap.tapped || plat::pressed(plat::BTN_BACK)) { scene = scene == END_WIN ? SELECT : MENU; plat::playMusic("assets/audio/music/andy.ogg"); }
                    break;
            }
        }

        if (scene != PLAY) menuSpace();
        plat::beginFrame();
        switch (scene) {
            case MENU: {
                vc::drawTitleArt(menuButton(0).w);
                for (int i = 0; i < 5; i++) {
                    const MenuBtn b = menuButton(i);
                    vc::titleButton(MENU_LBL[i], b.cx, b.cy, b.w, b.h, menuSel == i);
                }
                break;
            }
            case HELP: drawFull("assets/lang/images/help/ayuda.jpg"); break;
            case CREDITS: drawFull("assets/lang/images/credits/creditos.jpg"); break;
            case SELECT: {
                drawFull("assets/lang/images/seleccionar_nivel/fondo_seleccionar_nivel.jpg");
                const ContentFrame f = contentFrame();
                const float s = f.sy;
                // Épocas habilitadas: panel de color con su personaje (boton_*.png); las bloqueadas quedan
                // como en el fondo (oscuras). La elegida usa el panel resaltado.
                static const char* const BTN[LEVELS] = { "prehistoria", "egipto", "castillo", "piratas", "futuro" };
                static const SDL_Rect CHAR[LEVELS] = { { 442, 18, 246, 522 }, { 442, 4, 230, 536 }, { 446, 4, 194, 616 }, { 442, 2, 252, 438 }, { 442, 8, 258, 392 } };
                const int open = std::max(1, std::min(LEVELS, st.v1Unlocked));
                for (int i = 0; i < open; i++) {
                    plat::Image* b = img(std::string("assets/lang/images/seleccionar_nivel/boton_") + BTN[i] + ".png");
                    if (!b) continue;
                    const SDL_FRect r = { f.X(EPOCH[i].x), f.Y(EPOCH[i].y), EPOCH[i].w * f.sx, EPOCH[i].h * f.sy };
                    plat::drawImage(b, i == sel ? SDL_Rect{ 224, 4, 212, 534 } : SDL_Rect{ 4, 4, 212, 534 }, r);
                    const SDL_Rect& c = CHAR[i];
                    const float cw = c.w * r.w / 212.0f, ch = c.h * r.h / 534.0f;
                    plat::drawImage(b, c, SDL_FRect{ r.x + r.w * 0.5f - cw * 0.5f, r.y + r.h - ch, cw, ch });
                }
                // Monedas juntadas en cada época habilitada ("N / 100") y el botón "Atrás", como en el original.
                // Debajo del año de cada época: "N / 100" y el ícono de la moneda.
                const float ts = 27 * s / vc::uiFont().height(1.0f);
                const ImageMap* coin = imageMap("COIN", "AssetsItems");
                for (int i = 0; i < open; i++) {
                    char cb[16]; snprintf(cb, sizeof cb, "%d / 100", st.v1Coins[i]);
                    const float cx = f.X(EPOCH[i].x + EPOCH[i].w * 0.5f - 10), tw = vc::uiFont().width(cb, ts);
                    text(cb, cx, f.Y(807) - vc::uiFont().height(ts) * 0.5f, ts);
                    if (coin && !coin->frames.empty()) drawFrameScaled(coin->frames[0], cx + tw * 0.5f + 20 * s, f.Y(822), s * 0.36f);
                }
                if (const ImageMap* sb = imageMap("SMALL_BUTTON", "AssetsGeneral"); sb && sb->frames.size() >= 2)
                    drawFrameScaled(sb->frames[backHot ? 1 : 0], f.X(600), f.Y(862), s * 0.95f);
                text("Atrás", f.X(600), f.Y(862) - vc::uiFont().height(ts) * 0.5f, ts);
                break;
            }
            case STORY: drawFull("assets/lang/images/intros/intro_page" + std::to_string(storyPage) + ".jpg"); break;
            case INTRO: {
                const SDL_FRect d = drawFull(std::string("assets/lang/images/intros/") + INTRO_IMG[level] + ".jpg");
                if (introT > INTRO_LOADING) if (plat::Image* pk = img("assets/lang/images/menu/press_any_key.png")) {
                    // Tapa el "Cargando..." de la imagen.
                    const float sx = d.w / 1200.0f, sy = d.h / 900.0f;
                    plat::drawImage(pk, SDL_Rect{ 0, 0, pk->w, pk->h }, SDL_FRect{ d.x + 80 * sx, d.y + 773 * sy, pk->w * sx, pk->h * sy });
                }
                break;
            }
            case PLAY:
                if (world) {
                    world->setInterp(plat::highFps() && !paused ? acc / STEP : 1.0f);
                    worldSpace();
                    world->draw();
                    uiSpace();
                    world->drawHud();
                } else uiSpace();
                if (paused) {
                    const float cx = (st.stretch ? UI_W : LOGICAL_W) * 0.5f, cy = LOGICAL_H * 0.5f;
                    // Cartel de pausa del original (emblema + "Pausa") con sus botones rojos.
                    const ImageMap* pd = imageMap("PAUSE_DIALOG", "AssetsGeneral");
                    const ImageMap* mb = imageMap("MENU_BUTTON", "AssetsGeneral");
                    const float k = pauseScale();
                    if (pd && !pd->frames.empty()) drawFrameScaled(pd->frames[0], cx, cy, k);
                    static const char* const LBL[3] = { "Jugar", "Opciones", "Salir" };
                    for (int i = 0; i < 3; i++) {
                        const float by = cy + PAUSE_BTN_Y[i] * k;
                        if (mb && mb->frames.size() >= 2) drawFrameScaled(mb->frames[pauseSel == i ? 1 : 0], cx, by, 0.85f * k);
                        text(LBL[i], cx, by - vc::uiFont().height(0.62f * k) * 0.5f, 0.62f * k);
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
    plat::setStretch(false);
    plat::setLogicalSize(oldW, oldH);
}

}
