#pragma once

#include "artillery/types.hpp"

namespace artillery {

inline ButtonEdges make_edges(const Buttons& prev, const Buttons& now)
{
    ButtonEdges e;
    e.down = now;
    e.pressed.up = now.up && !prev.up;
    e.pressed.down = now.down && !prev.down;
    e.pressed.left = now.left && !prev.left;
    e.pressed.right = now.right && !prev.right;
    e.pressed.a = now.a && !prev.a;
    e.pressed.b = now.b && !prev.b;
    e.released.up = !now.up && prev.up;
    e.released.down = !now.down && prev.down;
    e.released.left = !now.left && prev.left;
    e.released.right = !now.right && prev.right;
    e.released.a = !now.a && prev.a;
    e.released.b = !now.b && prev.b;
    return e;
}

}  // namespace artillery
