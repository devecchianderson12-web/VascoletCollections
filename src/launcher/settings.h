#pragma once
#include <cstdint>

namespace vc {

// Efectos de la iluminación HD que se pueden apagar por separado (Gráficos > Avanzado).
enum HdFx { HDFX_LIGHTS, HDFX_SHADOWS, HDFX_RIM, HDFX_BLOOM, HDFX_RAYS, HDFX_FOG, HDFX_DUST, HDFX_COUNT };

// Ajustes de la colección: los mismos que tiene el XA y valen para los dos juegos.
struct Settings {
    // Sonido (0-100)
    int music = 80, sfx = 80;
    // Gráficos
    int fullscreen = 0, smooth = 1, sharpness = 0, hd = 0, tvEffect = 0;
    int camFixed = 0, fps120 = 0, modernArt = 0;
    int stretch = 1;          // Vascolet 1: estirar la pantalla 4:3 del juego a toda la pantalla
    int hdOff = 0;            // bits de HdFx apagados
    // Progreso
    int v1Unlocked = 1;       // Vascolet 1: épocas habilitadas (se habilita la siguiente al pasar una)
    int v1Coins[5] = {};      // Vascolet 1: mejores monedas juntadas en cada época (de 100)
    // Controles táctiles
    int stickType = 0, touchStyle = 0, stickSize = 1, stickTheme = 0, touchAlpha = 100;
    uint16_t touchPos[10] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };

    bool fx(HdFx f) const { return hd && !((hdOff >> f) & 1); }
    void load();
    void save() const;
    void apply() const;
};

Settings& settings();

extern const int STICK_SIZES[4];
extern const char* const STICK_THEMES[5];
extern const char* const TOUCH_STYLES[3];

}
