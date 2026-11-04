#pragma once
#include <algorithm>
#include <cmath>
#include <limits>

namespace Star::DirectionalMining {
struct Hit { bool found = false; int x = 0, y = 0; };
// Traverse grid cells in pointing order, not nearest Euclidean distance.
// A protected/indestructible occupied cell is still a hit: never skip it.
// The bounded query does not allocate; extreme mod ranges stop at 128 tiles.
template <typename Occupied>
Hit firstTile(float ox, float oy, float dx, float dy, float range, Occupied occupied) {
  if (!std::isfinite(ox) || !std::isfinite(oy) || !std::isfinite(dx)
      || !std::isfinite(dy) || !std::isfinite(range) || range <= 0
      || std::abs(ox) > 100000000 || std::abs(oy) > 100000000) return {};
  double length = std::hypot((double)dx, (double)dy);
  if (length < .001) return {};
  double vx = dx / length, vy = dy / length;
  double limit = std::min((double)range, 128.0);
  int x = (int)std::floor(ox), y = (int)std::floor(oy);
  int sx = vx > 0 ? 1 : -1, sy = vy > 0 ? 1 : -1;
  double inf = std::numeric_limits<double>::infinity();
  double stepX = vx == 0 ? inf : std::abs(1 / vx);
  double stepY = vy == 0 ? inf : std::abs(1 / vy);
  double tx = vx == 0 ? inf : ((vx > 0 ? x + 1.0 : x) - ox) / vx;
  double ty = vy == 0 ? inf : ((vy > 0 ? y + 1.0 : y) - oy) / vy;
  double distance = 0;
  for (unsigned count = 0; count < 512 && distance <= limit; ++count) {
    // Player/tool range is measured to tile centers, strictly inside radius.
    if (std::hypot(x + .5 - ox, y + .5 - oy) < range && occupied(x, y))
      return {true, x, y};
    if (tx < ty) { distance = tx; tx += stepX; x += sx; }
    else if (ty < tx) { distance = ty; ty += stepY; y += sy; }
    else { distance = tx; tx += stepX; ty += stepY; x += sx; y += sy; }
  }
  return {};
}
}
