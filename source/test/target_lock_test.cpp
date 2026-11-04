#include "StarTargetLock.hpp"
#include <cstdio>
#include <cstdlib>

static unsigned checks = 0;
static void check(bool ok) { ++checks; if (!ok) std::exit(1); }
int main() {
  using namespace Star::TargetLock;
  check(score(30,1,true,true) < score(1,1,false,true));
  check(score(30,1,true,false) > score(1,1,false,false));
  check(score(10,1,true,true) < score(20,1,true,true));
  check(score(10,1,false,true) < score(10,.6f,false,true));
  check(std::isinf(score(1,.49f,true,true)));
  check(std::isfinite(score(1,.5f,true,true)));
  check(std::isinf(score(0,1,true,true)));
  check(std::isinf(score(NAN,1,true,true)));
  check(std::isinf(score(1,NAN,true,true)));
  Candidate previous;
  for (int step=0; step<9; ++step) {
    Chooser chooser; chooser.previous=previous;
    chooser.consider(3,30); chooser.consider(1,10); chooser.consider(2,20);
    auto result=chooser.result();
    check(result.valid && result.id==(step%3+1)*10);
    previous=result;
  }
  Chooser tie; tie.consider(1,20); tie.consider(1,10);
  check(tie.result().id==10);
  check(score(30,1,false,false)<score(1,.8f,false,false));
  for (unsigned mask=0;mask<128;++mask) {
    bool valid=retain(mask&1,mask&2,mask&4,mask&8,mask&16,mask&32,mask&64);
    check(valid == (mask==63));
  }
  // Automatic mode acquires enemies behind the player, including at overlap;
  // it favors proximity, with optional boss priority and deterministic ties.
  check(std::isfinite(automaticScore(2,-1,false,true)));
  check(std::isfinite(automaticScore(0,0,false,true)));
  check(automaticScore(2,-1,false,false)<automaticScore(10,1,false,false));
  check(automaticScore(80,-1,true,true)<automaticScore(1,1,false,true));
  check(automaticScore(80,-1,true,false)>automaticScore(1,1,false,false));
  check(std::isinf(automaticScore(NAN,1,true,true)));
  check(std::isinf(automaticScore(2,NAN,true,true)));
  check(std::isinf(automaticScore(-1,1,true,true)));
  Chooser automatic;
  automatic.consider(automaticScore(2,-1,false,true),30);
  automatic.consider(automaticScore(3,1,false,true),10);
  check(automatic.result().id==30);
  automatic.consider(automaticScore(80,-1,true,true),50);
  check(automatic.result().id==50);
  check(weaponEligible(true,false,true,false,false)); // weapon plus shield
  check(weaponEligible(true,false,true,true,false));
  check(weaponEligible(true,true,true,false,true)); // two-handed weapon alt fire
  check(!weaponEligible(true,false,true,false,true)); // use nonweapon hand freely
  check(!weaponEligible(false,true,true,false,false)); // tool plus offhand weapon
  check(!weaponEligible(false,true,true,true,true)); // mining must stay free
  check(weaponEligible(false,true,true,false,true)); // deliberately fire offhand weapon
  check(weaponEligible(false,true,false,false,false)); // only an offhand weapon
  check(!weaponEligible(false,false,true,true,false));
  AutomaticSearch search;
  check(search.due(0,true,false,false));
  check(!search.due(.1,true,false,false));
  check(search.due(.25,true,false,false));
  check(!search.due(.3,true,true,false)); // holding a boss does not query
  check(!search.due(.4,true,false,false)); // losing it cannot bypass the cap
  check(search.due(.5,true,false,false));
  check(!search.due(.75,true,true,true)); // boss-promotion cadence is slower
  check(search.due(1.25,true,true,true));
  check(search.due(1.5,true,false,false)); // replacement need not wait .75s
  check(!search.due(2,false,false,false)); // menus, focus, tools or paused toggle
  check(search.due(2.01,true,false,false)); // automatic resumption
  check(!search.due(NAN,true,false,false));
  search.reset();
  unsigned scans=0;
  for(int i=0;i<=1000;++i) scans+=search.due(i*.01,true,false,false);
  check(scans<=41 && scans>=36);
  search.reset(); scans=0;
  for(int i=0;i<=1000;++i) scans+=search.due(i*.01,true,true,true);
  check(scans<=14 && scans>=12);
  search.reset();
  for(int i=0;i<=1000;++i) check(!search.due(i*.01,true,true,false));
  std::printf("Passed %u target lock checks\n",checks);
}
