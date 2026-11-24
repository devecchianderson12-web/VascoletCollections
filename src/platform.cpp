#include "platform.h"
#include <ctime>
#include <SDL_image.h>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <map>
#include <algorithm>
#include <new>
#include <fcntl.h>
#include <unistd.h>
#include <set>
#include <cmath>
#include <sstream>
#include <array>
#if 1
#include <filesystem>
#endif
#ifdef __ANDROID__
#include <android/log.h>
#include <jni.h>
#endif
#if defined(__ANDROID__) || defined(HOST)
#define XA_TOUCH 1
#endif

namespace plat {
static bool g_quit = false;
static bool g_smooth = false;
void setSmooth(bool on) { g_smooth = on; }

int g_sfxCallsFrame = 0;
uint32_t g_prof[3] = {};
void profMark(int i) { g_prof[i] = SDL_GetTicks(); }
static const int PRIO_AUDIO = 32, PRIO_MAIN = 64, PRIO_LOADER = 100;

static std::vector<std::string> g_logLines;
int g_errorCount = 0;

void log(const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
#ifdef __ANDROID__
    __android_log_write(ANDROID_LOG_INFO, "Xa", buf);
#else
    printf("%s\n", buf);
    fflush(stdout);
#endif
    g_logLines.push_back(buf);
    if (g_logLines.size() > 40) g_logLines.erase(g_logLines.begin());
}

void error(const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    g_errorCount++;
    log("ERROR: %s", buf);
}

void fatal(const char* fmt, ...) {
    char buf[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    log("FATAL: %s", buf);
#ifdef __ANDROID__
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Xa", buf, nullptr);
#endif
    exit(1);
}

struct PakEnt { uint32_t off, size; };
static std::map<std::string, PakEnt> g_dir;
static std::string g_base;
static std::vector<std::string> g_dataDirs = { "", "../", "../../" };

static bool readAt(uint32_t, void*, size_t) { return false; }

static std::string norm(std::string s) {
    for (auto& c : s) { if (c == '\\') c = '/'; c = (char)tolower((unsigned char)c); }
    while (s.size() > 2 && s[0] == '.' && s[1] == '/') s.erase(0, 2);
    size_t p; while ((p = s.find("//")) != std::string::npos) s.erase(p, 1);
    return s;
}

static SDL_RWops* rwOpen(const std::string& path) {
#ifdef __ANDROID__
    return SDL_RWFromFile(norm(path).c_str(), "rb");
#else
    return SDL_RWFromFile(path.c_str(), "rb");
#endif
}

static SDL_RWops* openBase(const std::string& name) {
    if (name.compare(0, 5, "data/") == 0) {
        for (auto& d : g_dataDirs) if (SDL_RWops* f = rwOpen(d + name)) return f;
        return nullptr;
    }
    return rwOpen(g_base + name);
}

static std::vector<std::string> g_modDirs;
// Gráficos > DISEÑO GRÁFICO: MODERNO. Las imágenes de la carpeta "moderno" (misma ruta y mismo
// tamaño que la original) reemplazan a las clásicas; si falta alguna se usa la clásica.
static bool g_modernArt = false;
static bool isImageName(const std::string& n) {
    auto ends = [&](const char* e) { const size_t l = strlen(e); return n.size() >= l && SDL_strcasecmp(n.c_str() + n.size() - l, e) == 0; };
    return ends(".png") || ends(".jpg") || ends(".jpeg");
}
static SDL_RWops* openAsset(const std::string& name) {
    for (auto it = g_modDirs.rbegin(); it != g_modDirs.rend(); ++it)
        if (SDL_RWops* f = SDL_RWFromFile((*it + name).c_str(), "rb")) return f;
    if (g_modernArt && isImageName(name))
        if (SDL_RWops* f = rwOpen(g_base + "moderno/" + name)) return f;
    return openBase(name);
}

static void loadPageIndex();
bool assetsOpen(const char* dir) {
    std::string b = dir ? dir : "";
    if (!b.empty() && b.back() != '/' && b.back() != '\\') b += '/';
    SDL_RWops* f = rwOpen(b + "v2/assets/data/texts.xml"); // Vascolet Collection: v1/ y v2/
    if (!f) return false;
    SDL_RWclose(f);
    g_base = b;
    log("assets: %sassets", b.c_str());
    loadPageIndex();
    return true;
}
void assetsDataDir(const std::string& dir) { g_dataDirs.insert(g_dataDirs.begin(), dir); }

bool assetExists(const std::string& name) {
    SDL_RWops* f = openAsset(name);
    if (f) SDL_RWclose(f);
    return f != nullptr;
}

std::vector<uint8_t> assetRead(const std::string& name) {
    std::vector<uint8_t> out;
    SDL_RWops* f = openAsset(name);
    if (!f) return out;
    Sint64 n = SDL_RWsize(f);
    if (n > 0) { out.resize((size_t)n); if (SDL_RWread(f, out.data(), 1, (size_t)n) != (size_t)n) out.clear(); }
    SDL_RWclose(f);
    return out;
}

static std::vector<ModInfo> g_modList;
static std::string g_modRoot;

static std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t i = 0; while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    return s.substr(i);
}
static std::vector<uint8_t> readFile(const std::string& path) {
    std::vector<uint8_t> out;
    if (SDL_RWops* f = SDL_RWFromFile(path.c_str(), "rb")) {
        Sint64 n = SDL_RWsize(f);
        if (n > 0) { out.resize((size_t)n); if (SDL_RWread(f, out.data(), 1, (size_t)n) != (size_t)n) out.clear(); }
        SDL_RWclose(f);
    }
    return out;
}

void modsInit() {
#ifdef __ANDROID__
    {
        const char* ext = SDL_AndroidGetExternalStoragePath();
        if (!ext) ext = SDL_AndroidGetInternalStoragePath();
        if (!ext) return;
        g_modRoot = std::string(ext) + "/mods/";
        std::error_code ec;
        const std::string ex = g_modRoot + "ejemplo/";
        if (!std::filesystem::exists(ex + "mod.txt", ec)) {
            static const char* FILES[] = { "mod.txt", "icon.png", "data/enemies.txt", "data/xa_texts.txt", "assets/data/level1.tmx" };
            for (const char* f : FILES) {
                auto d = assetRead(std::string("mods/ejemplo/") + f);
                if (d.empty()) continue;
                std::filesystem::create_directories(std::filesystem::path(ex + f).parent_path(), ec);
                if (SDL_RWops* o = SDL_RWFromFile((ex + f).c_str(), "wb")) { SDL_RWwrite(o, d.data(), 1, d.size()); SDL_RWclose(o); }
            }
            if (!std::filesystem::exists(g_modRoot + "mods.txt", ec))
                if (SDL_RWops* o = SDL_RWFromFile((g_modRoot + "mods.txt").c_str(), "wb")) { const char* t = "#ejemplo\n"; SDL_RWwrite(o, t, 1, strlen(t)); SDL_RWclose(o); }
        }
    }
#else
    g_modRoot = g_base + "mods/";
#endif
    {
    std::vector<std::string> found;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(std::filesystem::u8path(g_modRoot), ec))
        if (e.is_directory(ec)) found.push_back(e.path().filename().u8string());
    std::sort(found.begin(), found.end());

    std::vector<std::pair<std::string, bool>> order;
    auto txt = readFile(g_modRoot + "mods.txt");
    std::string all(txt.begin(), txt.end()), line;
    std::istringstream in(all);
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;
        bool on = line[0] != '#';
        std::string name = trim(on ? line : line.substr(1));
        if (std::find(found.begin(), found.end(), name) != found.end()) order.push_back({ name, on });
    }
    for (auto& n : found) {
        bool listed = false;
        for (auto& o : order) if (o.first == n) listed = true;
        if (!listed) order.push_back({ n, true });
    }
    for (auto& o : order) {
        ModInfo m;
        m.folder = o.first; m.enabled = o.second; m.name = o.first;
        auto info = readFile(g_modRoot + o.first + "/mod.txt");
        std::istringstream is(std::string(info.begin(), info.end()));
        while (std::getline(is, line)) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
            for (auto& c : k) c = (char)tolower((unsigned char)c);
            if (k == "name") m.name = v; else if (k == "author") m.author = v;
            else if (k == "description") m.description = v; else if (k == "version") m.version = v;
        }
        if (SDL_RWops* f = SDL_RWFromFile((g_modRoot + o.first + "/icon.png").c_str(), "rb")) { m.hasIcon = true; SDL_RWclose(f); }
        g_modList.push_back(m);
        if (m.enabled) g_modDirs.push_back(g_modRoot + o.first + "/");
        log("mod: %s (%s)%s", m.name.c_str(), m.folder.c_str(), m.enabled ? "" : " [disabled]");
    }
    }
}

const std::vector<ModInfo>& mods() { return g_modList; }

bool modsSave(const std::vector<ModInfo>& list) {
    std::string out = "# Orden de carga: gana el mod de más abajo. Una línea que empieza con # es un mod desactivado.\n";
    for (auto& m : list) out += (m.enabled ? "" : "#") + m.folder + "\n";
    SDL_RWops* f = SDL_RWFromFile((g_modRoot + "mods.txt").c_str(), "wb");
    if (!f) return false;
    SDL_RWwrite(f, out.data(), 1, out.size());
    SDL_RWclose(f);
    return true;
}

std::string modFilePath(const ModInfo& m, const std::string& file) { return g_modRoot + m.folder + "/" + file; }

std::vector<std::vector<uint8_t>> assetReadLayers(const std::string& name) {
    std::vector<std::vector<uint8_t>> out;
    std::vector<uint8_t> base;
    if (SDL_RWops* f = openBase(name)) {
        Sint64 n = SDL_RWsize(f);
        if (n > 0) { base.resize((size_t)n); if (SDL_RWread(f, base.data(), 1, (size_t)n) != (size_t)n) base.clear(); }
        SDL_RWclose(f);
    }
    out.push_back(std::move(base));
    for (auto& d : g_modDirs) out.push_back(readFile(d + name));
    return out;
}

#ifdef _WIN32
}
#include <windows.h>
namespace plat {
static HANDLE g_dPipe = INVALID_HANDLE_VALUE;
static std::string g_dClient, g_dImage = "xa", g_dImageText = "OpenXa";
static std::string g_dDetails, g_dState, g_dSent;
static bool g_dEnabled = false, g_dReady = false;
static Uint32 g_dNextTry = 0, g_dLastSend = 0;
static long long g_dStart = 0;

static std::string jsonEsc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if ((unsigned char)c < 0x20) o += ' ';
        else o += c;
    }
    return o;
}
static void dClose() {
    if (g_dPipe != INVALID_HANDLE_VALUE) CloseHandle(g_dPipe);
    g_dPipe = INVALID_HANDLE_VALUE; g_dReady = false; g_dSent.clear();
}
static bool dWrite(int op, const std::string& json) {
    if (g_dPipe == INVALID_HANDLE_VALUE) return false;
    std::string frame(8, '\0');
    const uint32_t len = (uint32_t)json.size(), opc = (uint32_t)op;
    memcpy(&frame[0], &opc, 4); memcpy(&frame[4], &len, 4);
    frame += json;
    DWORD wrote = 0;
    if (!WriteFile(g_dPipe, frame.data(), (DWORD)frame.size(), &wrote, nullptr) || wrote != frame.size()) { dClose(); return false; }
    return true;
}
static void dDrain() {
    if (g_dPipe == INVALID_HANDLE_VALUE) return;
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(g_dPipe, nullptr, 0, nullptr, &avail, nullptr)) { dClose(); return; }
        if (avail < 8) return;
        uint32_t hdr[2]; DWORD got = 0;
        if (!ReadFile(g_dPipe, hdr, 8, &got, nullptr) || got != 8) { dClose(); return; }
        std::string body(hdr[1], '\0');
        if (hdr[1] && (!ReadFile(g_dPipe, &body[0], hdr[1], &got, nullptr) || got != hdr[1])) { dClose(); return; }
        if (hdr[0] == 2) { log("discord: closed by Discord: %s", body.c_str()); dClose(); g_dNextTry = SDL_GetTicks() + 60000; return; }
        if (body.find("\"READY\"") != std::string::npos) { g_dReady = true; log("discord: connected"); }
    }
}
static void dConnect() {
    for (int i = 0; i < 10 && g_dPipe == INVALID_HANDLE_VALUE; i++) {
        char name[32]; snprintf(name, sizeof name, "\\\\.\\pipe\\discord-ipc-%d", i);
        g_dPipe = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    }
    if (g_dPipe == INVALID_HANDLE_VALUE) return;
    dWrite(0, "{\"v\":1,\"client_id\":\"" + jsonEsc(g_dClient) + "\"}");
}

void presenceInit() {
    auto d = assetRead("discord.txt");
    if (d.empty()) return;
    std::istringstream in(std::string(d.begin(), d.end()));
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos || line[0] == '#') continue;
        std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
        if (k == "client_id") g_dClient = v;
        else if (k == "large_image") g_dImage = v;
        else if (k == "large_text") g_dImageText = v;
    }
    if (g_dClient.empty()) { log("discord: mod enabled but discord.txt has no client_id"); return; }
    g_dEnabled = true;
    g_dStart = (long long)time(nullptr);
    log("discord: rich presence on");
}
void setPresence(const std::string& details, const std::string& state) { g_dDetails = details; g_dState = state; }
void presenceUpdate() {
    if (!g_dEnabled) return;
    const Uint32 now = SDL_GetTicks();
    if (g_dPipe == INVALID_HANDLE_VALUE) {
        if (now < g_dNextTry) return;
        g_dNextTry = now + 15000;
        dConnect();
        return;
    }
    dDrain();
    if (!g_dReady) return;
    const std::string want = g_dDetails + "\n" + g_dState;
    if (want == g_dSent || now - g_dLastSend < 5000) return;
    std::string act = "{\"details\":\"" + jsonEsc(g_dDetails) + "\"";
    if (!g_dState.empty()) act += ",\"state\":\"" + jsonEsc(g_dState) + "\"";
    act += ",\"timestamps\":{\"start\":" + std::to_string(g_dStart) + "}";
    act += ",\"assets\":{\"large_image\":\"" + jsonEsc(g_dImage) + "\",\"large_text\":\"" + jsonEsc(g_dImageText) + "\"}}";
    static int nonce = 0;
    if (dWrite(1, "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string((unsigned long)GetCurrentProcessId()) +
                  ",\"activity\":" + act + "},\"nonce\":\"" + std::to_string(++nonce) + "\"}")) {
        g_dSent = want; g_dLastSend = now;
    }
}
#else
void presenceInit() {}
void setPresence(const std::string&, const std::string&) {}
void presenceUpdate() {}
#endif

int SCREEN_W = 640, SCREEN_H = 448;
static SDL_Window* g_win = nullptr;
static SDL_Renderer* g_ren = nullptr;
static SDL_Rect g_baseVp = { 0, 0, 0, 0 };
static bool g_stretch = false, g_stretched = false;
static void applyLogicalSize() {
    int ow = 0, oh = 0;
    if (g_stretch && SDL_GetRendererOutputSize(g_ren, &ow, &oh) == 0 && ow > 0 && oh > 0) {
        SDL_RenderSetLogicalSize(g_ren, 0, 0);
        SDL_RenderSetViewport(g_ren, nullptr);
        SDL_RenderSetScale(g_ren, (float)ow / SCREEN_W, (float)oh / SCREEN_H);
        g_baseVp = { 0, 0, SCREEN_W, SCREEN_H };
        SDL_RenderSetViewport(g_ren, &g_baseVp);
        return;
    }
    SDL_RenderSetScale(g_ren, 1.0f, 1.0f);
    SDL_RenderSetLogicalSize(g_ren, SCREEN_W, SCREEN_H);
    SDL_RenderGetViewport(g_ren, &g_baseVp);
}
static void resetViewport() { SDL_RenderSetViewport(g_ren, &g_baseVp); }

void beginScaledSpace(float f) {
    if (!g_ren || f <= 0 || f == 1.0f) return;
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    const float w = SCREEN_W / f, h = SCREEN_H / f;
    if (g_stretch) {
        SDL_RenderSetLogicalSize(g_ren, 0, 0);
        SDL_RenderSetViewport(g_ren, nullptr);
        SDL_RenderSetScale(g_ren, ow / w, oh / h);
        return;
    }
    const float k = std::min((float)ow / SCREEN_W, (float)oh / SCREEN_H) * f;
    SDL_RenderSetLogicalSize(g_ren, 0, 0);
    SDL_RenderSetScale(g_ren, k, k);
    SDL_Rect v = { (int)std::lround((ow - w * k) * 0.5f / k), (int)std::lround((oh - h * k) * 0.5f / k), (int)std::lround(w), (int)std::lround(h) };
    SDL_RenderSetViewport(g_ren, &v);
}
void endScaledSpace() { if (g_ren) { g_stretched = false; applyLogicalSize(); } }

