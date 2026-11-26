#include "platform.h"
#include "launcher/launcher.h"
#include <string>

// Busca la carpeta con v1/ y v2/ (en PC); en Android están dentro del APK.
static bool openAssets(int argc, char** argv) {
    if (argc > 1 && plat::assetsOpen(argv[1])) return true;
    if (argc > 0 && argv[0]) {
        std::string p = argv[0];
        size_t s = p.find_last_of("/\\");
        if (s != std::string::npos) {
            std::string dir = p.substr(0, s + 1);
            plat::assetsDataDir(dir);
            if (plat::assetsOpen(dir.c_str())) return true;
        }
    }
    for (auto c : { "", "../", "../../" }) if (plat::assetsOpen(c)) return true;
    return false;
}

int main(int argc, char** argv) {
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#endif
#ifdef HOST
    // Capturas automáticas (XA_SHOT): la ventana no toma el foco para no molestar al usuario.
    if (getenv("XA_SHOT")) SDL_SetHint("SDL_WINDOW_ACTIVATE_WHEN_SHOWN", "0");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) < 0) {
        plat::fatal("SDL_Init: %s", SDL_GetError());
        return 1;
    }
#ifdef __ANDROID__
    if (!plat::assetsOpen("")) plat::fatal("faltan los juegos dentro del APK");
#else
    if (!openAssets(argc, argv)) plat::fatal("no se encontraron las carpetas v1 y v2");
#endif
    plat::loadBindings();
    int logicalW = plat::wideScreenWanted() ? plat::wideLogicalWidth(vc::WIDE_W, vc::BASE_H) : vc::BASE_W;
    if (!plat::videoInit(logicalW, vc::BASE_H)) plat::fatal("video init failed");
#ifdef __ANDROID__
    for (int i = 0; i < 20; i++) { SDL_PumpEvents(); SDL_Delay(15); }
    plat::setLogicalWidth(plat::wideLogicalWidth(vc::WIDE_W, vc::BASE_H));
#endif
    plat::audioInit();
    plat::setTouchShoot(false);   // solo Vascolet 2 muestra el botón de disparo (boomerang)
    vc::runLauncher();
    SDL_Quit();
    return 0;
}
