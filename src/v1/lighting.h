#pragma once
#include "defs.h"

// Iluminación HD: solo se usa con Opciones > Gráficos > CALIDAD HD activado.
namespace v1 {

// Gráficos > AVANZADO: cada efecto se puede apagar. g_hdOff guarda los bits apagados.
enum HdFx { HDFX_LIGHTS, HDFX_SHADOWS, HDFX_RIM, HDFX_BLOOM, HDFX_RAYS, HDFX_FOG, HDFX_DUST, HDFX_COUNT };
inline uint8_t g_hdOff = 0;
inline bool hdFx(HdFx f) { return !((g_hdOff >> f) & 1); }

// Luces del cuadro actual (coordenadas del mundo). Se vacían al dibujar la iluminación.
void hdAddLight(float wx, float wy, float radius, float r, float g, float b, float strength, float glow, bool occluded);
// Volumen de un sprite: sombra proyectada abajo-derecha y luz de borde cálida arriba-izquierda.
void hdSpriteShadow(const Frame& f, float x, float y, bool flipH);
inline void hdSpriteShadow(const AnimPlayer& a, float x, float y, bool flipH) {
    if (const Frame* f = a.current()) hdSpriteShadow(*f, x, y, flipH);
}

}
