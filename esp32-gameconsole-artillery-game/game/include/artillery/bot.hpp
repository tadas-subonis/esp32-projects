#pragma once

#include "artillery/types.hpp"
#include "artillery/world.hpp"

namespace artillery {

Shot choose_bot_shot(const World& world, PlayerId bot, int wind, int last_power = kDefaultPower);

}  // namespace artillery