bool videoInit(int lw, int lh) {
    SCREEN_W = lw; SCREEN_H = lh;
    int ww = 800, wh = 600;
    if (lw * 3 > lh * 4) ww = wh * lw / lh;
#ifdef __ANDROID__
    const Uint32 winFlags = SDL_WINDOW_FULLSCREEN;
    { SDL_DisplayMode dm; if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { ww = std::max(dm.w, dm.h); wh = std::min(dm.w, dm.h); } }
#else
    const Uint32 winFlags = SDL_WINDOW_RESIZABLE;
    wh = 600; ww = wh * lw / lh;
#endif
    g_win = SDL_CreateWindow("Vascolet Collection", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, winFlags);
    if (!g_win) { log("SDL_CreateWindow: %s", SDL_GetError()); return false; }
    g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#ifndef __ANDROID__
    SDL_ShowCursor(SDL_DISABLE);
#endif
#ifdef HOST
    if (const char* e = getenv("XA_WINSIZE")) { int w = 0, h = 0; if (sscanf(e, "%dx%d", &w, &h) == 2) { SDL_SetWindowSize(g_win, w, h); SDL_PumpEvents(); } }
#endif
    if (!g_ren) { log("SDL_CreateRenderer: %s", SDL_GetError()); return false; }
    applyLogicalSize();
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    { int ow = 0, oh = 0; SDL_GetRendererOutputSize(g_ren, &ow, &oh); SDL_RendererInfo ri; SDL_GetRendererInfo(g_ren, &ri);
      log("video: logical %dx%d, window %dx%d, output %dx%d, renderer %s", lw, lh, ww, wh, ow, oh, ri.name); }
    return true;
}

SDL_Renderer* renderer() { return g_ren; }

bool wideScreenWanted() {
#if defined(__ANDROID__)
    return true;
#elif defined(HOST)
    const char* e = getenv("XA_WIDE");
    return !(e && *e == '0');
#else
    return false;
#endif
}

int wideLogicalWidth(int fixedW, int logicalH) {
#if defined(__ANDROID__)
    int w = 0, h = 0;
    if (g_ren) SDL_GetRendererOutputSize(g_ren, &w, &h);
    if (w <= 0 || h <= 0) { SDL_DisplayMode dm; if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { w = dm.w; h = dm.h; } }
    if (w <= 0 || h <= 0) return fixedW;
    float aspect = (float)std::max(w, h) / (float)std::min(w, h);
    int lw = (int)std::lround(logicalH * aspect) & ~1;
    return std::max(512, std::min(lw, 1024));
#elif defined(HOST)
    if (const char* e = getenv("XA_WIDE_W"); e && *e) return std::max(512, std::min(atoi(e) & ~1, 1024));
    (void)logicalH;
    return 512;
#else
    (void)logicalH;
    return fixedW;
#endif
}

void setLogicalWidth(int w) {
    SCREEN_W = w;
    if (g_ren) applyLogicalSize();
}

static bool g_followWindow = false;
static int g_refH = 600;
static void fitShapeToWindow() {
    if (!g_followWindow || !g_ren) return;
    int ow = 0, oh = 0;
    if (SDL_GetRendererOutputSize(g_ren, &ow, &oh) != 0 || ow <= 0 || oh <= 0) return;
    const bool fs = g_win && (SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN);
    const float k = fs ? oh / 384.0f : std::max(1, g_refH) / 384.0f;
    const int w = std::max(160, std::min((int)std::lround(ow / k) & ~1, 4096));
    const int h = std::max(120, std::min((int)std::lround(oh / k), 2160));
    if (w != SCREEN_W || h != SCREEN_H) { SCREEN_W = w; SCREEN_H = h; g_stretched = false; applyLogicalSize(); }
}
void setFollowWindowShape(bool on, int refH) {
    g_followWindow = on; g_refH = refH;
    fitShapeToWindow();
}
void setLogicalSize(int w, int h) {
    SCREEN_W = w; SCREEN_H = h;
    if (g_ren) applyLogicalSize();
}

bool wideCapable() {
#ifdef __ANDROID__
    return SCREEN_W > 512;
#else
    return true;
#endif
}

void fillTriangle(SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_Color col) {
    if (!g_ren) return;
    SDL_Vertex v[3] = { { a, col, { 0, 0 } }, { b, col, { 0, 0 } }, { c, col, { 0, 0 } } };
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(g_ren, nullptr, v, 3, nullptr, 0);
}

void fillRoundRect(SDL_FRect r, float rad, SDL_Color c) {
    if (!g_ren) return;
    rad = std::max(0.0f, std::min(rad, std::min(r.w, r.h) * 0.5f));
    std::vector<SDL_Vertex> v;
    auto tri = [&](float x0, float y0, float x1, float y1, float x2, float y2) {
        v.push_back({ { x0, y0 }, c, { 0, 0 } }); v.push_back({ { x1, y1 }, c, { 0, 0 } }); v.push_back({ { x2, y2 }, c, { 0, 0 } });
    };
    auto quad = [&](float x, float y, float w, float h) { if (w > 0 && h > 0) { tri(x, y, x + w, y, x, y + h); tri(x + w, y, x + w, y + h, x, y + h); } };
    quad(r.x + rad, r.y, r.w - 2 * rad, r.h);
    quad(r.x, r.y + rad, rad, r.h - 2 * rad);
    quad(r.x + r.w - rad, r.y + rad, rad, r.h - 2 * rad);
    const float cx[4] = { r.x + rad, r.x + r.w - rad, r.x + r.w - rad, r.x + rad };
    const float cy[4] = { r.y + rad, r.y + rad, r.y + r.h - rad, r.y + r.h - rad };
    for (int k = 0; k < 4; k++) {
        const int N = 6;
        for (int i = 0; i < N; i++) {
            float a0 = (k + 2) * 1.5707963f + i * 1.5707963f / N, a1 = a0 + 1.5707963f / N;
            tri(cx[k], cy[k], cx[k] + std::cos(a0) * rad, cy[k] + std::sin(a0) * rad, cx[k] + std::cos(a1) * rad, cy[k] + std::sin(a1) * rad);
        }
    }
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(g_ren, nullptr, v.data(), (int)v.size(), nullptr, 0);
}

void desktopSize(int* w, int* h) {
    SDL_DisplayMode dm;
    const int d = g_win ? std::max(0, SDL_GetWindowDisplayIndex(g_win)) : 0;
    if (SDL_GetDesktopDisplayMode(d, &dm) == 0) { *w = dm.w; *h = dm.h; } else { *w = *h = 0; }
}
void setWindowResolution(int w, int h) {
#ifndef __ANDROID__
    if (!g_win || (SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN)) return;
#ifdef HOST
    if (getenv("XA_WINSIZE")) return;
#endif
    int cw = 0, ch = 0;
    SDL_GetWindowSize(g_win, &cw, &ch);
    if (cw == w && ch == h) return;
    SDL_RestoreWindow(g_win);
    SDL_SetWindowSize(g_win, w, h);
    SDL_SetWindowPosition(g_win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
#else
    (void)w; (void)h;
#endif
}

bool touchEnabled() {
#if defined(__ANDROID__)
    return true;
#elif defined(HOST)
    static bool on = getenv("XA_TOUCH") != nullptr;
    return on;
#else
    return false;
#endif
}

bool touchControlsAvailable() {
#if defined(__ANDROID__)
    return !SDL_IsAndroidTV();
#else
    return touchEnabled();
#endif
}

void setStretch(bool on) {
#ifdef HOST
    if (getenv("XA_STRETCH")) on = true;
#endif
    if (on != g_stretch) { g_stretch = on; g_stretched = false; if (g_ren) applyLogicalSize(); }
}

static void restoreLogical() {
    if (!g_stretched) return;
    g_stretched = false;
    applyLogicalSize();
}

void setView43(bool on, int baseW) {
    if (!g_ren) return;
    const int BOX_H = 384;
    if (!on || (SCREEN_W == baseW && SCREEN_H == BOX_H)) { restoreLogical(); resetViewport(); return; }
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    if (g_stretch) {
        SDL_RenderSetLogicalSize(g_ren, 0, 0);
        SDL_RenderSetViewport(g_ren, nullptr);
        SDL_RenderSetScale(g_ren, (float)ow / baseW, (float)oh / BOX_H);
        g_stretched = true;
        return;
    }
    if (baseW <= SCREEN_W && BOX_H <= SCREEN_H) {
        restoreLogical();
        SDL_Rect v = { g_baseVp.x + (SCREEN_W - baseW) / 2, g_baseVp.y + (SCREEN_H - BOX_H) / 2, baseW, BOX_H };
        SDL_RenderSetViewport(g_ren, &v);
        return;
    }
    const float kpx = std::min((float)ow / SCREEN_W, (float)oh / SCREEN_H);
    const float s = std::min((float)SCREEN_W / baseW, (float)SCREEN_H / BOX_H) * kpx;
    SDL_RenderSetLogicalSize(g_ren, 0, 0);
    SDL_RenderSetViewport(g_ren, nullptr);
    SDL_RenderSetScale(g_ren, s, s);
    SDL_Rect v = { (int)std::lround((ow - baseW * s) * 0.5f / s), (int)std::lround((oh - BOX_H * s) * 0.5f / s), baseW, BOX_H };
    SDL_RenderSetViewport(g_ren, &v);
    g_stretched = true;
}

static void fullScreenRect(SDL_Color c, SDL_BlendMode mode) {
    SDL_Rect vp; SDL_RenderGetViewport(g_ren, &vp);
    const bool stretched = g_stretched;
    restoreLogical();
    resetViewport();
    SDL_SetRenderDrawBlendMode(g_ren, mode);
    SDL_SetRenderDrawColor(g_ren, c.r, c.g, c.b, c.a);
    SDL_Rect all = { 0, 0, SCREEN_W, SCREEN_H };
    SDL_RenderFillRect(g_ren, &all);
    if (stretched) setView43(true, vp.w);
    else if (vp.w != SCREEN_W || vp.x != 0) SDL_RenderSetViewport(g_ren, &vp);
}
void fillScreen(SDL_Color c) { if (g_ren) fullScreenRect({ c.r, c.g, c.b, 255 }, SDL_BLENDMODE_NONE); }
void dimScreen(float a) {
    if (!g_ren || a <= 0) return;
    fullScreenRect({ 0, 0, 0, (Uint8)(std::min(a, 1.0f) * 255) }, SDL_BLENDMODE_BLEND);
}

void dimMargins(Uint8 alpha) {
    if (!g_ren || g_stretched) return;
    SDL_Rect vp; SDL_RenderGetViewport(g_ren, &vp);
    vp.x -= g_baseVp.x; vp.y -= g_baseVp.y;
    if (vp.x <= 0 && vp.w >= SCREEN_W) return;
    resetViewport();
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, alpha);
    SDL_Rect l = { 0, 0, vp.x, SCREEN_H }, r = { vp.x + vp.w, 0, SCREEN_W - (vp.x + vp.w), SCREEN_H };
    SDL_RenderFillRect(g_ren, &l);
    SDL_RenderFillRect(g_ren, &r);
    vp.x += g_baseVp.x; vp.y += g_baseVp.y;
    SDL_RenderSetViewport(g_ren, &vp);
}

SDL_Color imagePixel(const std::string& name, int x, int y) {
    SDL_Color c = { 0, 0, 0, 255 };
    auto data = assetRead(name);
    SDL_Surface* raw = data.empty() ? nullptr : IMG_Load_RW(SDL_RWFromConstMem(data.data(), (int)data.size()), 1);
    if (!raw) return c;
    if (SDL_Surface* s = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0)) {
        x = std::max(0, std::min(x, s->w - 1)); y = std::max(0, std::min(y, s->h - 1));
        const uint8_t* p = (const uint8_t*)s->pixels + y * s->pitch + x * 4;
        c = { p[0], p[1], p[2], 255 };
        SDL_FreeSurface(s);
    }
    SDL_FreeSurface(raw);
    return c;
}

int g_pagesLoaded = 0, g_pagesSkipped = 0;

static SDL_Texture* pageTexture(SDL_Surface* src, const SDL_Rect* sr, bool opaque) {
    int w = sr ? sr->w : src->w, h = sr ? sr->h : src->h;
    Uint32 pf = SDL_PIXELFORMAT_RGBA32;
    SDL_Surface* p = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE);
    SDL_BlitSurface(src, sr, p, nullptr);
    SDL_Texture* t = SDL_CreateTextureFromSurface(g_ren, p);
    SDL_FreeSurface(p);
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

Image* imageFromRGBA(int w, int h, const uint8_t* rgba) {
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom((void*)rgba, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    if (!s) return nullptr;
    SDL_Rect r = { 0, 0, w, h };
    SDL_Texture* t = pageTexture(s, &r, false);
    SDL_FreeSurface(s);
    if (!t) return nullptr;
    Image* img = new Image;
    img->w = w; img->h = h; img->cols = img->rows = 1;
    img->pages.assign(1, t);
    return img;
}

Image* loadImageFile(const std::string& path, bool gray) {
    auto d = readFile(path);
    if (d.empty()) return nullptr;
    SDL_Surface* raw = IMG_Load_RW(SDL_RWFromConstMem(d.data(), (int)d.size()), 1);
    if (!raw) return nullptr;
    SDL_Surface* s = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw);
    if (!s) return nullptr;
    std::vector<uint8_t> px((size_t)s->w * s->h * 4);
    for (int y = 0; y < s->h; y++) memcpy(px.data() + (size_t)y * s->w * 4, (uint8_t*)s->pixels + y * s->pitch, (size_t)s->w * 4);
    if (gray)
        for (size_t i = 0; i < px.size(); i += 4) { uint8_t l = (uint8_t)(px[i] * 0.30f + px[i + 1] * 0.59f + px[i + 2] * 0.11f); px[i] = px[i + 1] = px[i + 2] = l; }
    Image* img = imageFromRGBA(s->w, s->h, px.data());
    SDL_FreeSurface(s);
    if (img) for (auto* t : img->pages) if (t) SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
    return img;
}

struct PageInfo { int w, h, opaque; };
static std::map<std::string, PageInfo> g_pageIdx;
static void loadPageIndex() {
    auto d = assetRead("assets/pages.idx");
    d.push_back(0);
    const char* p = (const char*)d.data();
    while (*p) {
        const char* tab = strchr(p, '\t'); const char* nl = strchr(p, '\n');
        if (!nl) nl = p + strlen(p);
        if (tab && tab < nl) {
            PageInfo pi = {0, 0, 0};
            if (sscanf(tab + 1, "%d %d %d", &pi.w, &pi.h, &pi.opaque) >= 2) g_pageIdx[norm(std::string(p, tab - p))] = pi;
        }
        p = *nl ? nl + 1 : nl;
    }
    log("pak: page index %zu images", g_pageIdx.size());
}

struct Decoded {
    bool ok = false;
    PageInfo pi = {0, 0, 0};
    int cols = 0, rows = 0;
    std::vector<std::pair<int, SDL_Surface*>> pages;
    std::vector<int> failed;
};
static int evictPages(uint32_t minIdle, uint32_t targetKB);
static Decoded decodePaged(const std::string& name, const std::vector<SDL_Rect>* used, bool quiet);

static std::set<Image*> g_images;
static uint32_t g_useClock = 0;
static Image* buildPaged(Decoded& d, Image* into = nullptr) {
    Image* img = into;
    if (!img) {
        img = new Image;
        img->w = d.pi.w; img->h = d.pi.h; img->cols = d.cols; img->rows = d.rows;
        img->pages.assign(d.cols * d.rows, nullptr);
        img->tried.assign(d.cols * d.rows, 0);
        img->used.assign(d.cols * d.rows, 0);
        g_images.insert(img);
    }
    for (auto& p : d.pages) {
        SDL_Texture* t = pageTexture(p.second, nullptr, d.pi.opaque != 0);
        SDL_FreeSurface(p.second);
        if (!t) error("texture page %d: %s", p.first, SDL_GetError());
        g_pagesLoaded++;
        if (img->pages[p.first]) SDL_DestroyTexture(img->pages[p.first]);
        img->pages[p.first] = t;
        img->used[p.first] = g_useClock;
    }
    d.pages.clear();
    return img;
}

