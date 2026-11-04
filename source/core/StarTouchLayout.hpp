#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string_view>

// Geometry in safe-area pixels; dp is converted exactly once at the boundary.
// No SDL, renderer, game assets, heap storage or platform-specific APIs.
namespace Star::TouchLayout {
template <typename Measure>
void fitLabel(char* out, size_t capacity, std::string_view text, float width, Measure measure) {
  if (!capacity) return;
  size_t end=std::min(text.size(),capacity-1);
  auto boundary=[&] { while(end && end<text.size() && (static_cast<unsigned char>(text[end])&0xc0)==0x80) --end; };
  boundary();
  std::memcpy(out,text.data(),end); out[end]=0;
  if(end==text.size() && measure(out)<=width) return;
  if(capacity<4) { out[0]=0; return; }
  end=std::min(end,capacity-4); boundary();
  for(;;) {
    std::memcpy(out,text.data(),end); std::memcpy(out+end,"...",4);
    if(measure(out)<=width || !end) return;
    --end; boundary();
  }
}
struct Circle { float x = 0, y = 0, radius = 0; bool visible = true; };
struct Rect { float left = 0, top = 0, right = 0, bottom = 0; };
inline bool compactPrimary(std::string_view id, bool moreAction, bool manualPrimary) {
  return moreAction || id=="jump" || id=="interact" || id=="pause" || (id=="leftHand" && manualPrimary);
}
inline bool inPanel(std::string_view id, bool explicitlyGrouped, bool button, bool moreAction, bool compact, bool manualPrimary) {
  if(moreAction || (compact && manualPrimary && id=="leftHand")) return false;
  return explicitlyGrouped || (compact && button && !compactPrimary(id,moreAction,manualPrimary));
}
// Native inventory widget units, bottom-up. Extend sideways, never above the
// original pane: the space above it belongs to the hotbar/drop destinations.
struct InventoryToolsLayout {
  float nativeWidth, nativeHeight;
  float width() const { return nativeWidth+76; }
  Rect row(size_t index) const {
    float step=std::min(20.f,(nativeHeight-16)/8);
    float top=nativeHeight-8-index*step;
    return {nativeWidth+6,top-step+2,nativeWidth+70,top};
  }
};
struct Viewport { float width=1, height=1, left=0, top=0; };
inline std::array<float,2> inputPoint(std::array<float,2> local, Viewport touch, Viewport game, float width, float height) {
  return {(local[0]+touch.left-game.left)*width/std::max(1.f,game.width),
      height-(local[1]+touch.top-game.top)*height/std::max(1.f,game.height)};
}
inline std::array<float,2> screenPoint(std::array<float,2> input, Viewport touch, Viewport game, float width, float height) {
  return {input[0]*game.width/std::max(1.f,width)+game.left-touch.left,
      (height-input[1])*game.height/std::max(1.f,height)+game.top-touch.top};
}
struct Hud {
  std::array<Rect, 12> regions{}; // normalized, top-left origin
  size_t count = 0;
  bool locked = false;
  void add(Rect rect) { if (count < regions.size()) regions[count++] = rect; }
};
struct Metrics {
  float width, height, density = 1, scale = 1;
  float dp() const { return std::clamp(density, .5f, 4.f); }
  float fittedScale() const {
    // Preserve the center and the 44dp target floor even on compact displays.
    float fit = std::min(width / dp() * .33f / 226.f, height / dp() / 310.f);
    return std::clamp(std::min(scale, fit), .5f, 1.5f);
  }
};
inline bool contains(Circle c, float x, float y) {
  return c.visible && std::hypot(x - c.x, y - c.y) <= c.radius;
}
inline bool intersects(Circle c, Rect r, float gap = 0) {
  return std::hypot(c.x - std::clamp(c.x, r.left, r.right),
      c.y - std::clamp(c.y, r.top, r.bottom)) < c.radius + gap;
}
inline bool separated(Circle a, Circle b, float gap) {
  return !a.visible || !b.visible || std::hypot(a.x - b.x, a.y - b.y) + .01f >= a.radius + b.radius + gap;
}
inline float radius(Metrics m, float diameterDp, float elementScale = 1) {
  return std::max(44.f, diameterDp * m.fittedScale() * std::clamp(elementScale, .45f, 2.4f)) * m.dp() * .5f;
}
inline Circle clamp(Circle c, Metrics m) {
  float margin = 8 * m.dp();
  c.x = std::clamp(c.x, c.radius + margin, std::max(c.radius + margin, m.width - c.radius - margin));
  c.y = std::clamp(c.y, c.radius + margin, std::max(c.radius + margin, m.height - c.radius - margin));
  return c;
}
inline bool blocked(Circle c, Metrics m, Hud const& hud) {
  for (size_t i = 0; i < hud.count; ++i) {
    auto r = hud.regions[i];
    r.left *= m.width; r.right *= m.width; r.top *= m.height; r.bottom *= m.height;
    if (intersects(c, r, 8 * m.dp())) return true;
  }
  return false;
}
inline Circle placement(std::string_view id, Metrics m, float diameterDp, float elementScale = 1) {
  float d = m.dp(), s = m.fittedScale();
  float stick = 64 * s, button = std::max(22.f, 24 * s);
  float arc = std::max(110 * s, (2 * button + 8) / .517638f);
  float x = stick + 24, y = m.height / d - stick - 16;
  Circle c{x * d, y * d, radius(m, diameterDp, elementScale)};
  if (id == "aimJoystick") c.x = m.width - x * d;
  else if (id == "jump" || id == "crouch") {
    c.y -= (stick + std::max(28.f, 28 * s) + 8) * d;
    if (id == "crouch") c.x += (std::max(28.f, 28 * s) + std::max(22.f, 22 * s) + 8) * d;
  } else if (id == "interact" || id == "rightHand" || id == "tech" || id == "targetLock") {
    float angle = id == "interact" ? 180 : id == "rightHand" ? 150 : id == "tech" ? 120 : 90;
    angle *= .01745329252f;
    c.x = m.width - (x - arc * std::cos(angle)) * d;
    c.y -= arc * std::sin(angle) * d;
  } else if (id == "pause" || id == "more") {
    c.x = (button + 12) * d;
    if (id == "more") c.x = m.width - c.x;
    c.y = std::max(m.height * .25f, (button + 12) * d);
  } else if (id == "miningMode" || id == "singleBlock") {
    c.x=m.width*.27f;
    c.y=m.height*(id=="miningMode" ? .30f : .46f);
  } else if (id == "leftHand") {
    c.x=m.width-(x+arc)*d;
    c.y-=(2*button+8)*d;
  }
  return clamp(c, m);
}
inline float snap(float value, float density) { float step = 8 * density; return std::round(value / step) * step; }
inline Circle avoid(Circle c, Metrics m, Hud const& hud, Circle const* previous, size_t count) {
  auto free = [&](Circle candidate) {
    if (blocked(candidate, m, hud)) return false;
    for (size_t i = 0; i < count; ++i)
      if (!separated(candidate, previous[i], 8 * m.dp())) return false;
    return true;
  };
  c = clamp(c, m);
  if (free(c)) return c;
  // Keep the horizontal edge zone: never move a control over the character.
  float best = 1e30f;
  Circle result = c; result.visible = false;
  for (float y = c.radius + 8 * m.dp(); y + c.radius + 8 * m.dp() <= m.height; y += 8 * m.dp()) {
    Circle candidate{c.x, y, c.radius};
    float distance = std::abs(y - c.y);
    if (distance < best && free(candidate)) { result = candidate; best = distance; }
  }
  return result;
}
inline bool floatingZone(float x, float y, Metrics m) {
  return x > 8 * m.dp() && x < m.width * .33f && y > m.height * .48f && y < m.height - 8 * m.dp();
}
// Panel pages are bounded by the available edge space; no centered popup.
inline size_t panelCapacity(Metrics m) {
  return 2 * (size_t)std::clamp((int)(m.height * .30f / (52 * m.dp())), 1, 3);
}
inline Circle panelPlacement(size_t index, Metrics m, bool left) {
  float d = m.dp();
  float x = (30 + (index % 2) * 52) * d;
  return {left ? x : m.width - x, m.height * .25f + (56 + (index / 2) * 52) * d, 22 * d};
}
struct PanelCells { std::array<Circle, 6> cells{}; size_t count = 0; };
inline PanelCells panelCells(Metrics m, Hud const& hud, bool left, Circle menu, Circle movement, Circle aim) {
  PanelCells result;
  float d = m.dp();
  for (float y = 30*d; y + 30*d <= m.height && result.count < result.cells.size(); y += 52*d) {
    for (float x : {30.f,82.f,134.f}) {
      Circle c{left ? x*d : m.width-x*d,y,22*d};
      if (blocked(c,m,hud) || !separated(c,menu,8*d) || !separated(c,movement,8*d) || !separated(c,aim,8*d)) continue;
      result.cells[result.count++] = c;
      if (result.count == result.cells.size()) break;
    }
  }
  return result;
}
struct Drawer {
  bool open = false;
  size_t page = 0;
  double lastUse = 0;
  void next(size_t pages, double now) {
    if (!open) { open = true; page = 0; }
    else if (++page >= std::max<size_t>(1, pages)) { open = false; page = 0; }
    lastUse = now;
  }
  void tick(double now, bool held) { if (open && !held && now - lastUse >= 3) open = false; }
  void close() { open = false; page = 0; }
};
}
