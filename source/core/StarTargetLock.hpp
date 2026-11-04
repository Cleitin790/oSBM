#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace Star::TargetLock {
// Manual cycling retains its cone. Automatic acquisition covers the screen.
inline float score(float distance, float alignment, bool boss, bool preferBoss) {
  if (!std::isfinite(distance) || !std::isfinite(alignment) || distance <= 0 || alignment < 0.5f)
    return std::numeric_limits<float>::infinity();
  return distance * 0.1f + (1 - alignment) * 100 - (boss && preferBoss ? 1000 : 0);
}
struct Candidate { float rank = std::numeric_limits<float>::infinity(); int64_t id = 0; bool valid = false; };
inline float automaticScore(float distance, float alignment, bool boss, bool preferBoss) {
  if(!std::isfinite(distance) || !std::isfinite(alignment) || distance<0)
    return std::numeric_limits<float>::infinity();
  alignment=std::max(-1.f,std::min(1.f,alignment));
  return distance+(1-alignment)*.25f-(boss && preferBoss ? 1000 : 0);
}
inline bool weaponEligible(bool primaryWeapon, bool secondaryWeapon, bool primaryOccupied, bool primaryHeld, bool secondaryHeld) {
  if(primaryHeld && !primaryWeapon) return false;
  if(secondaryHeld && !secondaryWeapon) return false;
  return primaryWeapon || (secondaryWeapon && (secondaryHeld || !primaryOccupied));
}
struct AutomaticSearch {
  double lastAt=-1e30;
  void reset() { lastAt=-1e30; }
  bool due(double now, bool eligible, bool hasTarget, bool seekBoss) {
    if(!eligible) { reset(); return false; }
    if(hasTarget && !seekBoss) return false;
    if(!std::isfinite(now) || now<lastAt+(hasTarget ? .75 : .25)) return false;
    lastAt=now;
    return true;
  }
};
inline bool before(Candidate a, Candidate b) {
  return a.rank < b.rank || (a.rank == b.rank && a.id < b.id);
}
struct Chooser {
  Candidate previous, first, following;
  void consider(float rank, int64_t id) {
    if (!std::isfinite(rank) || (previous.valid && id == previous.id)) return;
    Candidate candidate{rank,id,true};
    if (!first.valid || before(candidate,first)) first=candidate;
    if (previous.valid && before(previous,candidate) && (!following.valid || before(candidate,following))) following=candidate;
  }
  Candidate result() const { return following.valid ? following : first; }
};
inline bool retain(bool exists, bool alive, bool hostile, bool visible, bool inRange, bool weapon, bool blocked) {
  return exists && alive && hostile && visible && inRange && weapon && !blocked;
}
}
