#pragma once
#include <cmath>
#include <cstdlib>

namespace v1 {

// Gráficos > CÁMARA: la cámara queda centrada en el jugador y no se adelanta hacia donde mira.
inline bool g_cameraFixed = false;

struct GameCamera {
    float x = 0, y = 0;
    float w = 512, h = 384;
    float halfW = 256, halfH = 192;
    float velX = 0, velY = 0;
    float targetX = 0, targetY = 0;
    float shakeTime = 0;
    float shakeInterval = 1.0f / 30.0f;
    float shakeTimer = 0;
    float zoneX = 0.6f, zoneY = 0.6f;
    float lastHx = 0, lastHy = 0;
    int lastDir = 0;
    int mode = 0;
    int modeDir = 0;
    float levelW = 1024, levelH = 512;
    float minX = 0, minY = 0;   // Vascolet 1: la zona jugable no empieza en 0
    int lastVdir = 0;

    void init(float lw, float lh, float hx, float hy) {
        levelW = lw; levelH = lh;
        halfW = w * 0.5f; halfH = h * 0.5f;
        x = hx - halfW;
        y = hy - zoneY * h;
        clamp();
    }

    void setTargetX(float t, float speed) {
        if (speed > 2000.0f) speed = 2000.0f;
        targetX = t;
        float d = t - x;
        velX = d > 0 ? speed : (d < 0 ? -speed : 0.0f);
    }
    void setTargetY(float t, float speed) {
        if (speed > 2000.0f) speed = 2000.0f;
        targetY = t;
        float d = t - y;
        velY = d > 0 ? speed : (d < 0 ? -speed : 0.0f);
    }

    void shake(float seconds) { shakeTime = seconds; shakeTimer = 0; }

    void update(float dt, float heroX, float heroY, int facing, float heroVy, float heroH) {
        float hx = std::floor(heroX), hy = std::floor(heroY);
        float dX = hx - lastHx, dY = hy - lastHy;
        lastHx = hx; lastHy = hy;
        int dir = facing;
        if (dir == 0) dir = lastDir;
        if (g_cameraFixed) {
            mode = 0; velX = 0;
        } else if (mode == 0) {
            if (dir == lastDir || lastDir == 0)
                setTargetX(hx - halfW + (zoneX - 0.5f) * dir * w, 800.0f);
            else { mode = 1; modeDir = lastDir; velX = -velX; }
        } else if (mode == 1) {
            float k = modeDir * ((hx - x) - (halfW - w * modeDir * (zoneX - 0.5f)));
            if (k > 0) mode = 0;
            else if (k < 5.0f) mode = 0;
        }
        lastDir = dir;
        int vdir = dY > 0 ? 1 : (dY < 0 ? -1 : 0);
        if (vdir == 0) vdir = lastVdir;
        lastVdir = vdir;

        float fy = hy - 0.5f * heroH;
        float vspeed = heroVy >= 800.0f ? heroVy : 800.0f;
        if (fy - y < (1.0f - zoneY) * h) setTargetY(fy - (1.0f - zoneY) * h, vspeed);
        if (h * zoneY < fy - y) setTargetY(fy - h * zoneY, vspeed);

        float stepX = dt * velX, stepY = dt * velY;
        if (g_cameraFixed) x = hx - halfW;
        else if (mode == 0 && (hx - (1.0f - zoneX) * w < x || x + w < hx + (1.0f - zoneX) * w || dX != 0)) {
            if (std::fabs(targetX - x) <= std::fabs(stepX)) { x = targetX; velX = 0; }
            else if (std::fabs(x - hx) <= zoneX * w) x += stepX;
            else x += stepX * 3.0f;
        }
        if (std::fabs(targetY - y) <= std::fabs(stepY)) { y = targetY; velY = 0; }
        else y += stepY;

        updateShake(dt);
        clamp();
    }

private:

    void updateShake(float dt) {
        if (shakeTime <= 0) return;
        shakeTimer -= dt;
        if (shakeTimer <= 0) {
            auto offs = []() {
                float mag = 2.0f + (float)rand() * 2.0f / (float)RAND_MAX;
                float sign = ((float)rand() / (float)RAND_MAX < 0.5f) ? 1.0f : -1.0f;
                return sign * mag;
            };
            float ox = offs(), oy = offs();
            x += ox; y += oy;
            shakeTimer = shakeInterval;
        }
        shakeTime -= dt;
    }

    void clamp() {
        float ox = x;
        if (x > levelW - w) x = levelW - w;
        if (x < minX && levelW - minX >= w) x = minX;
        if (y > levelH - h) y = levelH - h;
        if (y < minY && levelH - minY >= h) y = minY;
        if (mode == 0 && x != ox) { mode = 1; velX = 0; }
    }
};

}
