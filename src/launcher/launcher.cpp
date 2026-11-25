#include "launcher.h"
#include "font.h"
#include "settings.h"
#include "../v1/game.h"
#include "../platform.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace vc {

namespace {

// ---- Estética de "La Máquina del Tiempo": rayos rojos, emblema, botones rojos con borde dorado ----
const SDL_Color WHITE = { 255, 255, 255, 255 }, YELLOW = { 255, 222, 60, 255 };
const float TXT = 0.56f;          // escala de la letra en botones y filas (la fuente mide 40 px)

enum Page { HOME, OPTIONS, GRAPHICS, ADVANCED, CONTROLS, SOUND, MAPPING, GAME };

struct Row {
    std::string label;
    std::function<std::string()> value;   // texto a la derecha (nullptr = abre un submenú)
    std::function<void(int)> change;      // d = +1 / -1 (tocar o aceptar = +1)
};

float W() { return (float)plat::SCREEN_W; }
float H() { return (float)plat::SCREEN_H; }

Mix_Chunk* g_click = nullptr;
void click() { plat::playSound(g_click); }

plat::Image* img(const char* name) {
    static std::vector<std::pair<std::string, plat::Image*>> cache;
    for (auto& c : cache) if (c.first == name) return c.second;
    plat::Image* i = plat::loadImage(name);
    cache.push_back({ name, i });
    return i;
}

// ---- Toques: se detectan acá (apoyar y soltar sin arrastrar) en coordenadas de pantalla completa ----
struct Pointer {
    bool down = false, wasDown = false, tapped = false;
    float x = 0, y = 0, sx = 0, sy = 0, tx = 0, ty = 0, maxMove = 0;
    void poll() {
        tapped = false;
        float px, py;
        down = plat::pointerDown(&px, &py);
        if (down) {
            if (!wasDown) { sx = px; sy = py; maxMove = 0; }
            x = px; y = py;
            maxMove = std::max(maxMove, std::hypot(px - sx, py - sy));
        } else if (wasDown && maxMove < 24) { tapped = true; tx = sx; ty = sy; }
        wasDown = down;
    }
    bool tapIn(float rx, float ry, float rw, float rh) const { return tapped && tx >= rx && tx < rx + rw && ty >= ry && ty < ry + rh; }
    bool pressIn(float rx, float ry, float rw, float rh) const { return down && sx >= rx && sx < rx + rw && sy >= ry && sy < ry + rh; }
    bool hoverIn(float rx, float ry, float rw, float rh) const {
        float hx, hy;
        return plat::pointerHover(&hx, &hy) && hx >= rx && hx < rx + rw && hy >= ry && hy < ry + rh;
    }
} g_ptr;

// ---- Dibujo ----
Font& font() { return uiFont(); }
void text(const std::string& s, float x, float y, float sc = TXT, SDL_Color c = WHITE) { font().draw(s, x, y, sc, c); }
void textC(const std::string& s, float cx, float y, float sc = TXT, SDL_Color c = WHITE) { font().drawCentered(s, cx, y, sc, c); }

// Fondo de rayos rojos que giran despacio, con brillo naranja en el centro (como el menú original).
void rays(float cx, float cy, float dim = 0) {
    SDL_Renderer* r = plat::renderer();
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    plat::fillScreen({ 178, 12, 14, 255 });
    const float rot = plat::ticksMs() / 1000.0f * 0.05f, R = std::max(W(), H()) * 1.6f;
    const int N = 22;
    std::vector<SDL_Vertex> v;
    for (int i = 0; i < N; i += 2) {
        const float a0 = rot + i * 6.2831853f / N, a1 = rot + (i + 1) * 6.2831853f / N;
        const SDL_Color c = { 222, 40, 22, 255 }, e = { 205, 26, 18, 255 };
        v.push_back({ { cx, cy }, c, { 0, 0 } });
        v.push_back({ { cx + std::cos(a0) * R, cy + std::sin(a0) * R }, e, { 0, 0 } });
        v.push_back({ { cx + std::cos(a1) * R, cy + std::sin(a1) * R }, e, { 0, 0 } });
    }
    // Brillo suave en el centro.
    const int M = 72;
    for (int i = 0; i < M; i++) {
        const float a0 = i * 6.2831853f / M, a1 = (i + 1) * 6.2831853f / M, rr = H() * 0.62f;
        v.push_back({ { cx, cy }, { 255, 150, 40, 120 }, { 0, 0 } });
        v.push_back({ { cx + std::cos(a0) * rr, cy + std::sin(a0) * rr }, { 255, 90, 20, 0 }, { 0, 0 } });
        v.push_back({ { cx + std::cos(a1) * rr, cy + std::sin(a1) * rr }, { 255, 90, 20, 0 }, { 0, 0 } });
    }
    SDL_RenderGeometry(r, nullptr, v.data(), (int)v.size(), nullptr, 0);
    if (dim > 0) plat::dimScreen(dim);
}

// Botón rojo del juego (buttons_tile.png), estirado a lo ancho sin deformar las puntas.
void button(const std::string& s, float cx, float cy, float w, float h, bool hot, bool pressed = false) {
    plat::Image* b = img("v1/assets/images/menuelements/buttons_tile.png");
    const SDL_Rect src = hot ? SDL_Rect{ 258, 11, 232, 53 } : SDL_Rect{ 8, 11, 232, 53 };
    if (pressed) { w *= 0.96f; h *= 0.96f; }
    const float x = cx - w * 0.5f, y = cy - h * 0.5f, cap = 26.0f * h / src.h;
    if (b) {
        plat::drawImage(b, SDL_Rect{ src.x, src.y, 26, src.h }, SDL_FRect{ x, y, cap, h });
        plat::drawImage(b, SDL_Rect{ src.x + 26, src.y, src.w - 52, src.h }, SDL_FRect{ x + cap, y, w - 2 * cap, h });
        plat::drawImage(b, SDL_Rect{ src.x + src.w - 26, src.y, 26, src.h }, SDL_FRect{ x + w - cap, y, cap, h });
    } else plat::fillRoundRect({ x, y, w, h }, h * 0.5f, hot ? SDL_Color{ 250, 120, 20, 255 } : SDL_Color{ 200, 20, 20, 255 });
    const float sc = std::min(TXT * h / 40.0f * 1.25f, (w - 2 * cap) / std::max(1.0f, font().width(s, 1.0f)));
    textC(s, cx, cy - font().height(sc) * 0.5f, sc);
}

// Panel rojo con borde (del cartel de pausa del juego, banner.png), estirado en 9 partes.
void panel(float x, float y, float w, float h) {
    plat::Image* p = img("v1/assets/lang/images/menu/banner.png");
    if (!p) { plat::fillRoundRect({ x, y, w, h }, 18, { 190, 20, 20, 240 }); return; }
    const SDL_Rect s = { 16, 608, 368, 263 };   // panel rojo de banner.png (512x1024)
    const int c = 34;
    const float xs[4] = { x, x + c, x + w - c, x + w }, ys[4] = { y, y + c, y + h - c, y + h };
    const int sx[4] = { s.x, s.x + c, s.x + s.w - c, s.x + s.w }, sy[4] = { s.y, s.y + c, s.y + s.h - c, s.y + s.h };
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
            plat::drawImage(p, SDL_Rect{ sx[i], sy[j], sx[i + 1] - sx[i], sy[j + 1] - sy[j] },
                            SDL_FRect{ xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j] });
}

