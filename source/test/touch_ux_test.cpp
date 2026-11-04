#include "StarTouchInput.hpp"
#include <cstdlib>
#include <iostream>
using namespace Star::TouchInput;
int main() {
  unsigned checks = 0;
  auto check = [&](bool ok) { ++checks; if (!ok) { std::cerr << "Failed " << checks << "\n"; std::exit(1); } };
  for (auto mode : {ItemAction::Move, ItemAction::Inspect, ItemAction::One, ItemAction::Half}) {
    check(itemActionAfterTap(mode) == (mode == ItemAction::Half ? ItemAction::Move : mode));
    check(itemDragAllowed(mode) == (mode == ItemAction::Move));
    check(itemInspect(mode, true));
    check(itemInspect(mode, false) == (mode == ItemAction::Inspect));
    check(itemSplit(mode) == (mode == ItemAction::One || mode == ItemAction::Half));
    Gesture gesture; gesture.down(0,0,0);
    check(gesture.longPress(.5));
    gesture.state = Gesture::State::Consumed;
    check(!gesture.tap()); check(!gesture.move(100,100,10));
  }
  using Edge = HeldUse::Edge;
  for (int frames : {0,1,2,30,60,240}) {
    HeldUse use; check(use.update(true) == Edge::None);
    use.press();
    for (int i=0; i<frames; ++i) check(use.update(true) == (i == 0 ? Edge::Press : Edge::None));
    check(use.cancel() == (frames ? Edge::Release : Edge::None));
    check(use.cancel() == Edge::None);
    check(use.update(true) == Edge::None);
    use.press(); check(use.update(false) == Edge::None); // menu before first frame
    check(use.update(true) == Edge::None); // no resuming without new down
    use.press(); check(use.update(true) == Edge::Press);
    check(use.update(false) == Edge::Release); // focus/menu/item changed
    check(use.update(true) == Edge::None);
  }
  std::cout << "Passed " << checks << " touch UX checks\n";
}
