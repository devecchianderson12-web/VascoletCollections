#pragma once
#include <SDL.h>
#include <SDL_mixer.h>
#include <string>
#include <vector>
#include <cstdint>

namespace plat {

bool assetsOpen(const char* dir);
void assetsDataDir(const std::string& dir);

std::vector<uint8_t> assetRead(const std::string& name);
bool assetExists(const std::string& name);
// Prefijo para los nombres que empiezan con "assets/" (ej. "v1/" mientras se juega Vascolet 1).
void setAssetPrefix(const std::string& prefix);

struct ModInfo {
    std::string folder, name, author, description, version;
    bool enabled = true, hasIcon = false;
};
void modsInit();
const std::vector<ModInfo>& mods();
bool modsSave(const std::vector<ModInfo>& list);
void presenceInit();
void setPresence(const std::string& details, const std::string& state);
void presenceUpdate();
std::string modFilePath(const ModInfo& m, const std::string& file);
std::vector<std::vector<uint8_t>> assetReadLayers(const std::string& name);

extern int SCREEN_W, SCREEN_H;
bool videoInit(int logicalW, int logicalH);

bool wideScreenWanted();
int wideLogicalWidth(int fixedW, int logicalH);
void setLogicalWidth(int w);
bool wideCapable();
void desktopSize(int* w, int* h);
void setWindowResolution(int w, int h);
void setFollowWindowShape(bool on, int refH);
void setLogicalSize(int w, int h);
void beginScaledSpace(float f);
void endScaledSpace();
bool touchEnabled();
bool touchControlsAvailable();

void setView43(bool on, int baseW = 512);

void dimScreen(float a);
void dimMargins(Uint8 alpha);
void fillScreen(SDL_Color c);
SDL_Color imagePixel(const std::string& name, int x, int y);
void setStretch(bool on);
void setSmooth(bool on);
void setTvEffect(bool on);
void setTouchOpacity(int percent);
// Joystick táctil: tamaño en % (75-150), tema de color de iconos y tipo (0 clásico, 1 moderno).
void setTouchStick(int sizePercent, int theme, int type, int style);  // style: 0 Brillante, 1 Plano, 2 Aro dorado
// Mapeo de botones táctiles: 5 botones (joystick, saltar, disparar, pausa, atrás) x (x, y)
// normalizados 0-65534; 0xFFFF = posición por defecto.
constexpr int TOUCH_LAYOUT_N = 10;
void setTouchLayout(const uint16_t* pos);
void touchLayoutGet(uint16_t* pos);
void touchLayoutReset();
void touchMapEdit(bool on);
// Muestra u oculta los controles táctiles (el mapeo los muestra siempre).
void setTouchOverlay(bool on);
// Muestra u oculta el botón de disparo táctil.
void setTouchShoot(bool on);
// Calidad de imagen: nitidez 0-2 y texturas HD (2x). Recarga las imágenes si cambia.
void setImageQuality(int sharpness, bool hd);
// true con Gráficos > CALIDAD HD: activa también la iluminación y sombras HD.
bool hdGraphics();
// Gráficos > DISEÑO GRÁFICO: false = clásico, true = moderno (carpeta "moderno", ver MODERNO.md).
void setModernArt(bool on);
// Gráficos > 120 FPS: pide al sistema la tasa de refresco más alta y el juego interpola el movimiento.
void setHighFps(bool on);
bool highFps();
SDL_Renderer* renderer();

struct Image {
    int w = 0, h = 0;
#ifdef __ANDROID__
    int page = 1024, cols = 0, rows = 0;
#else
    int page = 256, cols = 0, rows = 0;
#endif
    std::vector<SDL_Texture*> pages;
    std::string src;
    std::string full; // nombre completo para recargar al cambiar la calidad gráfica
    std::vector<uint8_t> tried;
    std::vector<uint32_t> used;
    int tileW = 0, tileH = 0, tileCols = 0;
    bool ok() const { return !pages.empty(); }
};

Image* loadTileset(const std::string& name, int tileW, int tileH);
void drawTile(const Image* img, int id, const SDL_FRect& dst);

void ensurePages(Image* img, const SDL_Rect& r);

Image* loadImage(const std::string& name, std::vector<uint8_t>* alphaOut = nullptr,
                 const std::vector<SDL_Rect>* used = nullptr);
extern int g_pagesLoaded, g_pagesSkipped;
void freeImage(Image* img);
Image* imageFromRGBA(int w, int h, const uint8_t* rgba);
Image* loadImageFile(const std::string& path, bool gray = false);
void fillRoundRect(SDL_FRect r, float radius, SDL_Color c);
void fillTriangle(SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_Color col);

void prefetchImage(const std::string& name, const std::vector<SDL_Rect>* used = nullptr);
void cancelPrefetches();

void setLowMemoryHandler(void (*cb)());

void setDrawTint(SDL_Color c);
// drawImage suma luz (mezcla aditiva) en lugar de pintar encima. Lo usa la luz de borde HD.
void setDrawAdditive(bool on);
void drawImage(const Image* img, const SDL_Rect& src, const SDL_FRect& dst,
               bool flipH = false, uint8_t alpha = 255, bool flipV = false);
inline void drawImage(const Image* img, const SDL_FRect& dst) {
    if (img) drawImage(img, SDL_Rect{0, 0, img->w, img->h}, dst);
}
void beginFrame();
void endFrame();

enum Button {
    BTN_LEFT, BTN_RIGHT, BTN_UP, BTN_DOWN,
    BTN_JUMP,
    BTN_SHOOT,
    BTN_DEFEND,
    BTN_ACCEPT,
    BTN_BACK,
    BTN_START,
    BTN_CHEAT,
    BTN_COUNT
};
void inputPoll();
bool held(Button b);
bool pressed(Button b);
bool released(Button b);
bool anyPressed();
bool quitRequested();
bool remoteControls();

enum BindAction { BIND_LEFT, BIND_RIGHT, BIND_UP, BIND_DOWN, BIND_JUMP, BIND_SHOOT, BIND_PAUSE, BIND_COUNT };
struct Binding { int key; int pad; int key2 = -1; };
const Binding& binding(int action);
void setBinding(int action, Binding b);
void resetBindings();
bool saveBindings();
void loadBindings();
std::string keyName(int scancode);
std::string padName(int button);
void beginCapture();
int captureResult(Binding* b);
bool menuTap(float* x, float* y);
bool menuTouch(float* x, float* y);
bool pointerDown(float* x, float* y);   // dedo/mouse apretado, coordenadas de pantalla completa
bool pointerHover(float* x, float* y);  // mouse en PC, coordenadas de pantalla completa
bool menuPointer(float* x, float* y);
void setCursorMenus(bool menus);
int menuWheel();
void hideCursorThisFrame();

bool audioInit();
Mix_Chunk* loadSound(const std::string& name);
void playMusic(const std::string& name, bool loop = true);
void stopMusic();
void pauseMusic(bool paused);
bool musicPlaying();

void stopMusicUnless(const std::string& name);
void preloadMusic(const std::string& name);
void dropMusicCache();
void loadMusicData(const std::string& name);
void setMusicVolume(int v0to128);
void setSfxVolume(int v0to128);
void setFullscreen(bool on);
int localHour();
int  playSound(Mix_Chunk* c, int loops = 0);

uint32_t freeMemoryKB();

uint32_t ticksMs();

bool saveWrite(const std::vector<uint8_t>& data);
void profMark(int i);
void flushSave();
std::vector<uint8_t> saveRead();

void log(const char* fmt, ...);
void error(const char* fmt, ...);
[[noreturn]] void fatal(const char* fmt, ...);
extern int g_errorCount;

}
