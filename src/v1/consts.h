#pragma once

namespace v1 {

constexpr float GRAVITY        = 1800.0f;
constexpr float RUN_SPEED      = 250.0f;
constexpr float CLIMB_SPEED    = 150.0f;

constexpr float JUMP_SPEED[5]  = { -600, -700, -790, -810, -850 };

constexpr float DJUMP_SPEED[5] = { 0, -440, -680, -720, -760 };
constexpr float MAX_FALL_SPEED = 900.0f;

constexpr float BULLET_SPEED     = 500.0f;
constexpr float BULLET_OFFSET_X  = 25.0f;
constexpr float BULLET_OFFSET_Y  = 15.0f;
constexpr float BULLET_OFFSET_Y_AIR = 20.0f;

constexpr float FIRE_RATE        = 12.0f;
constexpr int   BURST_SHOTS      = 3;
constexpr float BURST_PAUSE      = 0.25f;
constexpr float BULLET_RANGE     = 768.0f;
constexpr float BULLET_LIFETIME  = BULLET_RANGE / 500.0f;

constexpr int   HERO_MAX_ENERGY  = 10;
constexpr int   HIT_DAMAGE       = 4;
constexpr float HIT_INVULN       = 0.5f;
constexpr float STOMP_BOUNCE     = -850.0f * 0.6f;
constexpr float JUMPER_JUMP_VY   = -200.0f;

constexpr int   RING_BULLETS     = 8;
constexpr float RING_SPEED       = 200.0f;
constexpr float BULLET_SIZE      = 6.0f;

constexpr int   BIRD_DEFAULT_BOBS = 4;

constexpr float HURT_INVULN_TIME = 1.5f;

constexpr float DEATH_ANIM_TIME  = 1.0f;
constexpr float FADE_OUT_TIME    = 0.85f;
constexpr float FADE_IN_TIME     = 0.8f;

}
