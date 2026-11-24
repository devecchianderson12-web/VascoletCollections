#include "launcher.h"
#include "font.h"
#include "settings.h"
#include "../platform.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace vc {

namespace {

// ---- Estilo: oscuro, bordes redondeados, acento naranja Vascolet ----
const SDL_Color BG = { 12, 14, 20, 255 }, PANEL = { 24, 28, 38, 236 }, ROW_HOT = { 242, 140, 40, 70 };
const SDL_Color ACCENT = { 242, 140, 40, 255 }, TEXT = { 240, 240, 245, 255 }, DIM = { 160, 166, 180, 255 };
const float TXT = 0.6f;   // escala del texto normal

enum Page { HOME, OPTIONS, GRAPHICS, ADVANCED, CONTROLS, SOUND, MAPPING, GAME };

struct Row {
    std::string label;
    std::function<std::string()> value;   // texto a la derecha (vacío = submenú)
    std::function<void(int)> change;      // d = +1 / -1 (aceptar = +1)
};

Mix_Chunk* g_click = nullptr;
void click() { plat::playSound(g_click); }

bool tapIn(float x, float y, float w, float h) {
    float tx, ty;
    return plat::menuTap(&tx, &ty) && tx >= x && tx < x + w && ty >= y && ty < y + h;
}
bool hoverIn(float x, float y, float w, float h) {
    float hx, hy;
    return plat::menuPointer(&hx, &hy) && hx >= x && hx < x + w && hy >= y && hy < y + h;
}

void text(const std::string& s, float x, float y, SDL_Color c = TEXT, float sc = TXT) { uiFont().draw(s, x, y, sc, c); }
void textC(const std::string& s, float cx, float y, SDL_Color c = TEXT, float sc = TXT) { uiFont().drawCentered(s, cx, y, sc, c); }
void title(const std::string& s, float y) {
    const float sc = 30.0f / std::max(1.0f, titleFont().height());
    titleFont().drawCentered(s, BASE_W * 0.5f, y, sc, TEXT);
}
void button(const std::string& s, float cx, float cy, float w, bool hot) {
    plat::fillRoundRect({ cx - w * 0.5f, cy - 15, w, 30 }, 15, hot ? ACCENT : SDL_Color{ 44, 50, 64, 240 });
    textC(s, cx, cy - uiFont().height(TXT) * 0.5f, hot ? SDL_Color{ 20, 14, 8, 255 } : TEXT);
}

const char* onOff(int v) { return v ? "Sí" : "No"; }

class Launcher {
public:
    void run();
private:
    Page page = HOME;
    int sel = 0;            // fila / elemento seleccionado
    int gameSel = 0;        // juego elegido (0 = Vascolet 1, 1 = Vascolet 2)
    Page back = HOME;       // a dónde vuelve cada lista
    std::vector<Row> rows;
    plat::Image* cover[2] = {};
    SDL_Rect coverSrc[2] = { { 0, 0, 1200, 900 }, { 0, 0, 512, 384 } };
    bool quit = false;
    // El cambio de página se aplica al final del cuadro (las filas pueden pedirlo desde su propio código).
    bool hasNext = false; Page nextPage = HOME; int nextSel = 0;

    void go(Page p, int s = 0) { hasNext = true; nextPage = p; nextSel = s; }
    void switchPage();
    void buildRows();
    void update();
    void draw();
    void updateHome();
    void drawHome();
    void updateList();
    void drawList();
    void updateMapping();
    void drawMapping();
    void updateGame();
    void drawGame();
    static const char* pageTitle(Page p);
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
    Settings& st = settings();
    rows.clear();
    auto toggle = [&](int Settings::* f) { return [f](int) { settings().*f ^= 1; }; };
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
        case CONTROLS: {
            back = OPTIONS;
            rows.push_back({ "Joystick", [] { return std::string(settings().stickType ? "Moderno" : "Clásico"); }, toggle(&Settings::stickType) });
            rows.push_back({ "Estilo", [] { return std::string(settings().btn3d ? "3D" : "2D"); }, toggle(&Settings::btn3d) });
            rows.push_back({ "Tamaño", [] { return std::to_string(STICK_SIZES[settings().stickSize & 3]) + "%"; }, cycle(&Settings::stickSize, 4) });
            rows.push_back({ "Íconos", [] { return std::string(STICK_THEMES[settings().stickTheme & 3]); }, cycle(&Settings::stickTheme, 4) });
            rows.push_back({ "Opacidad", [] { return settings().touchAlpha ? std::to_string(settings().touchAlpha) + "%" : std::string("No"); },
                             [](int d) { int& a = settings().touchAlpha; a = ((a / 25) + 5 + d) % 5 * 25; } });
            rows.push_back({ "Mapeo de botones", nullptr, [this](int) { go(MAPPING); } });
            break;
        }
        case SOUND: {
            back = OPTIONS;
            auto vol = [](int Settings::* f) { return [f](int d) { int& v = settings().*f; v = std::max(0, std::min(100, v + d * 10)); }; };
            rows.push_back({ "Música", [] { return std::to_string(settings().music) + "%"; }, vol(&Settings::music) });
            rows.push_back({ "Efectos", [] { return std::to_string(settings().sfx) + "%"; }, vol(&Settings::sfx) });
            break;
        }
        default: break;
    }
    (void)st;
}