struct PrefetchJob { std::string name; std::vector<SDL_Rect> used; bool hasUsed; bool done = false, started = false; Decoded result; };
static SDL_mutex* g_pfLock = nullptr;
static SDL_cond* g_pfCond = nullptr;
static SDL_Thread* g_pfThread = nullptr;
static std::vector<PrefetchJob*> g_pfJobs;
static int prefetchWorker(void*) {
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
    for (;;) {
        SDL_LockMutex(g_pfLock);
        PrefetchJob* job = nullptr;
        while (!job) {
            for (auto* j : g_pfJobs) if (!j->done && !j->started) { job = j; break; }
            if (!job) SDL_CondWait(g_pfCond, g_pfLock);
        }
        job->started = true;
        std::string name = job->name; std::vector<SDL_Rect> used = job->used; bool hasUsed = job->hasUsed;
        SDL_UnlockMutex(g_pfLock);
        Decoded d = decodePaged(name, hasUsed ? &used : nullptr, true);
        SDL_LockMutex(g_pfLock);
        job->result = std::move(d); job->done = true;
        SDL_CondBroadcast(g_pfCond);
        SDL_UnlockMutex(g_pfLock);
    }
    return 0;
}
void prefetchImage(const std::string& name, const std::vector<SDL_Rect>* used) {
    if (!g_pageIdx.count(norm(name))) return;
    if (!g_pfThread) {
        g_pfLock = SDL_CreateMutex(); g_pfCond = SDL_CreateCond();
        g_pfThread = SDL_CreateThreadWithStackSize(prefetchWorker, "prefetch", 64 * 1024, nullptr);
        if (!g_pfThread) { log("prefetch thread: %s", SDL_GetError()); return; }
    }
    SDL_LockMutex(g_pfLock);
    for (auto* j : g_pfJobs) if (j->name == name) { SDL_UnlockMutex(g_pfLock); return; }
    auto* job = new PrefetchJob;
    job->name = name; job->hasUsed = used != nullptr; if (used) job->used = *used;
    g_pfJobs.push_back(job);
    SDL_CondBroadcast(g_pfCond);
    SDL_UnlockMutex(g_pfLock);
}

void cancelPrefetches() {
    if (!g_pfThread) return;
    SDL_LockMutex(g_pfLock);
    for (auto it = g_pfJobs.begin(); it != g_pfJobs.end();) {
        PrefetchJob* j = *it;
        if (j->started && !j->done) { ++it; continue; }
        for (auto& p : j->result.pages) SDL_FreeSurface(p.second);
        delete j;
        it = g_pfJobs.erase(it);
    }
    SDL_UnlockMutex(g_pfLock);
}

static bool takePrefetched(const std::string& name, Decoded& out) {
    if (!g_pfThread) return false;
    SDL_LockMutex(g_pfLock);
    PrefetchJob* job = nullptr;
    for (auto* j : g_pfJobs) if (j->name == name) job = j;
    if (job) {
        while (!job->done) {
            if (SDL_CondWaitTimeout(g_pfCond, g_pfLock, 3000) == SDL_MUTEX_TIMEDOUT)
                log("prefetch: still waiting for %s (started=%d)", name.c_str(), (int)job->started);
        }
        out = std::move(job->result);
        g_pfJobs.erase(std::find(g_pfJobs.begin(), g_pfJobs.end(), job));
        delete job;
    }
    SDL_UnlockMutex(g_pfLock);
    return job != nullptr;
}

static Image* loadPaged(const std::string& name, const std::vector<SDL_Rect>* used) {
    Decoded d;
    if (!takePrefetched(name, d)) d = decodePaged(name, used, false);
    if (!d.ok) return nullptr;
    Image* img = buildPaged(d);
    img->src = name;

    for (int r = 0; r < img->rows; r++)
        for (int c = 0; c < img->cols; c++) {
            SDL_Rect pr = { c * img->page, r * img->page, img->page, img->page };
            bool asked = !used;
            if (used) for (auto& u : *used) { SDL_Rect x; if (SDL_IntersectRect(&pr, &u, &x)) { asked = true; break; } }
            if (asked) img->tried[r * img->cols + c] = 1;
        }
    return img;
}

void ensurePages(Image* img, const SDL_Rect& r) {
    if (!img || img->src.empty() || img->tried.empty()) return;
    const int P = img->page;
    std::vector<SDL_Rect> want;
    for (int y = std::max(0, r.y / P); y <= std::min(img->rows - 1, (r.y + r.h - 1) / P); y++)
        for (int x = std::max(0, r.x / P); x <= std::min(img->cols - 1, (r.x + r.w - 1) / P); x++) {
            int i = y * img->cols + x;
            if (img->tried[i]) continue;
            img->tried[i] = 1;
            want.push_back({ x * P + 1, y * P + 1, P - 2, P - 2 });
        }
    if (want.empty()) return;
    Decoded d = decodePaged(img->src, &want, false);
    if (d.ok) buildPaged(d, img);
}

static Decoded decodePaged(const std::string& name, const std::vector<SDL_Rect>* used, bool quiet) {
    Decoded out;
    PageInfo pi = {0, 0, 0};
    auto pit = g_pageIdx.find(norm(name));
    if (pit != g_pageIdx.end()) pi = pit->second;
    else {
        auto info = assetRead(name + ".pages/info.txt");
        info.push_back(0);
        if (sscanf((const char*)info.data(), "%d %d %d", &pi.w, &pi.h, &pi.opaque) < 2) return out;
    }
    int w = pi.w, h = pi.h;
    const int PAGE = 256;
    out.pi = pi; out.ok = true;
    out.cols = (w + PAGE - 1) / PAGE;
    out.rows = (h + PAGE - 1) / PAGE;
    struct { int page = PAGE, cols, rows; } geo; geo.cols = out.cols; geo.rows = out.rows;
    auto* img = &geo;

    struct Need { int idx; std::string name; PakEnt e; };
    std::vector<Need> need;
    for (int r = 0; r < img->rows; r++)
        for (int c = 0; c < img->cols; c++) {
            SDL_Rect sr = { c * img->page, r * img->page, std::min(img->page, w - c * img->page), std::min(img->page, h - r * img->page) };
            bool needed = !used;
            if (used) for (auto& u : *used) { SDL_Rect x; if (SDL_IntersectRect(&sr, &u, &x)) { needed = true; break; } }
            char pn[32]; snprintf(pn, sizeof pn, ".pages/%d_%d.png", c, r);
            auto it = g_dir.find(norm(name + pn));
            if (it == g_dir.end()) {
                snprintf(pn, sizeof pn, ".pages/%d_%d.jpg", c, r);
                it = g_dir.find(norm(name + pn));
            }
            if (!needed || it == g_dir.end()) { if (!quiet) g_pagesSkipped++; continue; }
            need.push_back({ r * img->cols + c, name + pn, it->second });
        }
    std::sort(need.begin(), need.end(), [](const Need& a, const Need& b) { return a.e.off < b.e.off; });

    const uint32_t MAX_GAP = 48 * 1024;
    for (size_t i = 0; i < need.size();) {
        size_t j = i + 1;
        uint32_t start = need[i].e.off, end = need[i].e.off + need[i].e.size;
        while (j < need.size() && need[j].e.off >= end && need[j].e.off - end <= MAX_GAP) { end = need[j].e.off + need[j].e.size; j++; }
        std::vector<uint8_t> buf(end - start);
        bool ok = readAt(start, buf.data(), buf.size());
        for (size_t k = i; k < j; k++) {
            SDL_Surface* s = ok ? IMG_Load_RW(SDL_RWFromConstMem(buf.data() + (need[k].e.off - start), (int)need[k].e.size), 1) : nullptr;
            if (!s) { if (!quiet) error("IMG_Load %s: %s", need[k].name.c_str(), IMG_GetError()); out.failed.push_back(need[k].idx); continue; }
            out.pages.push_back({ need[k].idx, s });
        }
        i = j;
    }
    return out;
}

static Image* loadImageImpl(const std::string& name, std::vector<uint8_t>* alphaOut, const std::vector<SDL_Rect>* used);
Image* loadImage(const std::string& name, std::vector<uint8_t>* alphaOut, const std::vector<SDL_Rect>* used) {
    uint32_t t0 = SDL_GetTicks();
    Image* img = loadImageImpl(name, alphaOut, used);
    uint32_t d = SDL_GetTicks() - t0;
    log("image %s %u ms, heap %u KB", name.c_str(), d, freeMemoryKB());
    return img;
}
static void dehalftone(SDL_Surface* s) {
    const int w = s->w, h = s->h;
    auto px = [&](int x, int y) { return (Uint8*)s->pixels + y * s->pitch + x * 4; };
    auto opaque = [&](int x, int y) { return x >= 0 && y >= 0 && x < w && y < h && px(x, y)[3] != 0; };
    std::vector<int> clear;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            Uint8* p = px(x, y);
            if (p[3] != 255 || p[0] >= 80 || p[1] >= 80 || p[2] >= 80) continue;
            int see = !opaque(x - 1, y) + !opaque(x + 1, y) + !opaque(x, y - 1) + !opaque(x, y + 1);
            if (see >= 2) clear.push_back(y * w + x);
        }
    for (int i : clear) { Uint8* p = px(i % w, i / w); p[0] = p[1] = p[2] = p[3] = 0; }
}

static void unsurround(SDL_Surface* s) {
    const float D = 0.6f;
    for (int y = 0; y < s->h; y++) {
        Uint8* p = (Uint8*)s->pixels + y * s->pitch;
        for (int x = 0; x < s->w; x++, p += 4) {
            if (p[3] == 0 || p[3] == 255 || p[0] > 8 || p[1] > 8 || p[2] > 8) continue;
            const float a = p[3] / 255.0f, a2 = 1.0f - (1.0f - a) / (1.0f - D);
            p[3] = (Uint8)(std::max(0.0f, a2) * 255.0f + 0.5f);
        }
    }
}

static void eraseRect(SDL_Surface* s, int x0, int y0, int x1, int y1) {
    x0 = std::max(0, x0); y0 = std::max(1, y0);
    x1 = std::min(s->w, x1); y1 = std::min(s->h - 1, y1);
    auto px = [&](int x, int y) { return (Uint8*)s->pixels + y * s->pitch + x * 4; };
    for (int x = x0; x < x1; x++) {
        const Uint8* a = px(x, y0 - 1);
        const Uint8* b = px(x, y1);
        for (int y = y0; y < y1; y++) {
            const float t = (y - y0 + 1) / (float)(y1 - y0 + 1);
            Uint8* p = px(x, y);
            for (int c = 0; c < 4; c++) p[c] = (Uint8)(a[c] + (b[c] - a[c]) * t + 0.5f);
        }
    }
}

static void limeLettering(SDL_Surface* s, Uint8 fr = 210, Uint8 fg = 255, Uint8 fb = 0) {
    for (int y = 0; y < s->h; y++) {
        Uint8* p = (Uint8*)s->pixels + y * s->pitch;
        for (int x = 0; x < s->w; x++, p += 4) {
            if (!p[3]) continue;
            const float k = std::min(1.0f, (p[1] + p[2]) / 400.0f);
            p[0] = (Uint8)(26 + (fr - 26) * k);
            p[1] = (Uint8)(24 + (fg - 24) * k);
            p[2] = (Uint8)(24 + (fb - 24) * k);
        }
    }
}

static void bleedAlpha(SDL_Surface* s) {
    const int w = s->w, h = s->h;
    auto px = [&](int x, int y) { return (Uint8*)s->pixels + y * s->pitch + x * 4; };
    std::vector<uint8_t> done((size_t)w * h);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) done[(size_t)y * w + x] = px(x, y)[3] != 0;
    std::vector<std::pair<int, std::array<Uint8, 3>>> next;
    for (int pass = 0; pass < 1; pass++) {
        next.clear();
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                if (done[(size_t)y * w + x]) continue;
                int r = 0, g = 0, b = 0, n = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int xx = x + dx, yy = y + dy;
                        if ((dx || dy) && xx >= 0 && yy >= 0 && xx < w && yy < h && done[(size_t)yy * w + xx]) {
                            const Uint8* q = px(xx, yy); r += q[0]; g += q[1]; b += q[2]; n++;
                        }
                    }
                if (n) next.push_back({ y * w + x, { (Uint8)(r / n), (Uint8)(g / n), (Uint8)(b / n) } });
            }
        if (next.empty()) break;
        for (auto& e : next) { Uint8* p = px(e.first % w, e.first / w); p[0] = e.second[0]; p[1] = e.second[1]; p[2] = e.second[2]; done[e.first] = 1; }
    }
}

// ---- Calidad de imagen (Gráficos: NITIDEZ y CALIDAD HD) ----
static int g_sharpness = 0;      // 0 = no, 1 = media, 2 = alta
static bool g_hdTextures = false; // texturas reescaladas 2x

// Máscara de enfoque 3x3 sobre RGB (el alfa no se toca).
static void sharpenSurface(SDL_Surface* s, float amt) {
    const int w = s->w, h = s->h;
    if (w < 3 || h < 3 || amt <= 0) return;
    std::vector<uint8_t> src((size_t)w * h * 4);
    for (int y = 0; y < h; y++) memcpy(&src[(size_t)y * w * 4], (uint8_t*)s->pixels + y * s->pitch, (size_t)w * 4);
    auto at = [&](int x, int y) { x = std::max(0, std::min(x, w - 1)); y = std::max(0, std::min(y, h - 1)); return &src[((size_t)y * w + x) * 4]; };
    for (int y = 0; y < h; y++) {
        uint8_t* row = (uint8_t*)s->pixels + y * s->pitch;
        for (int x = 0; x < w; x++) {
            const uint8_t* c = at(x, y);
            if (!c[3]) continue;
            for (int ch = 0; ch < 3; ch++) {
                int sum = 0;
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) sum += at(x + dx, y + dy)[ch];
                const float v = c[ch] + amt * (c[ch] - sum / 9.0f);
                row[x * 4 + ch] = (uint8_t)std::max(0.0f, std::min(255.0f, v + 0.5f));
            }
        }
    }
}

// Reescalado 2x con interpolación bicúbica Catmull-Rom (separable, 4 filas en caché).
static SDL_Surface* upscale2x(SDL_Surface* s) {
    const int w = s->w, h = s->h, W = w * 2, H = h * 2;
    SDL_Surface* o = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
    if (!o) return nullptr;
    auto weights = [](float f, float* wt) {
        const float f2 = f * f, f3 = f2 * f;
        wt[0] = (-f3 + 2 * f2 - f) * 0.5f; wt[1] = (3 * f3 - 5 * f2 + 2) * 0.5f;
        wt[2] = (-3 * f3 + 4 * f2 + f) * 0.5f; wt[3] = (f3 - f2) * 0.5f;
    };
    float wEven[4], wOdd[4];
    weights(0.75f, wEven); weights(0.25f, wOdd); // salida par: base i-2, impar: base i-1
    auto taps = [&](int X, int n, int* idx) -> const float* {
        const int i = X >> 1, base = (X & 1) ? i - 1 : i - 2;
        for (int k = 0; k < 4; k++) idx[k] = std::max(0, std::min(base + k, n - 1));
        return (X & 1) ? wOdd : wEven;
    };
    std::vector<float> cache(4 * (size_t)W * 4);
    int tag[4] = { -1, -1, -1, -1 };
    auto hrow = [&](int sy) -> const float* {
        const int slot = sy & 3;
        float* dst = &cache[(size_t)slot * W * 4];
        if (tag[slot] == sy) return dst;
        tag[slot] = sy;
        const uint8_t* row = (const uint8_t*)s->pixels + sy * s->pitch;
        for (int X = 0; X < W; X++) {
            int idx[4]; const float* wt = taps(X, w, idx);
            for (int ch = 0; ch < 4; ch++) {
                float v = 0;
                for (int k = 0; k < 4; k++) v += wt[k] * row[idx[k] * 4 + ch];
                dst[X * 4 + ch] = v;
            }
        }
        return dst;
    };
    for (int Y = 0; Y < H; Y++) {
        int idx[4]; const float* wt = taps(Y, h, idx);
        // filas consecutivas (o repetidas por el borde): nunca comparten slot siendo distintas
        const float* r[4];
        for (int k = 0; k < 4; k++) r[k] = hrow(idx[k]);
        uint8_t* out = (uint8_t*)o->pixels + Y * o->pitch;
        for (int X = 0; X < W * 4; X++) {
            const float v = wt[0] * r[0][X] + wt[1] * r[1][X] + wt[2] * r[2][X] + wt[3] * r[3][X];
            out[X] = (uint8_t)std::max(0.0f, std::min(255.0f, v + 0.5f));
        }
    }
    return o;
}

static int maxTextureSize() {
    SDL_RendererInfo ri;
    if (!g_ren || SDL_GetRendererInfo(g_ren, &ri) != 0 || !ri.max_texture_width) return 4096;
    return std::min(ri.max_texture_width, ri.max_texture_height);
}

// Aplica la calidad elegida a una superficie RGBA32. Devuelve la superficie a usar
// (puede ser nueva; la original sigue siendo del llamador) y el factor de escala.
static SDL_Surface* enhanceSurface(SDL_Surface* s, int largestPage, int* scale) {
    *scale = 1;
    if (g_sharpness == 0 && !g_hdTextures) return s;
    SDL_Surface* e = s;
    if (g_hdTextures && largestPage * 2 <= maxTextureSize()) {
        if (SDL_Surface* u = upscale2x(s)) { e = u; *scale = 2; }
    }
    if (g_sharpness) {
        if (e == s) { e = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0); if (!e) return s; }
        sharpenSurface(e, g_sharpness == 1 ? 0.45f : 0.9f);
    }
    return e;
}

