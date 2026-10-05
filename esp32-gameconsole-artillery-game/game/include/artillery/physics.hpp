#pragma once

#include "artillery/types.hpp"
#include "artillery/world.hpp"

namespace artillery {

Projectile launch_shot(const World& world, PlayerId shooter, Shot shot, int wind);

bool step_projectile(const World& world, Projectile& proj, int wind);

ShotOutcome simulate_shot(const World& world, PlayerId shooter, Shot shot, int wind);

ShotOutcome outcome_at(const World& world, Vec2 pos);

void apply_explosion(World& world, const ShotOutcome& outcome, PlayerId shooter);

}  // namespace artillery