const char* onOff(int v) { return v ? "Sí" : "No"; }

}  // namespace

// Arte del menú de "La Máquina del Tiempo" (la parte de 1200x900 de menu.jpg) ocupando el alto de la
// pantalla; a los costados siguen los rayos rojos animados y los bordes del arte se funden con ellos.
// bw: ancho de la columna de botones de la derecha. Lo usan el inicio y el menú de Vascolet 1.
void drawTitleArt(float bw) {
    const float t = plat::ticksMs() / 1000.0f;
    const float zoom = 1.0f + 0.025f * (0.5f + 0.5f * std::sin(t * 0.7f));   // respira despacio
    // Ocupa todo el alto; en pantallas poco anchas se achica para que no lo tapen los botones.
    // El 15% derecho del arte son solo rayos: los botones pueden quedar encima.
    const float baseH = H(), baseW = baseH * 4.0f / 3.0f;   // siempre todo el alto, sin bordes
    const float aw = baseW * zoom, ah = baseH * zoom;
    const float ax = std::max(0.0f, (W() - bw - 36 - baseW * 0.85f) * 0.5f) - (aw - baseW) * 0.5f;
    const float ay = (H() - ah) * 0.5f;
    rays(ax + aw * 0.5f, ay + ah * 0.47f, 0.25f);
    if (plat::Image* e = img("v1/assets/images/menuelements/menu.jpg")) {
        // Franjas verticales: el centro opaco y los bordes con transparencia creciente.
        const int STRIPS = 16; const float feather = aw * 0.07f;
        const float mid0 = ax + feather, mid1 = ax + aw - feather;
        plat::drawImage(e, SDL_Rect{ (int)(1200 * 0.07f), 0, (int)(1200 * 0.86f), 900 }, SDL_FRect{ mid0, ay, mid1 - mid0, ah });
        for (int side = 0; side < 2; side++)
            for (int k = 0; k < STRIPS; k++) {
                const float f0 = k / (float)STRIPS, f1 = (k + 1) / (float)STRIPS;   // 0 = borde externo
                const float sx0 = side ? 1200 * (1 - 0.07f * f1) : 1200 * 0.07f * f0;
                const float sx1 = side ? 1200 * (1 - 0.07f * f0) : 1200 * 0.07f * f1;
                const float dx0 = ax + aw * sx0 / 1200, dx1 = ax + aw * sx1 / 1200;
                const Uint8 a = (Uint8)(255 * (k + 0.5f) / STRIPS);
                plat::setDrawTint({ 255, 255, 255, a });
                plat::drawImage(e, SDL_Rect{ (int)sx0, 0, std::max(1, (int)(sx1 - sx0)), 900 }, SDL_FRect{ dx0, ay, dx1 - dx0, ah });
            }
        plat::setDrawTint({ 255, 255, 255, 255 });
    }
}