static Image* loadImageImpl(const std::string& fullName, std::vector<uint8_t>* alphaOut, const std::vector<SDL_Rect>* used) {
    const size_t q = fullName.find('?');
    const std::string name = q == std::string::npos ? fullName : fullName.substr(0, q);
    const bool unshade = fullName.find("?dehalftone") != std::string::npos;
    const bool lime = fullName.find("?lime") != std::string::npos;
    const bool unsur = fullName.find("?unsurround") != std::string::npos;
    std::vector<std::array<int, 4>> erases;
    for (size_t ep = fullName.find("?erase="); ep != std::string::npos; ep = fullName.find("?erase=", ep + 1)) {
        std::array<int, 4> r;
        if (sscanf(fullName.c_str() + ep + 7, "%d,%d,%d,%d", &r[0], &r[1], &r[2], &r[3]) == 4) erases.push_back(r);
    }
    if (!alphaOut && q == std::string::npos && assetExists(name + ".pages/info.txt")) return loadPaged(name, used);
    SDL_Surface* raw = nullptr;
    {
        auto data = assetRead(name);
        if (data.empty()) { error("missing image %s", name.c_str()); return nullptr; }
        raw = IMG_Load_RW(SDL_RWFromConstMem(data.data(), (int)data.size()), 1);
    }
    if (!raw) { error("IMG_Load %s: %s", name.c_str(), IMG_GetError()); return nullptr; }
    bool opaque = !SDL_ISPIXELFORMAT_ALPHA(raw->format->format);
    SDL_Surface* s = raw;
    if (raw->format->format != SDL_PIXELFORMAT_RGBA32) {
        s = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(raw);
    }
    if (!s) { error("convert %s: %s", name.c_str(), SDL_GetError()); return nullptr; }
    if (unshade) dehalftone(s);
    if (lime) limeLettering(s);
    if (fullName.find("?yellow") != std::string::npos) limeLettering(s, 255, 232, 0);
    if (fullName.find("?white") != std::string::npos) limeLettering(s, 255, 255, 255);
    if (fullName.find("?silhouette") != std::string::npos)
        for (int y = 0; y < s->h; y++) { Uint8* q = (Uint8*)s->pixels + y * s->pitch; for (int x = 0; x < s->w; x++, q += 4) q[0] = q[1] = q[2] = 255; }
    for (auto& r : erases) eraseRect(s, r[0], r[1], r[2], r[3]);
    if (unsur) unsurround(s);
    if (!opaque) bleedAlpha(s);
    SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_NONE);
    if (alphaOut) {
        alphaOut->resize((size_t)s->w * s->h);
        for (int y = 0; y < s->h; y++) {
            const uint8_t* row = (const uint8_t*)s->pixels + y * s->pitch;
            for (int x = 0; x < s->w; x++) (*alphaOut)[(size_t)y * s->w + x] = row[x * 4 + 3];
        }
    }

    Image* img = new Image;
    img->w = s->w; img->h = s->h;
    img->cols = (s->w + img->page - 1) / img->page;
    img->rows = (s->h + img->page - 1) / img->page;
    int ek = 1;
    SDL_Surface* es = enhanceSurface(s, std::min(img->page, std::max(s->w, s->h)), &ek);
    SDL_SetSurfaceBlendMode(es, SDL_BLENDMODE_NONE);
    for (int r = 0; r < img->rows; r++)
        for (int c = 0; c < img->cols; c++) {
            SDL_Rect sr = { c * img->page, r * img->page,
                            std::min(img->page, s->w - c * img->page),
                            std::min(img->page, s->h - r * img->page) };

            bool needed = true;
            if (needed) {
                bool any = false;
                for (int y = sr.y; y < sr.y + sr.h && !any; y++) {
                    const uint8_t* row = (const uint8_t*)s->pixels + y * s->pitch;
                    for (int x = sr.x; x < sr.x + sr.w; x++) if (row[x * 4 + 3]) { any = true; break; }
                }
                needed = any;
            }
            if (!needed) { img->pages.push_back(nullptr); g_pagesSkipped++; continue; }
            Uint32 pf = SDL_PIXELFORMAT_RGBA32;
            SDL_Rect er = { sr.x * ek, sr.y * ek, sr.w * ek, sr.h * ek };
            SDL_Surface* p = SDL_CreateRGBSurfaceWithFormat(0, er.w, er.h, SDL_BITSPERPIXEL(pf), pf);
            SDL_BlitSurface(es, &er, p, nullptr);
            SDL_Texture* t = SDL_CreateTextureFromSurface(g_ren, p);
            SDL_FreeSurface(p);
            g_pagesLoaded++;
            if (!t) error("texture %s page %d,%d: %s", name.c_str(), c, r, SDL_GetError());
            else SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
            img->pages.push_back(t);
        }
    if (es != s) SDL_FreeSurface(es);
    SDL_FreeSurface(s);
    img->full = fullName;
    g_images.insert(img);
    return img;
}

Image* loadTileset(const std::string& name, int tw, int th) {
    auto data = assetRead(name);
    SDL_Surface* raw = data.empty() ? nullptr : IMG_Load_RW(SDL_RWFromConstMem(data.data(), (int)data.size()), 1);
    SDL_Surface* s = raw ? SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0) : nullptr;
    if (raw) SDL_FreeSurface(raw);
    if (!s || tw <= 0 || th <= 0) { if (s) SDL_FreeSurface(s); return loadImage(name); }
    const int cols = s->w / tw, rows = s->h / th, cw = tw + 2, ch = th + 2;
    const int aw = cols * cw, ah = rows * ch;
    SDL_RendererInfo ri; SDL_GetRendererInfo(g_ren, &ri);
    if (cols <= 0 || rows <= 0 || (ri.max_texture_width && aw > ri.max_texture_width) || (ri.max_texture_height && ah > ri.max_texture_height)) {
        SDL_FreeSurface(s);
        return loadImage(name);
    }
    SDL_Surface* a = SDL_CreateRGBSurfaceWithFormat(0, aw, ah, 32, SDL_PIXELFORMAT_RGBA32);
    auto src = [&](int x, int y) { return (const uint32_t*)((const uint8_t*)s->pixels + y * s->pitch) + x; };
    auto dst = [&](int x, int y) { return (uint32_t*)((uint8_t*)a->pixels + y * a->pitch) + x; };
    for (int ty = 0; ty < rows; ty++)
        for (int tx = 0; tx < cols; tx++)
            for (int y = -1; y <= th; y++) {
                const int sy = ty * th + std::max(0, std::min(y, th - 1));
                for (int x = -1; x <= tw; x++) {
                    const int sx = tx * tw + std::max(0, std::min(x, tw - 1));
                    *dst(tx * cw + 1 + x, ty * ch + 1 + y) = *src(sx, sy);
                }
            }
    int ek = 1;
    SDL_Surface* ea = enhanceSurface(a, std::max(aw, ah), &ek);
    SDL_Texture* t = SDL_CreateTextureFromSurface(g_ren, ea);
    if (ea != a) SDL_FreeSurface(ea);
    SDL_FreeSurface(a);
    Image* img = new Image;
    img->w = s->w; img->h = s->h;
    SDL_FreeSurface(s);
    if (!t) { error("tileset %s: %s", name.c_str(), SDL_GetError()); delete img; return loadImage(name); }
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest);
    img->page = std::max(aw, ah); img->cols = img->rows = 1;
    img->pages.assign(1, t);
    img->tileW = tw; img->tileH = th; img->tileCols = cols;
    img->full = name;
    g_images.insert(img);
    log("tileset %s: %dx%d tiles, atlas %dx%d", name.c_str(), cols, rows, aw, ah);
    return img;
}

// Recarga las texturas con la calidad actual (las paginadas se recargan bajo demanda).
static void reloadImages() {
    std::vector<Image*> list(g_images.begin(), g_images.end());
    for (Image* im : list) {
        if (!im->src.empty()) {
            for (size_t i = 0; i < im->pages.size(); i++) {
                if (im->pages[i]) SDL_DestroyTexture(im->pages[i]);
                im->pages[i] = nullptr;
                if (i < im->tried.size()) im->tried[i] = 0;
            }
            continue;
        }
        if (im->full.empty()) continue;
        Image* n = im->tileW ? loadTileset(im->full, im->tileW, im->tileH) : loadImageImpl(im->full, nullptr, nullptr);
        if (!n) continue;
        g_images.erase(n);
        if (n->pages.size() == im->pages.size() && n->tileW == im->tileW && n->w == im->w && n->h == im->h) im->pages.swap(n->pages);
        for (auto* t : n->pages) if (t) SDL_DestroyTexture(t);
        delete n;
    }
}

void setImageQuality(int sharpness, bool hd) {
    sharpness = std::max(0, std::min(sharpness, 2));
    if (sharpness == g_sharpness && hd == g_hdTextures) return;
    g_sharpness = sharpness; g_hdTextures = hd;
    if (g_ren) reloadImages();
}
bool hdGraphics() { return g_hdTextures; }
void setModernArt(bool on) {
    if (on == g_modernArt) return;
    g_modernArt = on;
    if (g_ren) reloadImages();
}

static int g_highFps = -1;
bool highFps() { return g_highFps == 1; }
void setHighFps(bool on) {
    if (g_highFps == (on ? 1 : 0)) return;
    g_highFps = on ? 1 : 0;
#ifdef __ANDROID__
    // XaActivity.setHighRefresh elige el modo de pantalla con más Hz (o 60 Hz si está apagado).
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject act = env ? (jobject)SDL_AndroidGetActivity() : nullptr;
    if (!act) return;
    jclass cls = env->GetObjectClass(act);
    jmethodID m = cls ? env->GetStaticMethodID(cls, "setHighRefresh", "(Z)V") : nullptr;
    if (m) env->CallStaticVoidMethod(cls, m, (jboolean)on);
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (cls) env->DeleteLocalRef(cls);
    env->DeleteLocalRef(act);
#endif
}

static SDL_Color g_tint = { 255, 255, 255, 255 };
void setDrawTint(SDL_Color c) { g_tint = c; }
static bool g_drawAdd = false;
void setDrawAdditive(bool on) { g_drawAdd = on; }

void drawTile(const Image* img, int id, const SDL_FRect& d) {
    if (!img || img->pages.empty()) return;
    if (!img->tileW) {
        int tw = (int)d.w, th = (int)d.h, cols = std::max(1, img->w / std::max(1, tw));
        drawImage(img, SDL_Rect{ (id % cols) * tw, (id / cols) * th, tw, th }, d);
        return;
    }
    SDL_Rect s = { (id % img->tileCols) * (img->tileW + 2) + 1, (id / img->tileCols) * (img->tileH + 2) + 1, img->tileW, img->tileH };
    {
        int tw = 0; SDL_QueryTexture(img->pages[0], nullptr, nullptr, &tw, nullptr);
        const int k = tw / std::max(1, img->tileCols * (img->tileW + 2)); // 2 con texturas HD
        if (k > 1) { s.x *= k; s.y *= k; s.w *= k; s.h *= k; }
    }
    SDL_SetTextureAlphaMod(img->pages[0], g_tint.a);
    SDL_SetTextureColorMod(img->pages[0], g_tint.r, g_tint.g, g_tint.b);
    SDL_SetTextureScaleMode(img->pages[0], g_smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    SDL_RenderCopyF(g_ren, img->pages[0], &s, &d);
}

void freeImage(Image* img) {
    if (!img) return;
    for (auto t : img->pages) if (t) SDL_DestroyTexture(t);
    g_images.erase(img);
    delete img;
}

static int evictPages(uint32_t minIdle, uint32_t targetKB) {
    int n = 0;
    while (freeMemoryKB() > targetKB) {
        Image* best = nullptr; size_t bi = 0; uint32_t bu = 0;
        for (Image* im : g_images) {
            if (im->src.empty() || im->used.size() != im->pages.size()) continue;
            for (size_t i = 0; i < im->pages.size(); i++)
                if (im->pages[i] && g_useClock - im->used[i] >= minIdle && (!best || im->used[i] < bu)) {
                    best = im; bi = i; bu = im->used[i];
                }
        }
        if (!best) break;
        SDL_DestroyTexture(best->pages[bi]);
        best->pages[bi] = nullptr;
        best->tried[bi] = 0;
        n++;
    }
    return n;
}

void drawImage(const Image* img, const SDL_Rect& src, const SDL_FRect& dst, bool flipH, uint8_t alpha, bool flipV) {
    if (!img || img->pages.empty() || src.w <= 0 || src.h <= 0) return;
    const float sx = dst.w / src.w, sy = dst.h / src.h;
    const int P = img->page;
    const bool wholeImage = src.x == 0 && src.y == 0 && src.w == img->w && src.h == img->h;
    int c0 = src.x / P, c1 = (src.x + src.w - 1) / P;
    int r0 = src.y / P, r1 = (src.y + src.h - 1) / P;
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++) {
            if (c < 0 || r < 0 || c >= img->cols || r >= img->rows) continue;
            SDL_Texture* t = img->pages[r * img->cols + c];
            if (!t && !img->src.empty() && !img->tried.empty() && !img->tried[r * img->cols + c]) {
                ensurePages(const_cast<Image*>(img), SDL_Rect{ c * P, r * P, P, P });
                t = img->pages[r * img->cols + c];
            }
            if (!t) continue;
            if (img->used.size() == img->pages.size()) const_cast<Image*>(img)->used[r * img->cols + c] = g_useClock;

            {
                SDL_ScaleMode want = wholeImage || g_smooth ? SDL_ScaleModeLinear : SDL_ScaleModeNearest, cur;
                if (SDL_GetTextureScaleMode(t, &cur) != 0 || cur != want) SDL_SetTextureScaleMode(t, want);
            }

            int x0 = std::max(src.x, c * P), y0 = std::max(src.y, r * P);
            int x1 = std::min(src.x + src.w, (c + 1) * P), y1 = std::min(src.y + src.h, (r + 1) * P);
            if (x1 <= x0 || y1 <= y0) continue;
            SDL_Rect ps = { x0 - c * P, y0 - r * P, x1 - x0, y1 - y0 };
            float ox = flipH ? (src.x + src.w - x1) : (x0 - src.x);
            float oy = flipV ? (src.y + src.h - y1) : (y0 - src.y);
            SDL_FRect pd = { dst.x + ox * sx, dst.y + oy * sy, (x1 - x0) * sx, (y1 - y0) * sy };
            const Uint8 a8 = (Uint8)(alpha * g_tint.a / 255);
            int texW = 0, texH = 0;
            SDL_QueryTexture(t, nullptr, nullptr, &texW, &texH);
            const int k = std::max(1, texW / std::max(1, std::min(P, img->w - c * P))); // 2 con texturas HD
            SDL_SetTextureAlphaMod(t, a8);
            SDL_SetTextureColorMod(t, g_tint.r, g_tint.g, g_tint.b);
            SDL_BlendMode prevBm = SDL_BLENDMODE_BLEND;
            if (g_drawAdd) { SDL_GetTextureBlendMode(t, &prevBm); SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD); }
            const bool linear = wholeImage || g_smooth;
            if (linear && !wholeImage) {
                const float tw = (float)texW / k, th = (float)texH / k;
                float u0 = (float)ps.x, v0 = (float)ps.y, u1 = (float)(ps.x + ps.w), v1 = (float)(ps.y + ps.h);
                if (x0 == src.x) u0 += 0.5f;
                if (x1 == src.x + src.w) u1 -= 0.5f;
                if (y0 == src.y) v0 += 0.5f;
                if (y1 == src.y + src.h) v1 -= 0.5f;
                u0 /= tw; u1 /= tw; v0 /= th; v1 /= th;
                if (flipH) std::swap(u0, u1);
                if (flipV) std::swap(v0, v1);
                const SDL_Color col = { g_tint.r, g_tint.g, g_tint.b, a8 };
                const SDL_Vertex v[6] = {
                    { { pd.x, pd.y }, col, { u0, v0 } }, { { pd.x + pd.w, pd.y }, col, { u1, v0 } }, { { pd.x, pd.y + pd.h }, col, { u0, v1 } },
                    { { pd.x + pd.w, pd.y }, col, { u1, v0 } }, { { pd.x + pd.w, pd.y + pd.h }, col, { u1, v1 } }, { { pd.x, pd.y + pd.h }, col, { u0, v1 } },
                };
                SDL_RenderGeometry(g_ren, t, v, 6, nullptr, 0);
                if (g_drawAdd) SDL_SetTextureBlendMode(t, prevBm);
                continue;
            }
            if (k > 1) { ps.x *= k; ps.y *= k; ps.w *= k; ps.h *= k; }
            SDL_RenderCopyExF(g_ren, t, &ps, &pd, 0, nullptr, (SDL_RendererFlip)((flipH ? SDL_FLIP_HORIZONTAL : 0) | (flipV ? SDL_FLIP_VERTICAL : 0)));
            if (g_drawAdd) SDL_SetTextureBlendMode(t, prevBm);
        }
}