// ---------------- Inicio ----------------
const float CARD_W = 214, CARD_H = 160, CARD_Y = 72;
float cardX(int i) { return i == 0 ? 34.0f : BASE_W - 34.0f - CARD_W; }

void Launcher::updateHome() {
    // sel: 0-1 juegos, 2 Opciones, 3 Salir
    if (plat::pressed(plat::BTN_LEFT)) { sel = sel < 2 ? 0 : 2; click(); }
    if (plat::pressed(plat::BTN_RIGHT)) { sel = sel < 2 ? 1 : 3; click(); }
    if (plat::pressed(plat::BTN_DOWN) && sel < 2) { sel += 2; click(); }
    if (plat::pressed(plat::BTN_UP) && sel >= 2) { sel -= 2; click(); }
    bool act = plat::pressed(plat::BTN_ACCEPT) || plat::pressed(plat::BTN_START);
    for (int i = 0; i < 2; i++) {
        if (hoverIn(cardX(i), CARD_Y, CARD_W, CARD_H + 44)) sel = i;
        if (tapIn(cardX(i), CARD_Y, CARD_W, CARD_H + 44)) { sel = i; act = true; }
    }
    const float by = 334;
    if (hoverIn(BASE_W * 0.5f - 150, by - 15, 130, 30)) sel = 2;
    if (hoverIn(BASE_W * 0.5f + 20, by - 15, 130, 30)) sel = 3;
    if (tapIn(BASE_W * 0.5f - 150, by - 15, 130, 30)) { sel = 2; act = true; }
    if (tapIn(BASE_W * 0.5f + 20, by - 15, 130, 30)) { sel = 3; act = true; }
    if (act) {
        click();
        if (sel < 2) { gameSel = sel; go(GAME); }
        else if (sel == 2) go(OPTIONS);
        else quit = true;
    }
    if (plat::pressed(plat::BTN_BACK)) quit = true;
}

void Launcher::drawHome() {
    title("Vascolet Collection", 16);
    static const char* NAMES[2] = { "Vascolet 1", "Vascolet 2" };
    static const char* SUBS[2] = { "La Máquina del Tiempo", "La Fórmula Secreta" };
    const float t = plat::ticksMs() / 1000.0f;
    for (int i = 0; i < 2; i++) {
        const bool hot = sel == i;
        const float lift = hot ? 4.0f + std::sin(t * 3.0f) * 1.5f : 0.0f;
        const float x = cardX(i), y = CARD_Y - lift;
        plat::fillRoundRect({ x - 6, y - 6, CARD_W + 12, CARD_H + 54 }, 14, hot ? ACCENT : PANEL);
        plat::fillRoundRect({ x - 3, y - 3, CARD_W + 6, CARD_H + 48 }, 12, PANEL);
        if (!cover[i]) cover[i] = plat::loadImage(i == 0 ? "v1/assets/images/menuelements/menu.jpg" : "v2/assets/gui/menu/background.jpg");
        if (cover[i]) plat::drawImage(cover[i], coverSrc[i], SDL_FRect{ x, y, CARD_W, CARD_H });
        textC(NAMES[i], x + CARD_W * 0.5f, y + CARD_H + 4, hot ? ACCENT : TEXT, 0.68f);
        textC(SUBS[i], x + CARD_W * 0.5f, y + CARD_H + 25, DIM, 0.5f);
    }
    button("Opciones", BASE_W * 0.5f - 85, 334, 130, sel == 2);
    button("Salir", BASE_W * 0.5f + 85, 334, 130, sel == 3);
}

// ---------------- Listas de opciones ----------------
const float LIST_X = 76, LIST_W = BASE_W - 152, ROW_Y0 = 64, ROW_H = 28;

