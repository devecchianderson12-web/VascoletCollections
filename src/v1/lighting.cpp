// Motor de iluminación HD (Opciones > Gráficos > CALIDAD HD).
//
// Todo se hace con geometría de SDL (sin render targets ni shaders), así rinde en cualquier celular:
// - Mapa de luz por vértices (malla de 16 px) aplicado con "modulación 2x": 128 deja la imagen igual,
//   menos oscurece y más ilumina. Así las luces realmente iluminan (no solo oscurecen las sombras).
//   Cada vértice suma luz ambiente con gradación de color cinematográfica (sombras frías, luces
//   cálidas), viñeta, profundidad dentro del terreno y luces puntuales con sombras (la luz se
//   atenúa al atravesar bloques sólidos).
// - Atmósfera: perspectiva aérea sobre el fondo y niebla volumétrica que se desplaza detrás del terreno.
// - Post-proceso: rayos de luz (god rays), resplandor del sol, bloom de las fuentes de luz y polvo
//   flotando en el aire que brilla cerca de las luces.
// - Volumen: sombras proyectadas del terreno y de los sprites, luz de borde (rim light) en los
//   sprites, sombras de contacto, oclusión bajo salientes y borde iluminado arriba del terreno.
#include "lighting.h"
#include "world.h"
#include "game.h"
#include <algorithm>
#include <cmath>

