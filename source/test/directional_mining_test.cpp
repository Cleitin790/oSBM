#include "StarDirectionalMining.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <utility>
using namespace Star::DirectionalMining;
int main() {
  unsigned checks = 0;
  auto check = [&](bool value) { ++checks; if (!value) { std::cerr << "Failed " << checks << "\n"; std::exit(1); } };
  std::set<std::pair<int,int>> blocks{{2,0},{3,0},{4,0},{0,3},{-3,0},{2,2}};
  auto occupied = [&](int x, int y) { return blocks.count({x,y}) != 0; };
  for (float strength : {.01f,.2f,.5f,1.f,100.f}) {
    auto h = firstTile(.5f,.5f,strength,0,6,occupied); check(h.found && h.x == 2 && h.y == 0);
  }
  blocks.erase({2,0}); auto h = firstTile(.5f,.5f,1,0,6,occupied); check(h.found && h.x == 3);
  // Retain the first occupied/protected block rather than skipping to the next.
  h = firstTile(.5f,.5f,1,0,6,occupied); check(h.found && h.x == 3);
  h = firstTile(.5f,.5f,0,1,6,occupied); check(h.found && h.y == 3);
  h = firstTile(.5f,.5f,-1,0,6,occupied); check(h.found && h.x == -3);
  h = firstTile(.5f,.5f,1,1,6,occupied); check(h.found && h.x == 2 && h.y == 2);
  check(!firstTile(.5f,.5f,1,0,2.5f,occupied).found);
  check(!firstTile(.5f,.5f,0,0,6,occupied).found);
  check(!firstTile(.5f,.5f,1,0,0,occupied).found);
  check(!firstTile(.5f,.5f,std::numeric_limits<float>::quiet_NaN(),0,6,occupied).found);
  // All octants, arbitrary sub-tile origins, bounded cost and monotonic progress.
  for (int angle = 0; angle < 360; ++angle) {
    unsigned calls = 0;
    auto empty = [&](int, int) { ++calls; return false; };
    double rad = angle * 3.141592653589793 / 180;
    check(!firstTile(-.1f,.9f,std::cos(rad),std::sin(rad),100000,empty).found);
    check(calls <= 512);
  }
  std::cout << "Passed " << checks << " directional mining checks\n";
}