void Launcher::updateList() {
    const int n = (int)rows.size();             // la fila n es "Volver"
    if (plat::pressed(plat::BTN_DOWN)) { sel = (sel + 1) % (n + 1); click(); }
    if (plat::pressed(plat::BTN_UP)) { sel = (sel + n) % (n + 1); click(); }
    bool act = plat::pressed(plat::BTN_ACCEPT);
    int d = 0;
    if (plat::pressed(plat::BTN_RIGHT)) d = 1;
    if (plat::pressed(plat::BTN_LEFT)) d = -1;
    for (int i = 0; i < n; i++) {
        const float y = ROW_Y0 + i * ROW_H;
        if (hoverIn(LIST_X, y, LIST_W, ROW_H)) sel = i;
        if (tapIn(LIST_X, y, LIST_W, ROW_H)) { sel = i; act = true; }
    }
    const float by = ROW_Y0 + n * ROW_H + 30;
    if (hoverIn(BASE_W * 0.5f - 65, by - 15, 130, 30)) sel = n;
    if (tapIn(BASE_W * 0.5f - 65, by - 15, 130, 30)) { sel = n; act = true; }
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
    title(pageTitle(page), 16);
    const int n = (int)rows.size();
    plat::fillRoundRect({ LIST_X - 10, ROW_Y0 - 10, LIST_W + 20, n * ROW_H + 20 }, 16, PANEL);
    const float th = uiFont().height(TXT);
    for (int i = 0; i < n; i++) {
        const float y = ROW_Y0 + i * ROW_H;
        if (sel == i) plat::fillRoundRect({ LIST_X, y + 1, LIST_W, ROW_H - 2 }, 10, ROW_HOT);
        text(rows[i].label, LIST_X + 12, y + (ROW_H - th) * 0.5f, sel == i ? ACCENT : TEXT);
        const std::string v = rows[i].value ? rows[i].value() : std::string(">");
        uiFont().draw(v, LIST_X + LIST_W - 12 - uiFont().width(v, TXT), y + (ROW_H - th) * 0.5f, TXT, rows[i].value ? DIM : ACCENT);
    }
    button("Volver", BASE_W * 0.5f, ROW_Y0 + n * ROW_H + 30, 130, sel == n);
}

// ---------------- Mapeo de botones táctiles ----------------
const float MAP_Y = 200, MAP_RX = 188, MAP_OX = 324, MAP_HW = 62;

void Launcher::updateMapping() {
    if (tapIn(MAP_RX - MAP_HW, MAP_Y - 15, MAP_HW * 2, 30)) { click(); plat::touchLayoutReset(); }
    else if (tapIn(MAP_OX - MAP_HW, MAP_Y - 15, MAP_HW * 2, 30) || plat::pressed(plat::BTN_BACK) || plat::pressed(plat::BTN_ACCEPT)) {
        click();
        go(CONTROLS, 5); // al salir se guarda la posición de los botones
    }
}

void Launcher::drawMapping() {
    textC("Arrastrá cada botón a donde quieras", BASE_W * 0.5f, 150, TEXT);
    button("Restablecer", MAP_RX, MAP_Y, MAP_HW * 2, false);
    button("Listo", MAP_OX, MAP_Y, MAP_HW * 2, true);
}

// ---------------- Juego (en desarrollo) ----------------
void Launcher::updateGame() {
    if (plat::pressed(plat::BTN_ACCEPT) || plat::pressed(plat::BTN_BACK) || tapIn(BASE_W * 0.5f - 65, 300, 130, 30)) { click(); go(HOME, gameSel); }
}

void Launcher::drawGame() {
    title(gameSel == 0 ? "Vascolet 1" : "Vascolet 2", 16);
    plat::fillRoundRect({ 76, 70, BASE_W - 152, 200 }, 16, PANEL);
    if (cover[gameSel]) plat::drawImage(cover[gameSel], coverSrc[gameSel], SDL_FRect{ BASE_W * 0.5f - 80, 84, 160, 120 });
    textC("Este juego se está adaptando a Android.", BASE_W * 0.5f, 214, TEXT);
    textC("Va a estar disponible en una próxima versión.", BASE_W * 0.5f, 238, DIM, 0.52f);
    button("Volver", BASE_W * 0.5f, 315, 130, true);
}

// ---------------- Bucle ----------------
void Launcher::update() {
    switch (page) {
        case HOME: updateHome(); break;
        case MAPPING: updateMapping(); break;
        case GAME: updateGame(); break;
        default: updateList(); break;
    }
    if (hasNext) switchPage();
}

void Launcher::draw() {
    plat::setView43(true, BASE_W);
    plat::fillScreen(BG);
    // Fondo: degradado suave con un brillo naranja que respira.
    const float t = plat::ticksMs() / 1000.0f;
    const Uint8 glow = (Uint8)(26 + 10 * std::sin(t * 0.7f));
    plat::fillRoundRect({ -60, 250, BASE_W + 120, 260 }, 130, { 242, 120, 30, glow });
    switch (page) {
        case HOME: drawHome(); break;
        case MAPPING: plat::dimScreen(0.35f); drawMapping(); break;
        case GAME: drawGame(); break;
        default: drawList(); break;
    }
}

void Launcher::run() {
    g_click = plat::loadSound("v2/assets/audio/fx/win/coin.ogg");
    plat::setTouchOverlay(false);
    plat::playMusic("v2/assets/audio/music/win/menu.ogg");
    const float STEP = 1.0f / 60.0f;
    float acc = 0;
    uint32_t last = plat::ticksMs();
    while (!plat::quitRequested() && !quit) {
        const uint32_t now = plat::ticksMs();
        acc = std::min(0.25f, acc + (now - last) / 1000.0f); last = now;
        while (acc >= STEP && !quit) { acc -= STEP; plat::inputPoll(); update(); }
        plat::beginFrame();
        draw();
        plat::endFrame();
    }
    if (page == MAPPING) { go(HOME); switchPage(); }
    plat::stopMusic();
}

} // namespace

void runLauncher() {
    settings().load();
    settings().apply();
    Launcher l;
    l.run();
}

}