namespace v1 {

namespace {

struct HdLight { float x, y, r, cr, cg, cb, k, glow; bool occl; };
std::vector<HdLight> g_lights;
std::vector<SDL_Vertex> g_verts;
std::vector<int> g_idx;
std::vector<SDL_Vertex> g_batch; // triángulos sueltos que se dibujan juntos en una sola llamada
bool g_twoX = false;             // el renderer soporta la modulación 2x

// Dirección de la luz principal (arriba a la izquierda): hacia dónde caen las sombras.
const float SUN_DX = 5.0f, SUN_DY = 6.0f;
const int GRID = 16;
const int MAX_LIGHTS = 48;
// Brillo según la profundidad dentro del terreno (0 = aire, 1 = borde, 4+ = macizo).
const float DEPTH_SHADE[5] = { 1.0f, 0.97f, 0.74f, 0.58f, 0.48f };

inline Uint8 u8(float v) { return (Uint8)(std::max(0.0f, std::min(1.0f, v)) * 255.0f + 0.5f); }
inline float frac(float v) { return v - std::floor(v); }
inline float hash(int i, int salt) { return frac(std::sin(i * 12.9898f + salt * 78.233f) * 43758.5453f); }
inline float nowSec() { return plat::ticksMs() / 1000.0f; }

// Modulación 2x: resultado = 2 * origen * destino. Si el renderer no la soporta se usa MOD normal.
SDL_BlendMode mod2x() {
    static const SDL_BlendMode m = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_DST_COLOR, SDL_BLENDFACTOR_SRC_COLOR, SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
    return m;
}

void flush() {
    if (!g_batch.empty()) SDL_RenderGeometry(plat::renderer(), nullptr, g_batch.data(), (int)g_batch.size(), nullptr, 0);
    g_batch.clear();
}
void setBlend(SDL_BlendMode m) { flush(); SDL_SetRenderDrawBlendMode(plat::renderer(), m); }

// Círculo/elipse con degradado radial del centro (c0) al borde (c1). Se acumula en el lote.
void radial(float cx, float cy, float rx, float ry, SDL_Color c0, SDL_Color c1, int n = 24) {
    for (int i = 0; i < n; i++) {
        const float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n;
        g_batch.push_back({ { cx, cy }, c0, { 0, 0 } });
        g_batch.push_back({ { cx + std::cos(a0) * rx, cy + std::sin(a0) * ry }, c1, { 0, 0 } });
        g_batch.push_back({ { cx + std::cos(a1) * rx, cy + std::sin(a1) * ry }, c1, { 0, 0 } });
    }
}

// Cuadrilátero con color por esquina (a arriba-izq, b arriba-der, c abajo-der, d abajo-izq).
void quad(SDL_FPoint pa, SDL_FPoint pb, SDL_FPoint pc, SDL_FPoint pd, SDL_Color a, SDL_Color b, SDL_Color c, SDL_Color d) {
    g_batch.push_back({ pa, a, { 0, 0 } }); g_batch.push_back({ pb, b, { 0, 0 } }); g_batch.push_back({ pd, d, { 0, 0 } });
    g_batch.push_back({ pb, b, { 0, 0 } }); g_batch.push_back({ pc, c, { 0, 0 } }); g_batch.push_back({ pd, d, { 0, 0 } });
}
void vgrad(float x, float y, float w, float h, SDL_Color c0, SDL_Color c1) {
    quad({ x, y }, { x + w, y }, { x + w, y + h }, { x, y + h }, c0, c0, c1, c1);
}

bool solidFlags(uint8_t f) { return (f & TF_HARD) && !(f & TF_INVISIBLE); }
bool surfaceFlags(uint8_t f) { return (f & (TF_HARD | TF_PLATFORM)) && !(f & TF_INVISIBLE); }

} // namespace

void hdAddLight(float wx, float wy, float radius, float r, float g, float b, float strength, float glow, bool occluded) {
    if ((int)g_lights.size() >= MAX_LIGHTS || radius <= 0) return;
    g_lights.push_back({ wx, wy, radius, r, g, b, strength, glow, occluded });
}

void hdSpriteShadow(const Frame& f, float x, float y, bool flipH) {
    if (hdFx(HDFX_SHADOWS)) {
        plat::setDrawTint({ 0, 0, 0, 80 });                     // sombra proyectada
        drawFrame(f, x + SUN_DX * 0.6f, y + SUN_DY * 0.35f, flipH);
    }
    if (hdFx(HDFX_RIM)) {
        plat::setDrawTint({ 255, 214, 150, 120 });              // luz de borde (rim light), aditiva
        plat::setDrawAdditive(true);
        drawFrame(f, x - 1.4f, y - 1.4f, flipH);
        plat::setDrawAdditive(false);
    }
    plat::setDrawTint({ 255, 255, 255, 255 });
}

// Profundidad atmosférica sobre el fondo: perspectiva aérea y niebla que se desplaza.
void World::hdBackdrop(int cx, int cy) {
    if (!hdFx(HDFX_FOG)) return;
    const float W = (float)LOGICAL_W, H = (float)LOGICAL_H, t = nowSec();
    setBlend(SDL_BLENDMODE_BLEND);
    vgrad(0, 0, W, H * 0.55f, { 70, 96, 140, 34 }, { 30, 42, 66, 52 });
    vgrad(0, H * 0.55f, W, H * 0.45f, { 30, 42, 66, 52 }, { 14, 18, 30, 86 });
    // Niebla volumétrica: bancos suaves con paralaje (más lentos que la cámara = más lejos).
    for (int i = 0; i < 7; i++) {
        const float span = W + 520.0f;
        const float px = frac((hash(i, 1) * span + t * (6.0f + 10.0f * hash(i, 2)) - cx * 0.35f) / span) * span - 260.0f;
        const float py = H * (0.45f + 0.45f * hash(i, 3)) - cy * 0.08f + std::sin(t * 0.3f + i) * 6.0f;
        const float rx = 150.0f + 130.0f * hash(i, 4), ry = 34.0f + 30.0f * hash(i, 5);
        radial(px, py, rx, ry, { 196, 210, 232, (Uint8)(26 + 18 * hash(i, 6)) }, { 196, 210, 232, 0 });
    }
    flush();
}

// Silueta del terreno desplazada: proyecta sombra sobre el fondo.
void World::hdTileShadows(int cx, int cy) {
    if (!tiles || !hdFx(HDFX_SHADOWS)) return;
    plat::setDrawTint({ 0, 0, 0, 110 });
    const int tx0 = (int)std::floor(cx / (float)map.tileW) - 1, ty0 = (int)std::floor(cy / (float)map.tileH) - 1;
    for (int ty = ty0; ty <= ty0 + LOGICAL_H / map.tileH + 2; ty++)
        for (int tx = tx0; tx <= tx0 + LOGICAL_W / map.tileW + 2; tx++) {
            uint32_t g = map.gidAt(tx, ty);
            if (!g || (map.flagsAt(tx, ty) & TF_INVISIBLE)) continue;
            SDL_FRect d = { tx * map.tileW - cx + SUN_DX, ty * map.tileH - cy + SUN_DY, (float)map.tileW, (float)map.tileH };
            plat::drawTile(tiles, (int)g - map.firstGid, d);
        }
    plat::setDrawTint({ 255, 255, 255, 255 });
}

// Borde superior iluminado del terreno expuesto y oclusión debajo de los salientes.
void World::hdTileRims(int cx, int cy) {
    const int tw = map.tileW, th = map.tileH;
    const int tx0 = (int)std::floor(cx / (float)tw), ty0 = (int)std::floor(cy / (float)th);
    for (int pass = 0; pass < 2; pass++) {
        if (!hdFx(pass == 0 ? HDFX_RIM : HDFX_SHADOWS)) continue;
        setBlend(pass == 0 ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
        for (int ty = ty0; ty <= ty0 + LOGICAL_H / th + 1; ty++)
            for (int tx = tx0; tx <= tx0 + LOGICAL_W / tw + 1; tx++) {
                const float x = (float)(tx * tw - cx), y = (float)(ty * th - cy);
                const uint8_t f = map.flagsAt(tx, ty), up = map.flagsAt(tx, ty - 1);
                if (pass == 0 && surfaceFlags(f) && !surfaceFlags(up))
                    vgrad(x, y, (float)tw, 8, { 84, 70, 44, 255 }, { 0, 0, 0, 255 });
                else if (pass == 1 && !f && solidFlags(up))
                    vgrad(x, y, (float)tw, th * 0.6f, { 0, 0, 0, 120 }, { 0, 0, 0, 0 });
            }
    }
    setBlend(SDL_BLENDMODE_BLEND);
}

// Sombra suave en el piso debajo de un personaje (se achica y aclara si está en el aire).
void World::hdContactShadow(float wx, float feetY, bool grounded, float width, int cx, int cy) {
    if (!hdFx(HDFX_SHADOWS)) return;
    float gy = feetY;
    if (!grounded) {
        bool found = false;
        const int tx = (int)std::floor(wx / map.tileW);
        for (int ty = (int)std::floor(feetY / map.tileH); ty < (int)std::floor(feetY / map.tileH) + 7; ty++)
            if (surfaceFlags(map.flagsAt(tx, ty))) { gy = (float)(ty * map.tileH); found = true; break; }
        if (!found) return;
    }
    const float h = std::max(0.0f, gy - feetY);
    if (h > 200) return;
    const float k = 1.0f - h / 200.0f;
    setBlend(SDL_BLENDMODE_BLEND);
    const float w = width * (0.55f + 0.45f * k);
    radial(wx - cx + 2, gy - cy + 1, w, w * 0.22f, { 0, 0, 0, (Uint8)(135 * k) }, { 0, 0, 0, 0 });
    flush();
}

void World::hdLighting(int cx, int cy) {
    SDL_Renderer* ren = plat::renderer();
    const int W = LOGICAL_W, H = LOGICAL_H;
    const float t = nowSec();
    flush();
    g_twoX = SDL_SetRenderDrawBlendMode(ren, mod2x()) == 0;
    if (hdFx(HDFX_LIGHTS)) drawLightMap(cx, cy);

    // ---- Post-proceso aditivo ----
    setBlend(SDL_BLENDMODE_ADD);
    const float fW = (float)W, fH = (float)H;
    const SDL_Color none = { 0, 0, 0, 255 };
    const float glowBoost = g_twoX ? 1.5f : 1.0f;
    if (hdFx(HDFX_BLOOM))   // resplandor del sol arriba a la izquierda
        radial(fW * 0.1f, -fH * 0.2f, fW * 0.75f, fH * 0.9f, { 46, 38, 24, 255 }, none, 32);
    // Rayos de luz (god rays) que bajan en diagonal, con paralaje y respiración lenta.
    for (int i = 0; i < 5 && hdFx(HDFX_RAYS); i++) {
        const float span = fW + 300.0f;
        const float x = frac((hash(i, 7) * span - cx * 0.25f) / span) * span - 150.0f;
        const float w0 = 18.0f + 30.0f * hash(i, 8), w1 = w0 * 3.2f, slant = fH * 0.55f;
        const float in = (0.55f + 0.45f * std::sin(t * (0.25f + 0.2f * hash(i, 9)) + i * 1.7f)) * (0.6f + 0.4f * hash(i, 10));
        const SDL_Color top = { u8(0.13f * in), u8(0.11f * in), u8(0.075f * in), 255 };
        quad({ x, 0 }, { x + w0, 0 }, { x + slant + w1, fH * 0.9f }, { x + slant, fH * 0.9f }, top, top, none, none);
    }
    // Bloom de las fuentes de luz: halo amplio y núcleo intenso.
    for (const HdLight& l : g_lights) {
        if (l.glow <= 0 || !hdFx(HDFX_BLOOM)) continue;
        const float gr = std::max(12.0f, l.r * 0.45f), sx = l.x - cx, sy = l.y - cy;
        if (sx < -gr || sy < -gr || sx > W + gr || sy > H + gr) continue;
        const float gl = l.glow * glowBoost;
        radial(sx, sy, gr, gr, { u8(l.cr * gl * 0.6f), u8(l.cg * gl * 0.6f), u8(l.cb * gl * 0.6f), 255 }, none);
        radial(sx, sy, gr * 0.3f, gr * 0.3f, { u8(l.cr * gl), u8(l.cg * gl), u8(l.cb * gl), 255 }, none, 16);
    }
    // Polvo flotando en el aire: brilla más cerca de las luces.
    for (int i = 0; i < 44 && hdFx(HDFX_DUST); i++) {
        const float spanX = fW + 40.0f, spanY = fH + 40.0f;
        const float px = frac((hash(i, 11) * spanX + t * (4.0f + 8.0f * hash(i, 12)) - cx * 0.85f) / spanX) * spanX - 20.0f;
        const float py = frac((hash(i, 13) * spanY - t * (3.0f + 5.0f * hash(i, 14)) - cy * 0.85f) / spanY) * spanY - 20.0f
                         + std::sin(t * 0.8f + i) * 4.0f;
        float lit = 0.18f;
        for (const HdLight& l : g_lights) {
            const float ddx = px + cx - l.x, ddy = py + cy - l.y, d2 = ddx * ddx + ddy * ddy;
            if (d2 < l.r * l.r) lit += (1.0f - d2 / (l.r * l.r)) * 0.9f;
        }
        lit = std::min(1.0f, lit) * (0.5f + 0.25f * std::sin(t * 1.3f + i * 2.1f));
        const float sz = 1.0f + 1.6f * hash(i, 15);
        radial(px, py, sz, sz, { u8(0.55f * lit), u8(0.5f * lit), u8(0.4f * lit), 255 }, none, 6);
    }
    setBlend(SDL_BLENDMODE_BLEND);
    g_lights.clear();
}

// Mapa de luz por vértices (ver comentario al principio del archivo).
void World::drawLightMap(int cx, int cy) {
    SDL_Renderer* ren = plat::renderer();
    const int W = LOGICAL_W, H = LOGICAL_H, tw = std::max(1, map.tileW), th = std::max(1, map.tileH);
    const bool twoX = g_twoX;
    const float enc = twoX ? 0.5f : 1.0f;                       // valor de la malla que deja la imagen igual
    const float boost = twoX ? 1.8f : 1.0f;
    SDL_SetRenderDrawBlendMode(ren, twoX ? mod2x() : SDL_BLENDMODE_MOD);

    // Profundidad dentro del terreno, en la zona visible más un margen.
    const int M = 5;
    const int dx0 = (int)std::floor(cx / (float)tw) - M, dy0 = (int)std::floor(cy / (float)th) - M;
    const int dw = W / tw + 2 * M + 2, dh = H / th + 2 * M + 2;
    hdDepth.assign((size_t)(dw * dh), 0);
    for (int j = 0; j < dh; j++)
        for (int i = 0; i < dw; i++) {
            const int tx = std::max(0, std::min(map.width - 1, dx0 + i)), ty = std::max(0, dy0 + j);
            // Debajo del mapa se considera terreno macizo si la última fila lo es.
            hdDepth[j * dw + i] = solidFlags(map.flagsAt(tx, std::min(ty, map.height - 1))) ? 9 : 0;
        }
    for (int k = 1; k <= 4; k++)
        for (int j = 0; j < dh; j++)
            for (int i = 0; i < dw; i++) {
                uint8_t& d = hdDepth[j * dw + i];
                if (d <= k) continue;
                bool edge = false;
                for (int b = -1; b <= 1 && !edge; b++)
                    for (int a = -1; a <= 1 && !edge; a++) {
                        const int ii = i + a, jj = j + b;
                        if (ii < 0 || jj < 0 || ii >= dw || jj >= dh) continue;
                        edge = hdDepth[jj * dw + ii] == k - 1;
                    }
                if (edge) d = (uint8_t)k;
            }
    auto depthAt = [&](int tx, int ty) -> int {
        const int i = std::max(0, std::min(dw - 1, tx - dx0)), j = std::max(0, std::min(dh - 1, ty - dy0));
        return std::min(4, (int)hdDepth[j * dw + i]);
    };
    auto shadeAt = [&](float wx, float wy) {
        const float fx = wx / tw - 0.5f, fy = wy / th - 0.5f;
        const int ix = (int)std::floor(fx), iy = (int)std::floor(fy);
        const float u = fx - ix, v = fy - iy;
        const float a = DEPTH_SHADE[depthAt(ix, iy)], b = DEPTH_SHADE[depthAt(ix + 1, iy)];
        const float c = DEPTH_SHADE[depthAt(ix, iy + 1)], d = DEPTH_SHADE[depthAt(ix + 1, iy + 1)];
        return (a * (1 - u) + b * u) * (1 - v) + (c * (1 - u) + d * u) * v;
    };
    auto solidAtW = [&](float wx, float wy) { return depthAt((int)std::floor(wx / tw), (int)std::floor(wy / th)) > 0; };

    // Malla de vértices alineada al mundo.
    const int ox = -(((cx % GRID) + GRID) % GRID), oy = -(((cy % GRID) + GRID) % GRID);
    const int cols = W / GRID + 3, rows = H / GRID + 3;
    g_verts.resize((size_t)(cols * rows));
    for (int j = 0; j < rows; j++)
        for (int i = 0; i < cols; i++) {
            const float sx = (float)(ox + i * GRID), sy = (float)(oy + j * GRID);
            const float wx = sx + cx, wy = sy + cy;
            const float ny = std::max(0.0f, std::min(1.0f, sy / H));
            const float amb = 0.9f - 0.16f * ny;                     // cielo más claro arriba
            const float vx = (sx / W - 0.5f) * 2.0f, vy = (sy / H - 0.5f) * 2.0f;
            const float vig = std::max(0.62f, 1.0f - 0.24f * (vx * vx * 0.7f + vy * vy));
            const float base = amb * vig * shadeAt(wx, wy);
            // Gradación: sombras frías (azul verdoso), medios neutros.
            const float cool = 1.0f - std::min(1.0f, base);
            float r = base * (1.0f - 0.12f * cool), g = base * (1.0f - 0.03f * cool), b = base * (1.0f + 0.1f * cool);
            for (const HdLight& l : g_lights) {
                const float ddx = wx - l.x, ddy = wy - l.y, d2 = ddx * ddx + ddy * ddy;
                if (d2 >= l.r * l.r) continue;
                float f = 1.0f - d2 / (l.r * l.r);
                f *= f * l.k * boost;
                if (l.occl) {
                    // La luz se apaga al atravesar bloques sólidos: sombras reales detrás del terreno.
                    const int steps = (int)(std::sqrt(d2) / 8.0f);
                    float tr = 1.0f;
                    for (int s = 1; s < steps && tr > 0.05f; s++) {
                        const float q = s / (float)steps;
                        if (solidAtW(l.x + ddx * q, l.y + ddy * q)) tr *= 0.75f;
                    }
                    f *= tr;
                }
                r += l.cr * f; g += l.cg * f; b += l.cb * f;
            }
            g_verts[j * cols + i] = { { sx, sy }, { u8(r * enc), u8(g * enc), u8(b * enc), 255 }, { 0, 0 } };
        }
    g_idx.clear();
    for (int j = 0; j + 1 < rows; j++)
        for (int i = 0; i + 1 < cols; i++) {
            const int a = j * cols + i, b = a + 1, c = a + cols, d = c + 1;
            g_idx.insert(g_idx.end(), { a, b, c, b, d, c });
        }
    SDL_RenderGeometry(ren, nullptr, g_verts.data(), (int)g_verts.size(), g_idx.data(), (int)g_idx.size());
}

}