void titleButton(const std::string& label, float cx, float cy, float w, float h, bool hot, bool pressed) {
    button(label, cx, cy, w, h, hot, pressed);
}

namespace {

class Launcher {
public:
    void run();
    void runOptions();   // solo las opciones (desde la pausa de un juego)
private:
    Page page = HOME;
    int sel = 0, gameSel = 0;
    Page back = HOME;
    std::vector<Row> rows;
    bool quit = false;
    bool startV1 = false;   // se arranca fuera de update(), en el bucle principal
    bool hasNext = false; Page nextPage = HOME; int nextSel = 0;

    void go(Page p, int s = 0) { hasNext = true; nextPage = p; nextSel = s; }
    void switchPage();
    void buildRows();
    void update();
    void draw();
    void updateHome(); void drawHome();
    void updateList(); void drawList();
    void updateMapping(); void drawMapping();
    void updateGame(); void drawGame();
    static const char* pageTitle(Page p);
    // Botones del inicio (a la derecha del emblema).
    float homeBtnW() const { return std::min(230.0f, W() * 0.3f); }
    float homeBtnX() const { return W() - homeBtnW() * 0.5f - 18; }
    float homeBtnY(int i) const { return H() * 0.5f - 96 + i * 58; }
};

const char* Launcher::pageTitle(Page p) {
    switch (p) {
        case OPTIONS: return "Opciones";
        case GRAPHICS: return "Gráficos";
        case ADVANCED: return "Avanzado";
        case CONTROLS: return "Controles";
        case SOUND: return "Sonido";
        default: return "";
    }
}

void Launcher::switchPage() {
    hasNext = false;
    const Page p = nextPage; const int s = nextSel;
    if (page == MAPPING && p != MAPPING) {
        plat::touchLayoutGet(settings().touchPos);
        plat::touchMapEdit(false);
        settings().save();
    }
    page = p; sel = s;
    if (p == MAPPING) plat::touchMapEdit(true);
    buildRows();
}

void Launcher::buildRows() {
    rows.clear();
    auto toggle = [](int Settings::* f) { return [f](int) { settings().*f ^= 1; }; };
    auto cycle = [](int Settings::* f, int n) { return [f, n](int d) { settings().*f = (settings().*f + n + d) % n; }; };
    switch (page) {
        case OPTIONS:
            back = HOME;
            rows = { { "Gráficos", nullptr, [this](int) { go(GRAPHICS); } },
                     { "Controles", nullptr, [this](int) { go(CONTROLS); } },
                     { "Sonido", nullptr, [this](int) { go(SOUND); } } };
            break;
        case GRAPHICS: {
            back = OPTIONS;
#ifndef __ANDROID__
            rows.push_back({ "Pantalla completa", [] { return std::string(onOff(settings().fullscreen)); }, toggle(&Settings::fullscreen) });
#endif
            static const char* SHARP[3] = { "No", "Media", "Alta" };
            rows.push_back({ "Diseño gráfico", [] { return std::string(settings().modernArt ? "Moderno" : "Clásico"); }, toggle(&Settings::modernArt) });
            rows.push_back({ "Estirar pantalla", [] { return std::string(onOff(settings().stretch)); }, toggle(&Settings::stretch) });
            rows.push_back({ "Suavizado", [] { return std::string(onOff(settings().smooth)); }, toggle(&Settings::smooth) });
            rows.push_back({ "Nitidez", [] { return std::string(SHARP[settings().sharpness % 3]); }, cycle(&Settings::sharpness, 3) });
            rows.push_back({ "Calidad HD", [] { return std::string(onOff(settings().hd)); }, toggle(&Settings::hd) });
            rows.push_back({ "Avanzado", nullptr, [this](int) { go(ADVANCED); } });
            rows.push_back({ "Cámara fija", [] { return std::string(onOff(settings().camFixed)); }, toggle(&Settings::camFixed) });
            rows.push_back({ "120 FPS", [] { return std::string(onOff(settings().fps120)); }, toggle(&Settings::fps120) });
            rows.push_back({ "Efecto TV", [] { return std::string(onOff(settings().tvEffect)); }, toggle(&Settings::tvEffect) });
            break;
        }
        case ADVANCED: {
            back = GRAPHICS;
            static const char* FX[HDFX_COUNT] = { "Luces dinámicas", "Sombras", "Luz de borde", "Bloom", "Rayos de luz", "Niebla", "Polvo en el aire" };
            for (int i = 0; i < HDFX_COUNT; i++)
                rows.push_back({ FX[i], [i] { return std::string(onOff(!((settings().hdOff >> i) & 1))); },
                                 [i](int) { settings().hdOff ^= 1 << i; } });
            break;
        }
        case CONTROLS:
            back = OPTIONS;
            rows.push_back({ "Joystick", [] { return std::string(settings().stickType ? "Moderno" : "Clásico"); }, toggle(&Settings::stickType) });
            rows.push_back({ "Estilo", [] { return std::string(TOUCH_STYLES[settings().touchStyle % 3]); }, cycle(&Settings::touchStyle, 3) });
            rows.push_back({ "Tamaño", [] { return std::to_string(STICK_SIZES[settings().stickSize & 3]) + "%"; }, cycle(&Settings::stickSize, 4) });
            rows.push_back({ "Color", [] { return std::string(STICK_THEMES[settings().stickTheme % 5]); }, cycle(&Settings::stickTheme, 5) });
            rows.push_back({ "Opacidad", [] { return settings().touchAlpha ? std::to_string(settings().touchAlpha) + "%" : std::string("No"); },
                             [](int d) { int& a = settings().touchAlpha; a = ((a / 25) + 5 + d) % 5 * 25; } });
            rows.push_back({ "Mapeo de botones", nullptr, [this](int) { go(MAPPING); } });
            break;
        case SOUND: {
            back = OPTIONS;
            auto vol = [](int Settings::* f) { return [f](int d) { int& v = settings().*f; v = (v + 110 + d * 10) % 110; }; };
            rows.push_back({ "Música", [] { return std::to_string(settings().music) + "%"; }, vol(&Settings::music) });
            rows.push_back({ "Efectos", [] { return std::to_string(settings().sfx) + "%"; }, vol(&Settings::sfx) });
            break;
        }
        default: break;
    }
}

// ---------------- Inicio: emblema a la izquierda, botones a la derecha ----------------
static const char* HOME_BTNS[4] = { "Vascolet 1", "Vascolet 2", "Opciones", "Salir" };
const float HB_H = 46;

void Launcher::updateHome() {
    if (plat::pressed(plat::BTN_DOWN)) { sel = (sel + 1) % 4; click(); }
    if (plat::pressed(plat::BTN_UP)) { sel = (sel + 3) % 4; click(); }
    bool act = plat::pressed(plat::BTN_ACCEPT) || plat::pressed(plat::BTN_START);
    const float bw = homeBtnW();
    for (int i = 0; i < 4; i++) {
        const float x = homeBtnX() - bw * 0.5f, y = homeBtnY(i) - HB_H * 0.5f;
        if (g_ptr.hoverIn(x, y, bw, HB_H) || g_ptr.pressIn(x, y, bw, HB_H)) sel = i;
        if (g_ptr.tapIn(x, y, bw, HB_H)) { sel = i; act = true; }
    }
    if (act) {
        click();
        if (sel == 0) startV1 = true;
        else if (sel == 1) { gameSel = sel; go(GAME); }
        else if (sel == 2) go(OPTIONS);
        else quit = true;
    }
    if (plat::pressed(plat::BTN_BACK)) quit = true;
}

void Launcher::drawHome() {
    const float bw = homeBtnW();
    drawTitleArt(bw);
    for (int i = 0; i < 4; i++) {
        const float x = homeBtnX() - bw * 0.5f, y = homeBtnY(i) - HB_H * 0.5f;
        button(HOME_BTNS[i], homeBtnX(), homeBtnY(i), bw, HB_H, sel == i, g_ptr.pressIn(x, y, bw, HB_H));
    }
}

// ---------------- Listas de opciones ----------------
// Alto de cada fila: 29, o menos si la lista no entra (siempre se ve el botón "Volver").
float rowH(int n) { return std::min(29.0f, (H() - 112.0f) / std::max(1, n)); }
float listW() { return std::min(470.0f, W() - 40); }
float listX() { return (W() - listW()) * 0.5f; }
float listY0(int n) { return std::max(58.0f, (H() - (n * rowH(n) + 70)) * 0.5f + 20); }

void Launcher::updateList() {
    const int n = (int)rows.size();             // la fila n es "Volver"
    const float RH = rowH(n);
    if (plat::pressed(plat::BTN_DOWN)) { sel = (sel + 1) % (n + 1); click(); }
    if (plat::pressed(plat::BTN_UP)) { sel = (sel + n) % (n + 1); click(); }
    bool act = plat::pressed(plat::BTN_ACCEPT);
    int d = 0;
    if (plat::pressed(plat::BTN_RIGHT)) d = 1;
    if (plat::pressed(plat::BTN_LEFT)) d = -1;
    const float x = listX(), w = listW(), y0 = listY0(n);
    for (int i = 0; i < n; i++) {
        const float y = y0 + i * RH;
        if (g_ptr.hoverIn(x, y, w, RH) || g_ptr.pressIn(x, y, w, RH)) sel = i;
        if (g_ptr.tapIn(x, y, w, RH)) { sel = i; act = true; }
    }
    const float by = y0 + n * RH + 30;
    if (g_ptr.hoverIn(W() * 0.5f - 80, by - 18, 160, 36) || g_ptr.pressIn(W() * 0.5f - 80, by - 18, 160, 36)) sel = n;
    if (g_ptr.tapIn(W() * 0.5f - 80, by - 18, 160, 36)) { sel = n; act = true; }
    if (sel == n) {
        if (act) { click(); go(back, 0); }
    } else if (act || (d && rows[sel].value)) {
        click();
        rows[sel].change(act ? 1 : d);
        if (!hasNext) { settings().apply(); settings().save(); }
    }
    if (!hasNext && plat::pressed(plat::BTN_BACK)) { click(); go(back, 0); }
}

void Launcher::drawList() {
    rays(W() * 0.5f, H() * 0.45f, 0.25f);
    const int n = (int)rows.size();
    const float RH = rowH(n);
    const float x = listX(), w = listW(), y0 = listY0(n);
    textC(pageTitle(page), W() * 0.5f, y0 - 50, 0.85f, YELLOW);
    panel(x - 14, y0 - 12, w + 28, n * RH + 24);
    const float sc = TXT * 0.92f, th = font().height(sc);
    for (int i = 0; i < n; i++) {
        const float y = y0 + i * RH;
        if (sel == i) button("", x + w * 0.5f, y + RH * 0.5f, w, RH + 2, true);
        text(rows[i].label, x + 16, y + (RH - th) * 0.5f, sc, WHITE);
        if (rows[i].value) {
            const std::string v = rows[i].value();
            text(v, x + w - 16 - font().width(v, sc), y + (RH - th) * 0.5f, sc, YELLOW);
        } else {   // submenú: flecha
            const float ax = x + w - 22, ay = y + RH * 0.5f;
            plat::fillTriangle({ ax - 6, ay - 8 }, { ax + 6, ay }, { ax - 6, ay + 8 }, YELLOW);
        }
    }
    const float by = y0 + n * RH + 30;
    button("Volver", W() * 0.5f, by, 160, 36, sel == n, g_ptr.pressIn(W() * 0.5f - 80, by - 18, 160, 36));
}

// ---------------- Mapeo de botones táctiles ----------------
void Launcher::updateMapping() {
    const float cy = H() * 0.5f;
    if (g_ptr.tapIn(W() * 0.5f - 160, cy - 20, 150, 40)) { click(); plat::touchLayoutReset(); }
    else if (g_ptr.tapIn(W() * 0.5f + 10, cy - 20, 150, 40) || plat::pressed(plat::BTN_BACK) || plat::pressed(plat::BTN_ACCEPT)) {
        click();
        go(CONTROLS, 5); // al salir se guarda la posición de los botones
    }
}

void Launcher::drawMapping() {
    rays(W() * 0.5f, H() * 0.45f, 0.45f);
    const float cy = H() * 0.5f;
    textC("Arrastrá cada botón a donde quieras", W() * 0.5f, cy - 70, 0.6f, YELLOW);
    button("Restablecer", W() * 0.5f - 85, cy, 150, 40, false);
    button("Listo", W() * 0.5f + 85, cy, 150, 40, true);
}

// ---------------- Juego (en desarrollo) ----------------
void Launcher::updateGame() {
    const float by = H() - 46;
    if (plat::pressed(plat::BTN_ACCEPT) || plat::pressed(plat::BTN_BACK) || g_ptr.tapIn(W() * 0.5f - 80, by - 20, 160, 40)) { click(); go(HOME, gameSel); }
}

void Launcher::drawGame() {
    rays(W() * 0.5f, H() * 0.4f, 0.2f);
    textC(gameSel == 0 ? "Vascolet 1" : "Vascolet 2", W() * 0.5f, 18, 0.9f, YELLOW);
    const float pw = std::min(440.0f, W() - 60), px = (W() - pw) * 0.5f;
    panel(px, 70, pw, 220);
    plat::Image* c = gameSel == 0 ? img("v1/assets/images/menuelements/menu.jpg") : img("v2/assets/gui/menu/background.jpg");
    const SDL_Rect src = gameSel == 0 ? SDL_Rect{ 0, 0, 1200, 900 } : SDL_Rect{ 0, 0, 512, 384 };
    if (c) plat::drawImage(c, src, SDL_FRect{ W() * 0.5f - 88, 88, 176, 132 });
    textC("Este juego se está adaptando a Android.", W() * 0.5f, 232, 0.5f, WHITE);
    textC("Va a estar en una próxima versión.", W() * 0.5f, 256, 0.44f, YELLOW);
    const float by = H() - 46;
    button("Volver", W() * 0.5f, by, 160, 40, true, g_ptr.pressIn(W() * 0.5f - 80, by - 20, 160, 40));
}

// ---------------- Bucle ----------------
void Launcher::update() {
    g_ptr.poll();
    switch (page) {
        case HOME: updateHome(); break;
        case MAPPING: updateMapping(); break;
        case GAME: updateGame(); break;
        default: updateList(); break;
    }
    if (hasNext) switchPage();
}

void Launcher::draw() {
    plat::setView43(false, (int)W());   // pantalla completa, sin barras 4:3
    switch (page) {
        case HOME: drawHome(); break;
        case MAPPING: drawMapping(); break;
        case GAME: drawGame(); break;
        default: drawList(); break;
    }
}

void Launcher::run() {
    g_click = plat::loadSound("v2/assets/audio/fx/win/coin.ogg");
    plat::setTouchOverlay(false);
    plat::setCursorMenus(true);
    plat::playMusic("v1/assets/audio/music/andy.ogg");
    const float STEP = 1.0f / 60.0f;
    float acc = 0;
    uint32_t last = plat::ticksMs();
    while (!plat::quitRequested() && !quit) {
        const uint32_t now = plat::ticksMs();
        acc = std::min(0.25f, acc + (now - last) / 1000.0f); last = now;
        while (acc >= STEP && !quit) { acc -= STEP; plat::inputPoll(); update(); }
        if (startV1) {
            startV1 = false;
            plat::stopMusic();
            v1::run();
            plat::setTouchOverlay(false);
            plat::playMusic("v1/assets/audio/music/andy.ogg");
            acc = 0; last = plat::ticksMs();
            continue;
        }
        plat::beginFrame();
        draw();
        plat::endFrame();
    }
    if (page == MAPPING) { go(HOME); switchPage(); }
    plat::stopMusic();
}

} // namespace

// Opciones abiertas desde la pausa de un juego: mismas pantallas; vuelve al salir de "Opciones".
void Launcher::runOptions() {
    if (!g_click) g_click = plat::loadSound("v2/assets/audio/fx/win/coin.ogg");
    plat::setTouchOverlay(false);
    go(OPTIONS); switchPage();
    const float STEP = 1.0f / 60.0f;
    float acc = 0;
    uint32_t last = plat::ticksMs();
    while (!plat::quitRequested() && page != HOME) {
        const uint32_t now = plat::ticksMs();
        acc = std::min(0.25f, acc + (now - last) / 1000.0f); last = now;
        while (acc >= STEP && page != HOME) { acc -= STEP; plat::inputPoll(); update(); }
        plat::beginFrame();
        if (page != HOME) draw();
        plat::endFrame();
    }
    if (page == MAPPING) { go(HOME); switchPage(); }
}

void runOptionsMenu() {
    Launcher l;
    l.runOptions();
}

void runLauncher() {
    settings().load();
    settings().apply();
    Launcher l;
    l.run();
}

}
