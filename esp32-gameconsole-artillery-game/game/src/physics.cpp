#include "artillery/physics.hpp"

#include <cmath>

namespace artillery {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float deg_to_rad(int deg) { return static_cast<float>(deg) * kPi / 180.0f; }

Vec2 muzzle_pos(const Tank& tank, PlayerId shooter, int angle_deg)
{
    const float rad = deg_to_rad(angle_deg);
    const float dir = shooter == PlayerId::P0 ? 1.0f : -1.0f;
    // P0 faces right: 0° is along +x, 90° is up. P1 faces left: same local angle.
    const float local = shooter == PlayerId::P0 ? static_cast<float>(angle_deg)
                                                : static_cast<float>(180 - angle_deg);
    const float lr = deg_to_rad(static_cast<int>(local));
    (void)dir;
    (void)rad;
    return Vec2{
        tank.x + std::cos(lr) * static_cast<float>(kBarrelLen),
        tank.y - std::sin(lr) * static_cast<float>(kBarrelLen),
    };
}

int splash_damage(float dist)
{
    if (dist >= static_cast<float>(kSplashRadius)) {
        return 0;
    }
    const float t = 1.0f - dist / static_cast<float>(kSplashRadius);
    return clamp_int(static_cast<int>(kDirectHitDamage * t * t + 0.5f), 0, kDirectHitDamage);
}

}  // namespace

Projectile launch_shot(const World& world, PlayerId shooter, Shot shot, int wind)
{
    (void)wind;
    shot.angle_deg = clamp_int(shot.angle_deg, kMinAngle, kMaxAngle);
    shot.power = clamp_int(shot.power, kMinPower, kMaxPower);

    const Tank& tank = world.tank(shooter);
    const float local = shooter == PlayerId::P0 ? static_cast<float>(shot.angle_deg)
                                                : static_cast<float>(180 - shot.angle_deg);
    const float rad = deg_to_rad(static_cast<int>(local));
    const float speed = static_cast<float>(shot.power) * kPowerScale;

    Projectile p;
    p.pos = muzzle_pos(tank, shooter, shot.angle_deg);
    p.vel = Vec2{std::cos(rad) * speed, -std::sin(rad) * speed};
    p.alive = true;
    p.shooter = shooter;
    // Step off the barrel so the first collision test is in open sky.
    p.pos.x += p.vel.x;
    p.pos.y += p.vel.y;
    return p;
}

bool step_projectile(const World& world, Projectile& proj, int wind)
{
    if (!proj.alive) {
        return false;
    }

    constexpr int kSub = 4;
    const float ax = static_cast<float>(wind) * kWindScale;
    const float ay = kGravity;
    for (int i = 0; i < kSub; ++i) {
        proj.vel.x += ax / static_cast<float>(kSub);
        proj.vel.y += ay / static_cast<float>(kSub);
        proj.pos.x += proj.vel.x / static_cast<float>(kSub);
        proj.pos.y += proj.vel.y / static_cast<float>(kSub);
        if (world.solid_at(proj.pos.x, proj.pos.y)) {
            proj.alive = false;
            return false;
        }
        if (proj.pos.y < -40.0f) {
            continue;
        }
        if (proj.pos.x < -8.0f || proj.pos.x > static_cast<float>(kWidth + 8) ||
            proj.pos.y > static_cast<float>(kHeight + 8)) {
            proj.alive = false;
            return false;
        }
        for (int t = 0; t < 2; ++t) {
            const Tank& tank = world.tank(static_cast<PlayerId>(t));
            if (!tank.alive || static_cast<PlayerId>(t) == proj.shooter) {
                continue;
            }
            const float dx = proj.pos.x - tank.x;
            const float dy = proj.pos.y - (tank.y - static_cast<float>(kTankH) * 0.5f);
            const float rr = 9.0f * 9.0f;
            if (dx * dx + dy * dy <= rr) {
                proj.alive = false;
                return false;
            }
        }
    }
    return proj.alive;
}

ShotOutcome simulate_shot(const World& world, PlayerId shooter, Shot shot, int wind)
{
    Projectile p = launch_shot(world, shooter, shot, wind);

    ShotOutcome out;
    for (int i = 0; i < kMaxShotSteps; ++i) {
        const bool flying = step_projectile(world, p, wind);
        out.steps = i + 1;
        if (!flying) {
            break;
        }
    }
    const int steps = out.steps;
    out = outcome_at(world, p.pos);
    out.steps = steps;
    return out;
}

ShotOutcome outcome_at(const World& world, Vec2 pos)
{
    ShotOutcome out;
    out.impact = pos;
    out.left_map = pos.x < 0.0f || pos.x >= static_cast<float>(kWidth) || pos.y < -20.0f ||
                   pos.y >= static_cast<float>(kHeight);
    out.hit_terrain = !out.left_map && world.solid_at(pos.x, pos.y + 0.5f);

    for (int t = 0; t < 2; ++t) {
        const Tank& tank = world.tank(static_cast<PlayerId>(t));
        if (!tank.alive) {
            continue;
        }
        const float dx = pos.x - tank.x;
        const float dy = pos.y - (tank.y - static_cast<float>(kTankH) * 0.5f);
        const float dist = std::sqrt(dx * dx + dy * dy);
        const int dmg = splash_damage(dist);
        out.damage[t] = dmg;
        if (dist <= 9.0f) {
            out.hit_tank[t] = true;
            out.hit_terrain = false;
        }
    }
    if (out.hit_tank[0] || out.hit_tank[1]) {
        out.hit_terrain = false;
    }
    return out;
}

void apply_explosion(World& world, const ShotOutcome& outcome, PlayerId shooter)
{
    (void)shooter;
    if (!outcome.left_map) {
        world.apply_crater(static_cast<int>(outcome.impact.x + 0.5f),
                           static_cast<int>(outcome.impact.y + 0.5f), kCraterRadius);
    }
    for (int t = 0; t < 2; ++t) {
        Tank& tank = world.tank(static_cast<PlayerId>(t));
        if (!tank.alive) {
            continue;
        }
        tank.hp = clamp_int(tank.hp - outcome.damage[t], 0, kTankHp);
        if (tank.hp <= 0) {
            tank.alive = false;
            tank.hp = 0;
        }
        world.settle_tank(tank);
        if (tank.y >= static_cast<float>(kHeight - 6)) {
            tank.alive = false;
            tank.hp = 0;
        }
    }
}

}  // namespace artillery