#ifdef XA_TOUCH
enum { TB_DPAD, TB_JUMP, TB_SHOOT, TB_PAUSE, TB_BACK, TB_COUNT };
struct TouchBtn { float x, y, r; };
static std::map<SDL_FingerID, SDL_FPoint> g_fingers;
static bool g_touchUi = true;
static void touchInit() {
    static bool done = false;
    if (done) return;
    done = true;
#ifdef __ANDROID__
    if (SDL_IsAndroidTV()) g_touchUi = false;
#endif
}
static bool g_tbDown[TB_COUNT];
static bool g_dpad[4];

// Ajustes del joystick (menú CONTROL).
static float g_stickScale = 1.0f;
static int g_stickTheme = 0;
static bool g_stickModern = false;
static bool g_btn3d = false;
void setTouchStick(int sizePercent, int theme, int type, bool style3d) {
    g_stickScale = std::max(50, std::min(sizePercent, 200)) / 100.0f;
    g_stickTheme = theme;
    g_stickModern = type == 1;
    g_btn3d = style3d;
}
// Estado visual: mientras un dedo controla el joystick, la perilla sigue al dedo (la base queda fija).
static bool g_stickActive = false;
static SDL_FPoint g_stickBase, g_stickKnob;

// Los menús de la colección se manejan tocando la pantalla: ahí no se muestran los controles táctiles.
static bool g_overlayOn = true;
void setTouchOverlay(bool on) { g_overlayOn = on; }

// Mapeo (menú CONTROL > MAPEO): posición elegida por el usuario para cada botón, normalizada 0-1.
static bool g_custom[TB_COUNT];
static SDL_FPoint g_customPos[TB_COUNT];
static bool g_mapEdit = false;
static int g_mapDrag = -1;
static SDL_FingerID g_mapFinger = 0;
static SDL_FPoint g_mapGrab;

static void touchLayout(TouchBtn* b) {
    const float W = (float)SCREEN_W, H = (float)SCREEN_H;
    const float sr = 60 * g_stickScale;
    b[TB_DPAD]   = { std::max(80.0f, sr + 18), H - std::max(84.0f, sr + 20), sr };
    b[TB_JUMP]   = { W - 60, H - 72, 36 };
    b[TB_SHOOT]  = { W - 142, H - 52, 31 };
    b[TB_PAUSE]  = { W - 26, 76, 18 };
    b[TB_BACK]   = { 26, 96, 18 };
    for (int i = 0; i < TB_COUNT; i++) {
        if (!g_custom[i]) continue;
        const float r = std::min(b[i].r, std::min(W, H) * 0.5f);
        b[i].x = std::max(r, std::min(W - r, g_customPos[i].x * W));
        b[i].y = std::max(r, std::min(H - r, g_customPos[i].y * H));
    }
}

void setTouchLayout(const uint16_t* pos) {
    for (int i = 0; i < TB_COUNT; i++) {
        g_custom[i] = pos[i * 2] != 0xFFFF && pos[i * 2 + 1] != 0xFFFF;
        g_customPos[i] = { pos[i * 2] / 65534.0f, pos[i * 2 + 1] / 65534.0f };
    }
}
void touchLayoutGet(uint16_t* pos) {
    for (int i = 0; i < TB_COUNT; i++) {
        const bool c = g_custom[i];
        pos[i * 2]     = c ? (uint16_t)(std::max(0.0f, std::min(1.0f, g_customPos[i].x)) * 65534 + 0.5f) : 0xFFFF;
        pos[i * 2 + 1] = c ? (uint16_t)(std::max(0.0f, std::min(1.0f, g_customPos[i].y)) * 65534 + 0.5f) : 0xFFFF;
    }
}
void touchLayoutReset() { memset(g_custom, 0, sizeof g_custom); g_mapDrag = -1; }
void touchMapEdit(bool on) { g_mapEdit = on; g_mapDrag = -1; }

static SDL_FPoint touchToLogical(float nx, float ny) {
    if (g_stretch) return { nx * SCREEN_W, ny * SCREEN_H };
    int ow = SCREEN_W, oh = SCREEN_H;
    if (g_ren) SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    float s = std::min((float)ow / SCREEN_W, (float)oh / SCREEN_H);
    float ox = (ow - SCREEN_W * s) * 0.5f, oy = (oh - SCREEN_H * s) * 0.5f;
    return { (nx * ow - ox) / s, (ny * oh - oy) / s };
}

static int touchHit(SDL_FPoint p, float* ddx = nullptr, float* ddy = nullptr) {
    TouchBtn b[TB_COUNT]; touchLayout(b);
    int best = -1; float bd = 0;
    for (int i = TB_JUMP; i < TB_COUNT; i++) {
        float ex = p.x - b[i].x, ey = p.y - b[i].y, dd = std::sqrt(ex * ex + ey * ey);
        float reach = b[i].r * (i >= TB_PAUSE ? 1.7f : 1.35f);
        if (dd < reach && (best < 0 || dd / reach < bd)) { best = i; bd = dd / reach; }
    }
    if (best >= 0) return best;
    float dx = p.x - b[TB_DPAD].x, dy = p.y - b[TB_DPAD].y, d = std::sqrt(dx * dx + dy * dy);
    if (d < b[TB_DPAD].r * 1.8f) {
        if (ddx) *ddx = dx;
        if (ddy) *ddy = dy;
        return TB_DPAD;
    }
    return -1;
}

// Dedos que empezaron en el joystick: lo siguen controlando aunque salgan del círculo.
// El joystick es fijo: siempre se mide desde su centro.
static std::map<SDL_FingerID, SDL_FPoint> g_stickAnchor;

struct MenuFinger { SDL_FPoint start, cur; Uint32 t0; bool menu; };
static std::map<SDL_FingerID, MenuFinger> g_menuFingers;
static bool g_tap = false;
static SDL_FPoint g_tapAt;
static SDL_FPoint toMenu(SDL_FPoint p) {
    const float BW = 512, BH = 384;
    if (SCREEN_W == BW && SCREEN_H == BH) return p;
    if (g_stretch) return { p.x * BW / SCREEN_W, p.y * BH / SCREEN_H };
    if (SCREEN_W >= BW && SCREEN_H >= BH) return { p.x - (SCREEN_W - BW) * 0.5f, p.y - (SCREEN_H - BH) * 0.5f };
    const float k = std::min(SCREEN_W / BW, SCREEN_H / BH);
    return { (p.x - (SCREEN_W - BW * k) * 0.5f) / k, (p.y - (SCREEN_H - BH * k) * 0.5f) / k };
}

static void touchEvent(const SDL_Event& e) {
    touchInit();
    switch (e.type) {
    case SDL_FINGERDOWN: case SDL_FINGERMOTION: {
        const SDL_FPoint p = touchToLogical(e.tfinger.x, e.tfinger.y);
        g_fingers[e.tfinger.fingerId] = p;
        if (e.type == SDL_FINGERDOWN) {
            const int hit = touchHit(p);
            g_menuFingers[e.tfinger.fingerId] = { p, p, SDL_GetTicks(), !(((g_touchUi && g_overlayOn) || g_mapEdit) && hit >= 0) };
            TouchBtn b[TB_COUNT]; touchLayout(b);
            if (g_mapEdit) {
                // Mapeo: el dedo que toca un botón lo arrastra.
                if (hit >= 0 && g_mapDrag < 0) {
                    g_mapDrag = hit; g_mapFinger = e.tfinger.fingerId;
                    g_mapGrab = { b[hit].x - p.x, b[hit].y - p.y };
                }
            } else if (hit == TB_DPAD && g_overlayOn) g_stickAnchor[e.tfinger.fingerId] = { b[TB_DPAD].x, b[TB_DPAD].y };
        } else {
            if (g_menuFingers.count(e.tfinger.fingerId)) g_menuFingers[e.tfinger.fingerId].cur = p;
            if (g_mapEdit && g_mapDrag >= 0 && e.tfinger.fingerId == g_mapFinger && SCREEN_W > 0 && SCREEN_H > 0) {
                g_custom[g_mapDrag] = true;
                g_customPos[g_mapDrag] = { (p.x + g_mapGrab.x) / SCREEN_W, (p.y + g_mapGrab.y) / SCREEN_H };
                TouchBtn b[TB_COUNT]; touchLayout(b); // guarda la posición ya ajustada a la pantalla
                g_customPos[g_mapDrag] = { b[g_mapDrag].x / SCREEN_W, b[g_mapDrag].y / SCREEN_H };
            }
        }
        g_touchUi = true;
        break;
    }
    case SDL_FINGERUP: {
        auto it = g_menuFingers.find(e.tfinger.fingerId);
        if (it != g_menuFingers.end()) {
            const MenuFinger& f = it->second;
            const float mx = f.cur.x - f.start.x, my = f.cur.y - f.start.y;
            if (f.menu && SDL_GetTicks() - f.t0 < 700 && mx * mx + my * my < 14 * 14) { g_tap = true; g_tapAt = toMenu(f.start); }
            g_menuFingers.erase(it);
        }
        g_fingers.erase(e.tfinger.fingerId);
        g_stickAnchor.erase(e.tfinger.fingerId);
        if (g_mapDrag >= 0 && e.tfinger.fingerId == g_mapFinger) g_mapDrag = -1;
        break;
    }
    case SDL_APP_WILLENTERBACKGROUND: case SDL_APP_DIDENTERBACKGROUND: g_fingers.clear(); g_menuFingers.clear(); g_stickAnchor.clear(); g_mapDrag = -1; break;
    case SDL_CONTROLLERBUTTONDOWN: g_touchUi = false; break;
    case SDL_KEYDOWN:
#ifdef HOST
        if (getenv("XA_SHOT")) break;
#endif
        if (e.key.keysym.scancode != SDL_SCANCODE_AC_BACK) g_touchUi = false;
        break;
    }
}

bool menuTap(float* x, float* y) {
    if (!g_tap) return false;
    if (x) *x = g_tapAt.x;
    if (y) *y = g_tapAt.y;
    return true;
}

#ifndef __ANDROID__
static bool g_mouseDown = false;
static int g_wheel = 0, g_wheelStep = 0;
static SDL_FPoint g_mouseStart;
static Uint32 g_mouseSeen = 0;
static bool g_cursorMenus = true;
static bool g_cursorHidden = false;
static Image* g_cursorImg = nullptr;
static SDL_FPoint mouseLogical() {
    int mx = 0, my = 0, ww = 1, wh = 1;
    SDL_GetMouseState(&mx, &my);
    if (g_win) SDL_GetWindowSize(g_win, &ww, &wh);
    return touchToLogical(mx / (float)std::max(1, ww), my / (float)std::max(1, wh));
}
static void mouseEvent(const SDL_Event& e) {
#ifdef HOST
    if (getenv("XA_SHOT") && !getenv("XA_TAPS")) return;
#endif
    switch (e.type) {
    case SDL_MOUSEWHEEL:
        g_wheel += e.wheel.y; g_mouseSeen = SDL_GetTicks() | 1;
        break;
    case SDL_MOUSEMOTION:
        if (e.motion.which != SDL_TOUCH_MOUSEID) g_mouseSeen = SDL_GetTicks() | 1;
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT) break;
        g_mouseDown = true; g_mouseStart = mouseLogical(); g_mouseSeen = SDL_GetTicks() | 1;
        break;
    case SDL_MOUSEBUTTONUP:
        if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT) break;
        if (g_mouseDown) {
            const SDL_FPoint p = mouseLogical();
            const float dx = p.x - g_mouseStart.x, dy = p.y - g_mouseStart.y;
            if (dx * dx + dy * dy < 14 * 14) { g_tap = true; g_tapAt = toMenu(g_mouseStart); }
        }
        g_mouseDown = false;
        break;
    case SDL_KEYDOWN: case SDL_CONTROLLERBUTTONDOWN:
        g_mouseSeen = 0;
        break;
    }
}
void setCursorMenus(bool menus) { g_cursorMenus = menus; }
int menuWheel() { return g_cursorMenus ? g_wheelStep : 0; }
void hideCursorThisFrame() { g_cursorHidden = true; }
bool menuPointer(float* x, float* y) {
    if (!g_cursorMenus || !g_mouseSeen || !(SDL_GetWindowFlags(g_win) & SDL_WINDOW_MOUSE_FOCUS)) return false;
    const SDL_FPoint p = toMenu(mouseLogical());
    if (x) *x = p.x;
    if (y) *y = p.y;
    return true;
}
static bool mouseDrag(float* x, float* y) {
    if (!g_mouseDown || !g_cursorMenus) return false;
    const SDL_FPoint p = toMenu(mouseLogical());
    if (x) *x = p.x;
    if (y) *y = p.y;
    return true;
}
static void drawCursor() {
    const bool hidden = g_cursorHidden;
    g_cursorHidden = false;
    if (hidden || !g_cursorMenus || !g_mouseSeen || !g_ren || !(SDL_GetWindowFlags(g_win) & SDL_WINDOW_MOUSE_FOCUS)) return;
    if (!g_cursorImg) g_cursorImg = loadImage("assets/images/menuElements/cursor.png");
    if (!g_cursorImg) return;
    restoreLogical();
    resetViewport();
    const SDL_FPoint p = mouseLogical();
    drawImage(g_cursorImg, SDL_Rect{ 0, 0, 32, 32 }, SDL_FRect{ p.x - 3, p.y - 2, 32, 32 });
}
#else
void setCursorMenus(bool) {}
int menuWheel() { return 0; }
void hideCursorThisFrame() {}
bool menuPointer(float*, float*) { return false; }
#endif

// Dedo apoyado (o botón del mouse apretado) en coordenadas lógicas de pantalla completa.
bool pointerDown(float* x, float* y) {
#ifndef __ANDROID__
    if (g_mouseDown) { const SDL_FPoint p = mouseLogical(); *x = p.x; *y = p.y; return true; }
#endif
    for (auto& f : g_menuFingers) if (f.second.menu) { *x = f.second.cur.x; *y = f.second.cur.y; return true; }
    return false;
}
// Posición del mouse (para resaltar botones en PC), en coordenadas lógicas de pantalla completa.
bool pointerHover(float* x, float* y) {
#ifndef __ANDROID__
    if (g_mouseSeen && (SDL_GetWindowFlags(g_win) & SDL_WINDOW_MOUSE_FOCUS)) { const SDL_FPoint p = mouseLogical(); *x = p.x; *y = p.y; return true; }
#endif
    (void)x; (void)y;
    return false;
}

bool menuTouch(float* x, float* y) {
#ifndef __ANDROID__
    if (mouseDrag(x, y)) return true;
#endif
    for (auto& f : g_menuFingers)
        if (f.second.menu) {
            const SDL_FPoint p = toMenu(f.second.cur);
            if (x) *x = p.x;
            if (y) *y = p.y;
            return true;
        }
    return false;
}

static void touchUpdate() {
    memset(g_tbDown, 0, sizeof g_tbDown);
    memset(g_dpad, 0, sizeof g_dpad);
    TouchBtn b[TB_COUNT]; touchLayout(b);
    g_stickActive = false;
    if (g_mapEdit || !g_overlayOn) return; // en el mapeo y en los menús los toques no son botones
    for (auto& f : g_fingers) {
        float dx = 0, dy = 0;
        int hit;
        auto anc = g_stickAnchor.find(f.first);
        if (anc != g_stickAnchor.end()) {
            // El dedo que empezó en el joystick lo controla aunque salga del círculo; el joystick no se mueve.
            hit = TB_DPAD;
            anc->second = { b[TB_DPAD].x, b[TB_DPAD].y };
            dx = f.second.x - anc->second.x; dy = f.second.y - anc->second.y;
            if (!g_stickActive) { g_stickActive = true; g_stickBase = anc->second; g_stickKnob = f.second; }
        } else hit = touchHit(f.second, &dx, &dy);
        if (hit == TB_DPAD) {
            g_tbDown[TB_DPAD] = true;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > b[TB_DPAD].r * 0.22f) {
                const float k = 0.38f * d;
                g_dpad[0] |= dx < -k; g_dpad[1] |= dx > k;
                g_dpad[2] |= dy < -k; g_dpad[3] |= dy > k;
            }
        } else if (hit >= 0) g_tbDown[hit] = true;
    }
}

static void geoCircle(float cx, float cy, float r0, float r1, SDL_Color c) {
    const int N = 40;
    SDL_Vertex v[N * 6]; int n = 0;
    for (int i = 0; i < N; i++) {
        float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N;
        SDL_FPoint o0 = { cx + std::cos(a0) * r1, cy + std::sin(a0) * r1 }, o1 = { cx + std::cos(a1) * r1, cy + std::sin(a1) * r1 };
        SDL_FPoint i0 = { cx + std::cos(a0) * r0, cy + std::sin(a0) * r0 }, i1 = { cx + std::cos(a1) * r0, cy + std::sin(a1) * r0 };
        v[n++] = { o0, c, {0, 0} }; v[n++] = { o1, c, {0, 0} }; v[n++] = { i0, c, {0, 0} };
        if (r0 > 0) { v[n++] = { i0, c, {0, 0} }; v[n++] = { o1, c, {0, 0} }; v[n++] = { i1, c, {0, 0} }; }
    }
    SDL_RenderGeometry(g_ren, nullptr, v, n, nullptr, 0);
}

struct TouchSprite { const char* file; SDL_Rect r; SDL_Texture* tex; bool tried; };
enum { TS_JUMP, TS_SHOOT, TS_COUNT };
static TouchSprite g_tsprites[TS_COUNT] = {
    { "data/ui/jump.png",  { 0, 0, 0, 0 }, nullptr, false },
    { "data/ui/shoot.png", { 0, 0, 0, 0 }, nullptr, false },
};
static SDL_Texture* touchSprite(int i) {
    TouchSprite& s = g_tsprites[i];
    if (s.tried) return s.tex;
    s.tried = true;
    auto data = assetRead(s.file);
    SDL_Surface* raw = data.empty() ? nullptr : IMG_Load_RW(SDL_RWFromConstMem(data.data(), (int)data.size()), 1);
    if (!raw) { error("touch sprite %s missing", s.file); return nullptr; }
    if (s.r.w <= 0) s.r = { 0, 0, raw->w, raw->h };
    SDL_Surface* cut = SDL_CreateRGBSurfaceWithFormat(0, s.r.w, s.r.h, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_SetSurfaceBlendMode(raw, SDL_BLENDMODE_NONE);
    SDL_BlitSurface(raw, &s.r, cut, nullptr);
    SDL_FreeSurface(raw);
    s.tex = SDL_CreateTextureFromSurface(g_ren, cut);
    SDL_FreeSurface(cut);
    if (s.tex) { SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_BLEND); SDL_SetTextureScaleMode(s.tex, SDL_ScaleModeLinear); }
    return s.tex;
}
static void spriteAt(int i, float cx, float cy, float h, double angle, Uint8 alpha, bool flip = false, SDL_Color tint = { 255, 255, 255, 255 }) {
    SDL_Texture* t = touchSprite(i);
    if (!t) return;
    const SDL_Rect& r = g_tsprites[i].r;
    float w = h * r.w / r.h;
    SDL_FRect d = { cx - w * 0.5f, cy - h * 0.5f, w, h };
    SDL_SetTextureColorMod(t, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(t, alpha);
    SDL_RenderCopyExF(g_ren, t, nullptr, &d, angle, nullptr, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

static void drawArrow(float cx, float cy, float dx, float dy, float len, SDL_Color fill) {
    auto shape = [&](float tail, float base, float tip, float shaft, float head, SDL_Color c) {
        auto P = [&](float u, float v) { return SDL_FPoint{ cx + dx * u - dy * v, cy + dy * u + dx * v }; };
        SDL_FPoint q0 = P(tail, -shaft), q1 = P(base, -shaft), q2 = P(base, shaft), q3 = P(tail, shaft);
        SDL_FPoint h0 = P(base, -head), h1 = P(tip, 0), h2 = P(base, head);
        SDL_Vertex v[9] = { { q0, c, {0, 0} }, { q1, c, {0, 0} }, { q2, c, {0, 0} },
                            { q0, c, {0, 0} }, { q2, c, {0, 0} }, { q3, c, {0, 0} },
                            { h0, c, {0, 0} }, { h1, c, {0, 0} }, { h2, c, {0, 0} } };
        SDL_RenderGeometry(g_ren, nullptr, v, 9, nullptr, 0);
    };
    const float o = std::max(1.5f, len * 0.07f);
    const float tail = -len * 0.5f, base = 0, tip = len * 0.5f, shaft = len * 0.13f, head = len * 0.36f;
    shape(tail - o, base - o, tip + o * 1.9f, shaft + o, head + o * 1.9f, SDL_Color{ 20, 28, 8, (Uint8)(fill.a * 0.8f) });
    shape(tail, base, tip, shaft, head, fill);
}

// Círculo/elipse con degradado radial: color c0 en (fx, fy) y c1 en el borde.
static void geoRadial(float cx, float cy, float rx, float ry, float fx, float fy, SDL_Color c0, SDL_Color c1) {
    const int N = 40;
    SDL_Vertex v[N * 3]; int n = 0;
    for (int i = 0; i < N; i++) {
        const float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N;
        v[n++] = { { fx, fy }, c0, { 0, 0 } };
        v[n++] = { { cx + std::cos(a0) * rx, cy + std::sin(a0) * ry }, c1, { 0, 0 } };
        v[n++] = { { cx + std::cos(a1) * rx, cy + std::sin(a1) * ry }, c1, { 0, 0 } };
    }
    SDL_RenderGeometry(g_ren, nullptr, v, n, nullptr, 0);
}
static SDL_Color mixColor(SDL_Color a, SDL_Color b, float t) {
    return { (Uint8)(a.r + (b.r - a.r) * t), (Uint8)(a.g + (b.g - a.g) * t), (Uint8)(a.b + (b.b - a.b) * t), (Uint8)(a.a + (b.a - a.a) * t) };
}

static float g_touchOpacity = 1.0f;
void setTouchOpacity(int percent) { g_touchOpacity = std::max(0, std::min(percent, 100)) / 100.0f; }

static void drawTouchOverlay() {
    touchInit();
    if (!touchEnabled() || !g_ren) return;
    if (!g_mapEdit && (!g_touchUi || g_touchOpacity <= 0 || !g_overlayOn)) return;
    restoreLogical();
    resetViewport();
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
    TouchBtn b[TB_COUNT]; touchLayout(b);
    // En el mapeo los botones se ven siempre, aunque la opacidad esté baja.
    const float op = g_mapEdit ? std::max(g_touchOpacity, 0.85f) : g_touchOpacity;
    auto A = [op](SDL_Color c) { c.a = (Uint8)(c.a * op); return c; };
    auto AS = [op](int a) { return (Uint8)(a * op); };
    // Temas de color de los iconos: LIMA (original), CIAN, FUEGO, BLANCO.
    struct Theme { SDL_Color ring, hot, press; };
    static const Theme THEMES[] = {
        { { 160, 214, 40, 255 }, { 214, 255, 90, 255 }, { 40, 70, 10, 255 } },
        { { 40, 190, 220, 255 }, { 130, 235, 255, 255 }, { 10, 50, 70, 255 } },
        { { 235, 130, 30, 255 }, { 255, 200, 90, 255 }, { 80, 35, 5, 255 } },
        { { 205, 205, 205, 255 }, { 255, 255, 255, 255 }, { 60, 60, 60, 255 } },
    };
    const Theme& th = THEMES[(unsigned)g_stickTheme < sizeof THEMES / sizeof THEMES[0] ? g_stickTheme : 0];
    auto Ca = [&](SDL_Color c, int a) { c.a = (Uint8)a; return A(c); };
    const SDL_Color lime = Ca(th.ring, 150), limeHot = Ca(th.hot, 235);
    const Uint8 IDLE = AS(165), HOT = AS(255);
    const SDL_Color black = { 0, 0, 0, 255 }, white = { 255, 255, 255, 255 };
    // Dibuja la base de un botón y devuelve la Y de su cara (en 3D baja al presionarlo).
    auto base = [&](const TouchBtn& t, bool down) -> float {
        if (!g_btn3d) {
            geoCircle(t.x, t.y, 0, t.r, down ? Ca(th.press, 140) : A({ 0, 0, 0, 85 }));
            geoCircle(t.x, t.y, t.r - 2.5f, t.r, down ? limeHot : lime);
            return t.y;
        }
        // 3D: sombra, lateral extruido, cara con degradado, borde y brillo.
        const float depth = std::max(2.5f, t.r * 0.16f), push = down ? depth * 0.7f : 0.0f;
        const float fy = t.y - depth * 0.5f + push;
        geoCircle(t.x + 1.0f, t.y + depth * 0.5f + 2.0f, 0, t.r * 1.02f, A({ 0, 0, 0, 70 }));
        geoCircle(t.x, t.y + depth * 0.5f, 0, t.r, Ca(mixColor(th.ring, black, 0.62f), 215));
        const SDL_Color in = down ? mixColor(th.press, th.ring, 0.35f) : SDL_Color{ 64, 72, 54, 255 };
        const SDL_Color out = down ? mixColor(th.press, black, 0.4f) : SDL_Color{ 12, 14, 10, 255 };
        geoRadial(t.x, fy, t.r, t.r, t.x - t.r * 0.25f, fy - t.r * 0.3f, Ca(in, 175), Ca(out, 175));
        geoCircle(t.x, fy, t.r - 2.5f, t.r, down ? limeHot : lime);
        geoRadial(t.x, fy - t.r * 0.42f, t.r * 0.62f, t.r * 0.36f, t.x, fy - t.r * 0.5f, Ca(white, 60), Ca(white, 0));
        return fy;
    };
    // Flecha con sombra extra en 3D.
    auto arrow = [&](float x, float y, float dx, float dy, float len, SDL_Color c) {
        if (g_btn3d) drawArrow(x + 0.8f, y + std::max(1.5f, len * 0.12f), dx, dy, len, A({ 10, 14, 6, 150 }));
        drawArrow(x, y, dx, dy, len, c);
    };

    // Mapeo: aro resaltado alrededor de cada botón (más fuerte en el que se arrastra).
    if (g_mapEdit)
        for (int i = 0; i < TB_COUNT; i++)
            geoCircle(b[i].x, b[i].y, b[i].r + 3.0f, b[i].r + (g_mapDrag == i ? 7.0f : 5.0f), Ca(g_mapDrag == i ? th.hot : th.ring, g_mapDrag == i ? 230 : 120));
    auto pressedVis = [](int i) { return g_tbDown[i] || g_mapDrag == i; };

    TouchBtn d = b[TB_DPAD];
    if (g_stickActive) { d.x = g_stickBase.x; d.y = g_stickBase.y; }
    if (g_stickModern) {
        // Moderno: aro fino y una perilla que sigue al dedo.
        if (g_btn3d) geoRadial(d.x, d.y, d.r, d.r, d.x, d.y - d.r * 0.2f, A({ 0, 0, 0, 30 }), A({ 0, 0, 0, 115 })); // hundido
        else geoCircle(d.x, d.y, 0, d.r, A({ 0, 0, 0, 60 }));
        geoCircle(d.x, d.y, d.r - 1.5f, d.r, Ca(th.ring, g_stickActive ? 170 : 110));
        const float kr = d.r * 0.42f, maxOff = d.r - kr * 0.5f;
        float kx = d.x, ky = d.y;
        if (g_stickActive) {
            float ox = g_stickKnob.x - d.x, oy = g_stickKnob.y - d.y, ol = std::sqrt(ox * ox + oy * oy);
            if (ol > maxOff) { ox *= maxOff / ol; oy *= maxOff / ol; }
            kx += ox; ky += oy;
            geoCircle(kx, ky, 0, kr * 1.35f, Ca(th.hot, 45));
        }
        const SDL_Color face = g_stickActive ? th.hot : th.ring;
        if (g_btn3d) {
            const float dep = std::max(2.0f, kr * 0.22f), fy = ky - dep * 0.5f;
            geoCircle(kx + 1.0f, ky + dep * 0.5f + 2.0f, 0, kr, A({ 0, 0, 0, 90 }));
            geoCircle(kx, ky + dep * 0.5f, 0, kr, Ca(mixColor(face, black, 0.55f), 235));
            geoRadial(kx, fy, kr, kr, kx - kr * 0.3f, fy - kr * 0.35f, Ca(mixColor(face, white, 0.35f), 240), Ca(mixColor(face, black, 0.25f), 240));
            geoRadial(kx, fy - kr * 0.42f, kr * 0.6f, kr * 0.34f, kx, fy - kr * 0.5f, Ca(white, 90), Ca(white, 0));
        } else {
            geoCircle(kx, ky, 0, kr, g_stickActive ? Ca(th.hot, 230) : Ca(th.ring, 190));
            geoCircle(kx, ky, kr - 2.0f, kr, A({ 20, 28, 8, 110 }));
        }
    } else {
        const float fy = base(d, false);
        const float a = d.r * 0.58f, al = d.r * 0.5f;
        const SDL_Color arrowIdle = A({ 240, 246, 225, 215 }), arrowHot = limeHot;
        arrow(d.x, fy + a, 0, 1, al, g_dpad[3] ? arrowHot : arrowIdle);
        arrow(d.x - a, fy, -1, 0, al, g_dpad[0] ? arrowHot : arrowIdle);
        arrow(d.x, fy - a, 0, -1, al, g_dpad[2] ? arrowHot : arrowIdle);
        arrow(d.x + a, fy, 1, 0, al, g_dpad[1] ? arrowHot : arrowIdle);
    }

    const SDL_Color iconIdle = th.ring, iconHot = th.hot;
    auto icon = [&](int sprite, const TouchBtn& t, bool down) {
        const float fy = base(t, down);
        if (g_btn3d) spriteAt(sprite, t.x + 0.8f, fy + 1.6f, t.r * 1.25f, 0, AS(120), false, { 10, 14, 6, 255 });
        spriteAt(sprite, t.x, fy, t.r * 1.25f, 0, down ? HOT : IDLE, false, down ? iconHot : iconIdle);
    };
    icon(TS_JUMP, b[TB_JUMP], pressedVis(TB_JUMP));
    icon(TS_SHOOT, b[TB_SHOOT], pressedVis(TB_SHOOT));

    const TouchBtn& p = b[TB_PAUSE];
    {
        const float fy = base(p, pressedVis(TB_PAUSE));
        const SDL_Color c = pressedVis(TB_PAUSE) ? limeHot : A({ 235, 245, 210, 200 });
        const float bw = p.r * 0.26f, bh = p.r * 0.85f;
        SDL_FRect l = { p.x - bw * 1.6f, fy - bh * 0.5f, bw, bh }, r = { p.x + bw * 0.6f, fy - bh * 0.5f, bw, bh };
        if (g_btn3d) {
            const SDL_Color sc = A({ 10, 14, 6, 150 });
            SDL_SetRenderDrawColor(g_ren, sc.r, sc.g, sc.b, sc.a);
            SDL_FRect ls = { l.x + 0.8f, l.y + 1.5f, l.w, l.h }, rs = { r.x + 0.8f, r.y + 1.5f, r.w, r.h };
            SDL_RenderFillRectF(g_ren, &ls); SDL_RenderFillRectF(g_ren, &rs);
        }
        SDL_SetRenderDrawColor(g_ren, c.r, c.g, c.b, c.a);
        SDL_RenderFillRectF(g_ren, &l); SDL_RenderFillRectF(g_ren, &r);
    }
    const TouchBtn& bk = b[TB_BACK];
    const float bky = base(bk, pressedVis(TB_BACK));
    arrow(bk.x, bky, -1, 0, bk.r * 1.05f, pressedVis(TB_BACK) ? limeHot : A({ 240, 246, 225, 215 }));
}
#endif

#ifndef XA_TOUCH
void setTouchOpacity(int) {}
void setTouchStick(int, int, int, bool) {}
void setTouchLayout(const uint16_t*) {}
void setTouchOverlay(bool) {}
void touchLayoutGet(uint16_t* pos) { for (int i = 0; i < 10; i++) pos[i] = 0xFFFF; }
void touchLayoutReset() {}
void touchMapEdit(bool) {}
bool menuTap(float*, float*) { return false; }
bool menuTouch(float*, float*) { return false; }
bool pointerDown(float*, float*) { return false; }
bool pointerHover(float*, float*) { return false; }
void setCursorMenus(bool) {}
int menuWheel() { return 0; }
void hideCursorThisFrame() {}
bool menuPointer(float*, float*) { return false; }
#endif

static bool g_tvEffect = false;
void setTvEffect(bool on) { g_tvEffect = on; }
static SDL_Texture* g_scanTex = nullptr;
static int g_scanH = 0;
static void drawTvEffect() {
    if (!g_tvEffect || !g_ren) return;
    int ow, oh; SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    const float ky = g_stretch ? (float)oh / SCREEN_H : std::min((float)ow / SCREEN_W, (float)oh / SCREEN_H);
    const float top = g_stretch ? 0.0f : (oh - SCREEN_H * ky) * 0.5f;
    const int key = oh * 4096 + (int)std::lround(ky * 64) + (int)top;
    static int scanKey = -1;
    if (!g_scanTex || scanKey != key) {
        if (g_scanTex) SDL_DestroyTexture(g_scanTex);
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, 4, oh, 32, SDL_PIXELFORMAT_RGBA32);
        for (int y = 0; y < oh; y++) {
            Uint8 a;
            if (ky >= 2.5f) {
                const float f = std::fmod((y + 0.5f - top) / ky + 1000.0f, 1.0f);
                const float edge = 1.0f - std::sin(f * 3.14159265f);
                a = (Uint8)(150.0f * std::pow(edge, 1.7f));
            } else a = (y & 1) ? 70 : 0;
            uint32_t* row = (uint32_t*)((uint8_t*)s->pixels + y * s->pitch);
            for (int x = 0; x < 4; x++) row[x] = SDL_MapRGBA(s->format, 0, 0, 0, a);
        }
        g_scanTex = SDL_CreateTextureFromSurface(g_ren, s);
        SDL_FreeSurface(s);
        if (!g_scanTex) { g_tvEffect = false; return; }
        SDL_SetTextureBlendMode(g_scanTex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(g_scanTex, SDL_ScaleModeNearest);
        g_scanH = oh; scanKey = key;
    }
    static SDL_Texture* vig = nullptr;
    if (!vig) {
        const int N = 128;
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, N, N, 32, SDL_PIXELFORMAT_RGBA32);
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++) {
                const float dx = (x + 0.5f) / N * 2 - 1, dy = (y + 0.5f) / N * 2 - 1;
                const float r = std::sqrt(dx * dx * 0.85f + dy * dy);
                const float t = std::max(0.0f, std::min(1.0f, (r - 0.6f) / 0.75f));
                ((uint32_t*)((uint8_t*)s->pixels + y * s->pitch))[x] = SDL_MapRGBA(s->format, 0, 0, 0, (Uint8)(130 * t * t * (3 - 2 * t)));
            }
        vig = SDL_CreateTextureFromSurface(g_ren, s);
        SDL_FreeSurface(s);
        if (vig) { SDL_SetTextureBlendMode(vig, SDL_BLENDMODE_BLEND); SDL_SetTextureScaleMode(vig, SDL_ScaleModeLinear); }
    }
    restoreLogical();
    SDL_RenderSetLogicalSize(g_ren, 0, 0);
    SDL_RenderSetViewport(g_ren, nullptr);
    SDL_RenderSetScale(g_ren, 1.0f, 1.0f);
    SDL_RenderCopy(g_ren, g_scanTex, nullptr, nullptr);
    if (vig) {
        const SDL_Rect pic = { (int)std::lround((ow - SCREEN_W * (g_stretch ? (float)ow / SCREEN_W : ky)) * 0.5f), (int)std::lround(top),
                               (int)std::lround(SCREEN_W * (g_stretch ? (float)ow / SCREEN_W : ky)), (int)std::lround(SCREEN_H * ky) };
        SDL_RenderCopy(g_ren, vig, nullptr, &pic);
    }
    applyLogicalSize();
}

static uint32_t g_drawStart = 0;
static bool g_refit = false;
void beginFrame() {
    if (g_refit && g_ren) { g_refit = false; g_stretched = false; applyLogicalSize(); fitShapeToWindow(); }
    restoreLogical();
    g_drawStart = SDL_GetTicks();
    resetViewport();
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
}
static int g_frame = 0;
static unsigned g_frameCount = 0;
static bool g_lowMemTried = false;
void endFrame() {
    g_frameCount++;
    g_lowMemTried = false;
    g_useClock++;

    drawTvEffect();
#ifdef XA_TOUCH
    drawTouchOverlay();
#endif
#if defined(XA_TOUCH) && !defined(__ANDROID__)
    drawCursor();
#endif

#ifdef HOST

    static int shotAt = getenv("XA_SHOT") ? atoi(getenv("XA_SHOT")) : -1;
    if (g_frame == shotAt) {
        SDL_RenderSetLogicalSize(g_ren, 0, 0); SDL_RenderSetScale(g_ren, 1, 1); SDL_RenderSetViewport(g_ren, nullptr);
        int w, h; SDL_GetRendererOutputSize(g_ren, &w, &h);
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        SDL_RenderReadPixels(g_ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
        IMG_SavePNG(s, getenv("XA_SHOT_FILE") ? getenv("XA_SHOT_FILE") : "shot.png");
        SDL_FreeSurface(s);
        g_quit = true;
    }
#endif

    static uint32_t lastDiag = 0;
    if (ticksMs() - lastDiag >= 2000) {
        lastDiag = ticksMs();
        void audioDiagnostics();
        audioDiagnostics();
    }
    g_frame++;

#ifdef XA_PERF
    uint32_t p0 = SDL_GetTicks();
    SDL_RenderPresent(g_ren);
    {
        static uint32_t secStart = 0, last = 0, worst = 0, drawMs = 0, presMs = 0; static int n = 0;
        uint32_t now = SDL_GetTicks();
        if (last) worst = std::max(worst, now - last);
        last = now; n++;
        drawMs += p0 - g_drawStart; presMs += now - p0;
        if (!secStart) secStart = now;
        if (now - secStart >= 1000) {
            log("perf: %d fps, worst %u ms, draw %u ms/s, present %u ms/s, heap %u KB", n, worst, drawMs, presMs, freeMemoryKB());
            secStart = now; worst = 0; drawMs = presMs = 0; n = 0;
        }
    }
#else
    uint32_t pres0 = SDL_GetTicks();
    SDL_RenderPresent(g_ren);
    {
        static uint32_t prev = 0; uint32_t now = SDL_GetTicks();
        if (prev && now - prev > 40)
            log("slow frame: %u ms (logic %u, draw %u, present %u; %d sounds)", now - prev, g_drawStart - g_prof[0], pres0 - g_drawStart, now - pres0, g_sfxCallsFrame);
        prev = now; g_sfxCallsFrame = 0;
    }
#endif
}

static bool g_cur[BTN_COUNT], g_prev[BTN_COUNT];

static std::vector<SDL_GameController*> g_pads;
static bool g_padsDirty = true;

static void openPads() {
    for (auto* p : g_pads) SDL_GameControllerClose(p);
    g_pads.clear();
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i))
            if (SDL_GameController* p = SDL_GameControllerOpen(i)) g_pads.push_back(p);
    g_padsDirty = false;
}
static bool isFullPad(SDL_GameController* p) {
    return SDL_GameControllerHasAxis(p, SDL_CONTROLLER_AXIS_LEFTX) && SDL_GameControllerHasAxis(p, SDL_CONTROLLER_AXIS_LEFTY);
}

static bool g_remoteMode = false;
static bool computeRemoteMode() {
#if defined(__ANDROID__)
    if (!SDL_IsAndroidTV()) return false;
#elif defined(HOST)
    if (!getenv("XA_REMOTE")) return false;
#else
    return false;
#endif
    for (auto* p : g_pads) if (isFullPad(p)) return false;
    return true;
}
bool remoteControls() { return g_remoteMode; }

static const Binding DEFAULT_BINDS[BIND_COUNT] = {
    { SDL_SCANCODE_LEFT,  SDL_CONTROLLER_BUTTON_DPAD_LEFT,  SDL_SCANCODE_A },
    { SDL_SCANCODE_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_SCANCODE_D },
    { SDL_SCANCODE_UP,    SDL_CONTROLLER_BUTTON_DPAD_UP,    SDL_SCANCODE_W },
    { SDL_SCANCODE_DOWN,  SDL_CONTROLLER_BUTTON_DPAD_DOWN,  SDL_SCANCODE_S },
    { SDL_SCANCODE_Z,     SDL_CONTROLLER_BUTTON_A,          -1 },
    { SDL_SCANCODE_X,     SDL_CONTROLLER_BUTTON_X,          -1 },
    { SDL_SCANCODE_RETURN, SDL_CONTROLLER_BUTTON_START,     -1 },
};
static Binding g_bind[BIND_COUNT] = {
    DEFAULT_BINDS[0], DEFAULT_BINDS[1], DEFAULT_BINDS[2], DEFAULT_BINDS[3], DEFAULT_BINDS[4], DEFAULT_BINDS[5], DEFAULT_BINDS[6],
};
static const char* BIND_IDS[BIND_COUNT] = { "left", "right", "up", "down", "jump", "shoot", "pause" };

static std::string controlsPath();
const Binding& binding(int a) { return g_bind[std::max(0, std::min(a, BIND_COUNT - 1))]; }
void setBinding(int a, Binding b) {
    if (a < 0 || a >= BIND_COUNT) return;
    for (int i = 0; i < BIND_COUNT; i++) {
        if (i == a) continue;
        if (b.key >= 0 && g_bind[i].key == b.key) g_bind[i].key = g_bind[a].key;
        if (b.key >= 0 && g_bind[i].key2 == b.key) g_bind[i].key2 = -1;
        if (b.pad >= 0 && g_bind[i].pad == b.pad) g_bind[i].pad = g_bind[a].pad;
    }
    if (b.key >= 0) { g_bind[a].key = b.key; g_bind[a].key2 = -1; }
    if (b.pad >= 0) g_bind[a].pad = b.pad;
}
void resetBindings() { for (int i = 0; i < BIND_COUNT; i++) g_bind[i] = DEFAULT_BINDS[i]; }
static bool isDefault(int a) { return g_bind[a].key == DEFAULT_BINDS[a].key && g_bind[a].key2 == DEFAULT_BINDS[a].key2 && g_bind[a].pad == DEFAULT_BINDS[a].pad; }

bool saveBindings() {
    std::string out = "v2  # action keyboard-scancode gamepad-button second-key\n";
    for (int i = 0; i < BIND_COUNT; i++)
        out += std::string(BIND_IDS[i]) + " " + std::to_string(g_bind[i].key) + " " + std::to_string(g_bind[i].pad) + " " + std::to_string(g_bind[i].key2) + "\n";
    SDL_RWops* f = SDL_RWFromFile(controlsPath().c_str(), "wb");
    if (!f) return false;
    SDL_RWwrite(f, out.data(), 1, out.size());
    SDL_RWclose(f);
    return true;
}
void loadBindings() {
    std::vector<uint8_t> d;
    if (SDL_RWops* f = SDL_RWFromFile(controlsPath().c_str(), "rb")) {
        Sint64 n = SDL_RWsize(f);
        if (n > 0) { d.resize((size_t)n); SDL_RWread(f, d.data(), 1, (size_t)n); }
        SDL_RWclose(f);
    }
    std::istringstream in(std::string(d.begin(), d.end()));
    std::string line;
    if (!std::getline(in, line) || line.compare(0, 2, "v2") != 0) return;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string id; int key = -1, pad = -1, key2 = -1;
        if (!(ls >> id >> key >> pad)) continue;
        ls >> key2;
        for (int i = 0; i < BIND_COUNT; i++)
            if (id == BIND_IDS[i]) {
                if (key > 0 && key < SDL_NUM_SCANCODES) g_bind[i].key = key;
                if (pad >= 0 && pad < SDL_CONTROLLER_BUTTON_MAX) g_bind[i].pad = pad;
                g_bind[i].key2 = key2 > 0 && key2 < SDL_NUM_SCANCODES ? key2 : -1;
            }
    }
}

std::string keyName(int sc) {
    switch (sc) {
        case SDL_SCANCODE_LEFT: return "IZQ";
        case SDL_SCANCODE_RIGHT: return "DER";
        case SDL_SCANCODE_UP: return "ARRIBA";
        case SDL_SCANCODE_DOWN: return "ABAJO";
        case SDL_SCANCODE_SPACE: return "ESPACIO";
        case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return "INTRO";
        case SDL_SCANCODE_LCTRL: case SDL_SCANCODE_RCTRL: return "CTRL";
        case SDL_SCANCODE_LSHIFT: case SDL_SCANCODE_RSHIFT: return "SHIFT";
        case SDL_SCANCODE_LALT: case SDL_SCANCODE_RALT: return "ALT";
        case SDL_SCANCODE_TAB: return "TAB";
        case SDL_SCANCODE_BACKSPACE: return "BORRAR";
        default: break;
    }
    if (sc <= 0) return "-";
    std::string out;
    for (const char* p = SDL_GetScancodeName((SDL_Scancode)sc); p && *p; p++) {
        char c = (char)toupper((unsigned char)*p);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') out += c;
    }
    return out.empty() ? "?" : out;
}
std::string padName(int b) {
    switch (b) {
        case SDL_CONTROLLER_BUTTON_A: return "A";
        case SDL_CONTROLLER_BUTTON_B: return "B";
        case SDL_CONTROLLER_BUTTON_X: return "X";
        case SDL_CONTROLLER_BUTTON_Y: return "Y";
        case SDL_CONTROLLER_BUTTON_BACK: return "SELECT";
        case SDL_CONTROLLER_BUTTON_START: return "START";
        case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "L3";
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "R3";
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "LB";
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "RB";
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return "CRUZ ARR";
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "CRUZ ABA";
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "CRUZ IZQ";
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "CRUZ DER";
        default: break;
    }
    return b < 0 ? "-" : "BOTON " + std::to_string(b);
}

static int g_capture = 0;
static Binding g_captured = { -1, -1 };
static bool g_waitRelease = false;
void beginCapture() { g_capture = 1; g_captured = { -1, -1 }; }
int captureResult(Binding* b) {
    if (g_capture < 2) return g_capture == 1 ? 0 : -1;
    int r = g_capture == 2 ? 1 : -1;
    if (b) *b = g_captured;
    g_capture = 0;
    return r;
}
static void captureEvent(const SDL_Event& e) {
    if (g_capture != 1) return;
    if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) g_capture = 3;
        else { g_captured.key = e.key.keysym.scancode; g_capture = 2; }
        g_waitRelease = true;
    } else if (e.type == SDL_CONTROLLERBUTTONDOWN) {
        g_captured.pad = e.cbutton.button; g_capture = 2; g_waitRelease = true;
    }
}

void inputPoll() {
    memcpy(g_prev, g_cur, sizeof g_cur);
#ifdef XA_TOUCH
    g_tap = false;
#ifndef __ANDROID__
    g_wheelStep = g_wheel; g_wheel = 0;
#endif
#endif
#ifdef HOST
    {
        static int step = 0; step++;
        if (const char* s = getenv("XA_TAPS"))
            for (const char* p = s; *p; ) {
                int st; float x, y;
                if (sscanf(p, "%d:%f:%f", &st, &x, &y) == 3 && st == step) { g_tap = true; g_tapAt = { x, y }; }
                const char* c = strchr(p, ',');
                if (!c) break;
                p = c + 1;
            }
    }
#endif
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) g_quit = true;
        if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED) g_padsDirty = true;
        if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED)) g_refit = true;
#ifdef XA_TOUCH
        touchEvent(e);
#endif
        captureEvent(e);
#if defined(XA_TOUCH) && !defined(__ANDROID__)
        mouseEvent(e);
#endif
    }
    if (g_padsDirty) {
        openPads();
        bool was = g_remoteMode;
        g_remoteMode = computeRemoteMode();
        log("input: %d controller(s), remote controls %s", (int)g_pads.size(), g_remoteMode ? "on" : "off");
        for (auto* p : g_pads) log("input:   %s (%s)", SDL_GameControllerName(p) ? SDL_GameControllerName(p) : "?", isFullPad(p) ? "gamepad" : "remote-like");
        (void)was;
    }
    memset(g_cur, 0, sizeof g_cur);
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    auto K1 = [&](int sc) { return sc > 0 && sc < SDL_NUM_SCANCODES && k[sc] != 0; };
    auto K = [&](int a) { return K1(g_bind[a].key) || K1(g_bind[a].key2); };
    g_cur[BTN_LEFT]   = K(BIND_LEFT);
    g_cur[BTN_RIGHT]  = K(BIND_RIGHT);
    g_cur[BTN_UP]     = K(BIND_UP);
    g_cur[BTN_DOWN]   = K(BIND_DOWN);
    g_cur[BTN_JUMP]   = K(BIND_JUMP);
    g_cur[BTN_SHOOT]  = K(BIND_SHOOT);
    g_cur[BTN_DEFEND] = k[SDL_SCANCODE_C] || k[SDL_SCANCODE_LSHIFT];
    g_cur[BTN_ACCEPT] = k[SDL_SCANCODE_RETURN] || k[SDL_SCANCODE_SPACE] || K(BIND_JUMP);
    g_cur[BTN_BACK]   = k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_BACKSPACE] || K(BIND_SHOOT);
    g_cur[BTN_START]  = k[SDL_SCANCODE_ESCAPE] || K(BIND_PAUSE);
    g_cur[BTN_CHEAT]  = k[SDL_SCANCODE_F1];
#ifdef XA_TOUCH
    g_cur[BTN_BACK]  |= k[SDL_SCANCODE_AC_BACK];
    g_cur[BTN_START] |= k[SDL_SCANCODE_AC_BACK];
    g_cur[BTN_ACCEPT] |= k[SDL_SCANCODE_SELECT];
    g_cur[g_remoteMode ? BTN_SHOOT : BTN_JUMP] |= k[SDL_SCANCODE_SELECT];
    touchUpdate();
    g_cur[BTN_LEFT]   |= g_dpad[0];
    g_cur[BTN_RIGHT]  |= g_dpad[1];
    g_cur[BTN_UP]     |= g_dpad[2];
    g_cur[BTN_DOWN]   |= g_dpad[3];
    g_cur[BTN_JUMP]   |= g_tbDown[TB_JUMP];
    g_cur[BTN_ACCEPT] |= g_tbDown[TB_JUMP];
    g_cur[BTN_SHOOT]  |= g_tbDown[TB_SHOOT];
    g_cur[BTN_START]  |= g_tbDown[TB_PAUSE];
    g_cur[BTN_BACK]   |= g_tbDown[TB_BACK];
#endif
    for (SDL_GameController* pad : g_pads) {
        auto b = [pad](SDL_GameControllerButton x) { return SDL_GameControllerGetButton(pad, x) != 0; };
        int ax = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        int ay = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        const int dz = 16000;
        auto Pb = [&](int a) { int bt = g_bind[a].pad; return bt >= 0 && b((SDL_GameControllerButton)bt); };
        g_cur[BTN_LEFT]   |= Pb(BIND_LEFT) || ax < -dz;
        g_cur[BTN_RIGHT]  |= Pb(BIND_RIGHT) || ax > dz;
        g_cur[BTN_UP]     |= Pb(BIND_UP) || ay < -dz;
        g_cur[BTN_DOWN]   |= Pb(BIND_DOWN) || ay > dz;
        if (g_remoteMode) {
            g_cur[BTN_LEFT] |= b(SDL_CONTROLLER_BUTTON_DPAD_LEFT); g_cur[BTN_RIGHT] |= b(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            g_cur[BTN_UP] |= b(SDL_CONTROLLER_BUTTON_DPAD_UP); g_cur[BTN_DOWN] |= b(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            g_cur[BTN_SHOOT] |= b(SDL_CONTROLLER_BUTTON_A);
        } else {
            g_cur[BTN_JUMP]  |= Pb(BIND_JUMP);
            g_cur[BTN_SHOOT] |= Pb(BIND_SHOOT) || (isDefault(BIND_SHOOT) && b(SDL_CONTROLLER_BUTTON_B));
        }
        g_cur[BTN_DEFEND] |= b(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        g_cur[BTN_ACCEPT] |= b(SDL_CONTROLLER_BUTTON_A) || Pb(BIND_JUMP);
        g_cur[BTN_BACK]   |= b(SDL_CONTROLLER_BUTTON_Y) || b(SDL_CONTROLLER_BUTTON_B) || (!g_remoteMode && Pb(BIND_SHOOT));
        g_cur[BTN_BACK]   |= b(SDL_CONTROLLER_BUTTON_BACK);
        g_cur[BTN_START]  |= Pb(BIND_PAUSE) || b(SDL_CONTROLLER_BUTTON_BACK);
        g_cur[BTN_CHEAT]  |= SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8000
                          && SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 8000;
    }
    if (g_capture == 1 || g_waitRelease) {
        bool any = false;
        int nk = 0; const Uint8* ks = SDL_GetKeyboardState(&nk);
        for (int i = 0; i < nk && !any; i++) any = ks[i] != 0;
        for (auto* p : g_pads) for (int bt = 0; bt < SDL_CONTROLLER_BUTTON_MAX && !any; bt++) any = SDL_GameControllerGetButton(p, (SDL_GameControllerButton)bt) != 0;
        if (g_capture != 1 && !any) g_waitRelease = false;
        memset(g_cur, 0, sizeof g_cur);
    }
#ifdef HOST
    if (g_remoteMode && k[SDL_SCANCODE_RETURN]) { g_cur[BTN_SHOOT] = true; }
#endif
#if defined(HOST) || defined(XA_FLOW_TEST)
    {
#ifdef XA_FLOW_TEST
        static bool flowMode = true;
#else
        static bool flowMode = getenv("XA_FLOW") != nullptr;
#endif
        static int flowT = 0;
        if (flowMode) {
            flowT++;
#if defined(XA_CYCLE_TEST)
            g_cur[BTN_ACCEPT] = flowT % 45 == 0;
            g_cur[BTN_JUMP] |= (flowT * 7919) % 13 == 0;
            g_cur[BTN_SHOOT] |= (flowT * 104729) % 11 == 0;
#elif defined(XA_FLOW_TEST)

            if (flowT < 1200) g_cur[BTN_ACCEPT] = flowT % 60 == 0;
            else if (flowT < 1500) {   }
            else if (flowT < 1800) g_cur[(flowT / 15) & 1 ? BTN_DOWN : BTN_UP] = flowT % 15 == 0;
            else if (flowT == 1830) g_cur[BTN_UP] = true;
            else if (flowT == 1860) g_cur[BTN_ACCEPT] = true;
            else if (flowT < 2160) {   }
            else if (flowT < 2460) g_cur[(flowT / 15) & 1 ? BTN_RIGHT : BTN_LEFT] = flowT % 15 == 0;
            else g_cur[BTN_ACCEPT] = flowT % 120 == 0;
#else
            g_cur[BTN_ACCEPT] = (flowT % 60) == 0;
#endif
        }
    }
#endif
#ifdef HOST

    if (const char* sp = getenv("XA_SPAM")) { static int st = 0; if (++st < atoi(sp)) g_cur[BTN_ACCEPT] = g_cur[BTN_JUMP] = (st & 1); }
    static bool autoMode = getenv("XA_AUTO") != nullptr;
    static int autoT = 0;
    static bool blockMode = autoMode && !strcmp(getenv("XA_AUTO"), "block");
    static bool remoteRun = autoMode && !strcmp(getenv("XA_AUTO"), "remote");
    if (remoteRun) {
        autoT++;
        g_cur[BTN_RIGHT] = autoT > 150 && autoT <= 200;
        g_cur[BTN_UP] = autoT >= 204 && autoT <= 207;
        g_cur[BTN_SHOOT] = autoT > 204;
    } else if (blockMode) {
        autoT++;
        g_cur[BTN_DOWN] = autoT > 20;
        g_cur[BTN_SHOOT] = autoT > 60;
    } else if (autoMode) {
        autoT++;
        g_cur[BTN_RIGHT] = autoT > 30;
        g_cur[BTN_JUMP] = autoT > 30 && ((autoT % 90) < 10 || ((autoT % 90) >= 22 && (autoT % 90) < 28));
        g_cur[BTN_SHOOT] = (autoT % 120) > 60;
    }
#endif
}
bool held(Button b) { return g_cur[b]; }
bool pressed(Button b) { return g_cur[b] && !g_prev[b]; }
bool released(Button b) { return !g_cur[b] && g_prev[b]; }
bool anyPressed() {
#ifdef XA_TOUCH
    if (g_tap) return true;
#endif
    for (int i = 0; i < BTN_COUNT; i++) if (i != BTN_CHEAT && pressed((Button)i)) return true;
    return false;
}
bool quitRequested() { return g_quit; }

static std::vector<uint8_t> g_musicData;
static Mix_Music* g_music = nullptr;
static std::string g_musicName;
static std::string g_musicDataName;
static std::map<std::string, Mix_Chunk*> g_sounds;

static volatile uint32_t g_mixBytes = 0;
static volatile int g_mixPeak = 0;

static volatile int g_musicGain = 128;

static volatile uint32_t g_cbMaxGap = 0, g_cbLate = 0, g_cbCount = 0; static volatile int g_queueMin = 1 << 30;
static int g_mainPrio = -1;

static void postMixProbe(void*, Uint8* stream, int len) {
    g_mixBytes += len;
    const int16_t* s = (const int16_t*)stream;
    int peak = g_mixPeak;
    for (int i = 0; i < len / 2; i += 16) { int v = s[i] < 0 ? -s[i] : s[i]; if (v > peak) peak = v; }
    g_mixPeak = peak;
}
void audioDiagnostics() {
    static uint32_t lastT = 0; static unsigned lastF = 0;
    uint32_t now = ticksMs();
    float fps = lastT ? (g_frameCount - lastF) * 1000.0f / (now - lastT) : 0;
    lastT = now; lastF = g_frameCount;
    log("audio: mixed=%u bytes peak=%d music=%s fps=%.1f refills=%u maxgap=%u ms late=%u queuemin=%d", (unsigned)g_mixBytes, g_mixPeak, Mix_PlayingMusic() ? "playing" : "stopped", fps,
        (unsigned)g_cbCount, (unsigned)g_cbMaxGap, (unsigned)g_cbLate, g_queueMin == (1 << 30) ? -1 : (int)g_queueMin);
    g_cbMaxGap = 0; g_cbLate = 0; g_cbCount = 0; g_queueMin = 1 << 30;
    g_mixPeak = 0;
}

bool audioInit() {
    int rate = 44100;
    int samples = 2048;
    if (Mix_OpenAudio(rate, AUDIO_S16SYS, 2, samples) < 0) { error("Mix_OpenAudio: %s", Mix_GetError()); return false; }
    int inited = Mix_Init(MIX_INIT_OGG);
    Mix_AllocateChannels(16);
    int qr = 0, qc = 0; Uint16 qf = 0;
    Mix_QuerySpec(&qr, &qf, &qc);
    log("audio: driver=%s rate=%d fmt=0x%x ch=%d ogg=%s", SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "none",
        qr, qf, qc, (inited & MIX_INIT_OGG) ? "yes" : "NO");
    Mix_SetPostMix(postMixProbe, nullptr);
    return true;
}

Mix_Chunk* loadSound(const std::string& name) {
    auto it = g_sounds.find(name);
    if (it != g_sounds.end()) return it->second;

    std::vector<uint8_t> data;
    size_t slash = name.find_last_of('/'), dot = name.find_last_of('.');
    if (slash != std::string::npos && dot != std::string::npos && dot > slash) {
        std::string wav = "assets/audio/fx/wav/" + name.substr(slash + 1, dot - slash - 1) + ".wav";
        if (assetExists(wav)) data = assetRead(wav);
    }
    if (data.empty()) data = assetRead(name);
    Mix_Chunk* c = nullptr;
    if (!data.empty()) c = Mix_LoadWAV_RW(SDL_RWFromConstMem(data.data(), (int)data.size()), 1);
    if (!c && !data.empty()) log("Mix_LoadWAV %s: %s", name.c_str(), Mix_GetError());
    g_sounds[name] = c;
    return c;
}

static std::map<std::string, std::vector<uint8_t>> g_smallMusic;
void preloadMusic(const std::string& name) {
    if (!g_smallMusic.count(name)) { auto d = assetRead(name); if (d.size() < 700 * 1024) g_smallMusic[name] = std::move(d); }
}

void dropMusicCache() {
    for (auto it = g_smallMusic.begin(); it != g_smallMusic.end();) {
        if (it->first == g_musicName) { ++it; continue; }
        it = g_smallMusic.erase(it);
    }
    if (g_musicName != g_musicDataName) { g_musicData.clear(); g_musicData.shrink_to_fit(); g_musicDataName.clear(); }
}

void loadMusicData(const std::string& name) {
    if (g_smallMusic.count(name) || (name == g_musicDataName && !g_musicData.empty())) return;
    if (g_music && g_musicName == g_musicDataName) stopMusic();
    g_musicData.clear(); g_musicData.shrink_to_fit(); g_musicDataName.clear();
    g_musicData = assetRead(name);
    g_musicDataName = name;
}
void playMusic(const std::string& name, bool loop) {
    if (name == g_musicName && g_music && Mix_PlayingMusic()) return;
    uint32_t t0 = SDL_GetTicks();
    stopMusic();
    uint32_t t1 = SDL_GetTicks();

    const std::vector<uint8_t>* data = nullptr;
    auto si = g_smallMusic.find(name);
    if (si != g_smallMusic.end()) data = &si->second;
    else if (name == g_musicDataName && !g_musicData.empty()) data = &g_musicData;
    else {
        auto d = assetRead(name);
        if (d.size() < 700 * 1024) data = &(g_smallMusic[name] = std::move(d));
        else { g_musicData.clear(); g_musicData.shrink_to_fit(); g_musicData = std::move(d); g_musicDataName = name; data = &g_musicData; }
    }
    if (data->empty()) return;
    g_music = Mix_LoadMUS_RW(SDL_RWFromConstMem(data->data(), (int)data->size()), 1);
    if (!g_music) { log("Mix_LoadMUS %s: %s", name.c_str(), Mix_GetError()); return; }
    g_musicName = name;
    Mix_PlayMusic(g_music, loop ? -1 : 0);
    log("timing: playMusic %s stop=%u ms total=%u ms", name.c_str(), t1 - t0, SDL_GetTicks() - t0);
}
void pauseMusic(bool p) { if (p) Mix_PauseMusic(); else Mix_ResumeMusic(); }
bool musicPlaying() { return Mix_PlayingMusic() && !Mix_PausedMusic(); }
void stopMusicUnless(const std::string& name) { if (!(name == g_musicName && g_music && Mix_PlayingMusic())) stopMusic(); }
void stopMusic() {
    if (g_music) { Mix_HaltMusic(); Mix_FreeMusic(g_music); g_music = nullptr; }
    g_musicName.clear();
    flushSave();
}
void setSfxVolume(int v) { Mix_Volume(-1, v); }
void setMusicVolume(int v) { Mix_VolumeMusic(v); }
void setFullscreen(bool on) {
#ifdef HOST
    SDL_SetWindowFullscreen(g_win, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#else
    (void)on;
#endif
}
int playSound(Mix_Chunk* c, int loops) { return c ? Mix_PlayChannel(-1, c, loops) : -1; }

uint32_t ticksMs() { return SDL_GetTicks(); }

uint32_t freeMemoryKB() {
    return 0;
}

static const char* savePath() {
#ifdef __ANDROID__
    static std::string p;
    if (p.empty()) { const char* d = SDL_AndroidGetInternalStoragePath(); p = std::string(d ? d : ".") + "/save.xas"; }
    return p.c_str();
#else
    static std::string p;
    if (p.empty()) { char* d = SDL_GetBasePath(); p = std::string(d ? d : "") + "save.xas"; SDL_free(d); }
    return p.c_str();
#endif
}
#define SAVE_PATH savePath()
static std::string controlsPath() {
    std::string s = savePath();
    size_t slash = s.find_last_of("/\\");
    return (slash == std::string::npos ? std::string() : s.substr(0, slash + 1)) + "controls.cfg";
}

static bool saveWriteNow(const std::vector<uint8_t>& d) {
    uint32_t t0 = SDL_GetTicks();
    FILE* f = fopen(SAVE_PATH, "wb");
    if (!f) return false;
    fwrite(d.data(), 1, d.size(), f);
    fclose(f);
    log("timing: save write %u bytes %u ms", (unsigned)d.size(), SDL_GetTicks() - t0);
    return true;
}

static std::vector<uint8_t> g_pendingSave;
static bool g_savePending = false;
void flushSave() {
    if (!g_savePending) return;
    g_savePending = false;
    saveWriteNow(g_pendingSave);
}
bool saveWrite(const std::vector<uint8_t>& d) {
    return saveWriteNow(d);
}
std::vector<uint8_t> saveRead() {
    if (g_savePending) return g_pendingSave;
    std::vector<uint8_t> d;
    FILE* f = fopen(SAVE_PATH, "rb");
    if (!f) return d;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n > 0) { d.resize(n); fread(d.data(), 1, n, f); }
    fclose(f);
    return d;
}

}

namespace plat {

static void (*g_lowMemCb)() = nullptr;
static void newHandler() {
    if (!g_lowMemTried) {
        g_lowMemTried = true;
        log("low memory: freeing caches (heap in use %u KB)", freeMemoryKB());
        cancelPrefetches();
        dropMusicCache();
        {
            uint32_t now = freeMemoryKB();
            int n = evictPages(0, now > 4000 ? now - 4000 : 0);
            log("low memory: evicted %d texture pages, heap now %u KB", n, freeMemoryKB());
        }
        if (g_lowMemCb) g_lowMemCb();
        return;
    }
    fatal("out of memory (heap in use %u KB)", freeMemoryKB());
}
void setLowMemoryHandler(void (*cb)()) {
    g_lowMemCb = cb;
    std::set_new_handler(newHandler);
}
}

namespace plat {

int localHour() {
    time_t t = time(nullptr);
    struct tm* lt = localtime(&t);
    return lt ? lt->tm_hour : 12;
}
}
