#include "StarTouchLayout.hpp"
#include "StarTouchInput.hpp"
#include <cstdlib>
#include <iostream>
#include <new>

static unsigned allocations = 0;
void* operator new(std::size_t size) { ++allocations; if (auto p=std::malloc(size ? size : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
using namespace Star::TouchLayout;
int main(int argc, char**) {
  unsigned checks=0;
  Metrics current{}; const char* currentId="";
  auto test=[&](bool ok,int line) { if(!ok) { std::cerr<<"FAIL layout check "<<checks<<" line "<<line<<" "<<currentId<<" "<<current.width<<"x"<<current.height<<" density "<<current.density<<" scale "<<current.scale<<"\n"; std::exit(1); } ++checks; };
#define check(value) test((value),__LINE__)
  const char* ids[]={"joystick","aimJoystick","jump","interact","pause","rightHand","tech","targetLock","crouch","more","miningMode","singleBlock"};
  if(argc>1) {
    Metrics m{874,393,1,1}; Hud hud; hud.add({0,0,1,.16f}); hud.add({.93f,0,1,.32f});
    std::array<Circle,12> placed{};
    for(size_t i=0;i<placed.size();++i) {
      auto c=avoid(placement(ids[i],m,i<2?128:i==2?56:i==8?44:48),m,hud,placed.data(),i); placed[i]=c;
      std::cout<<ids[i]<<","<<c.x/m.width*100<<","<<c.y/m.height*100<<","<<c.radius*2<<"\n";
    }
    auto cells=panelCells(m,hud,false,placed[9],placed[0],placed[1]);
    for(size_t i=0;i<cells.count;++i) { auto c=cells.cells[i]; std::cout<<"panel"<<i<<","<<c.x/m.width*100<<","<<c.y/m.height*100<<","<<c.radius*2<<"\n"; }
    return 0;
  }
  unsigned baseline=allocations;
  for (float aspect : {16.f/9,20.f/9,2400.f/1080}) for(float density : {1.f,2.f,2.75f,3.f})
    for(float heightDp : {360.f,393.f,480.f,720.f}) for(float scale : {.5f,1.f,1.5f}) for(float notchDp : {0.f,24.f}) {
      Metrics m{heightDp*aspect*density-notchDp*density,heightDp*density,density,scale};
      current=m;
      Hud hud; hud.add({0,0,1,.16f}); hud.add({.93f,0,1,.32f});
      std::array<Circle,12> placed{};
      for(size_t i=0;i<placed.size();++i) {
        currentId=ids[i];
        float size=i<2?128:i==2?56:i==8?44:48;
        auto c=avoid(placement(ids[i],m,size),m,hud,placed.data(),i);
        check(c.visible);
        check(c.radius*2/m.dp()>=43.99f);
        check(!blocked(c,m,hud));
        check(c.x-c.radius>=7.99f*m.dp() && c.x+c.radius<=m.width-7.99f*m.dp());
        check(c.y-c.radius>=7.99f*m.dp() && c.y+c.radius<=m.height-7.99f*m.dp());
        check(!intersects(c,{m.width*.34f,m.height*.2f,m.width*.66f,m.height*.88f}));
        for(size_t j=0;j<i;++j) check(separated(c,placed[j],8*m.dp()));
        check(contains(c,c.x,c.y)); check(!contains(c,c.x+c.radius,c.y+c.radius));
        placed[i]=c;
      }
      for(bool left : {false,true}) {
        Circle menu=placed[9]; if(left) menu.x=m.width-menu.x;
        auto cells=panelCells(m,hud,left,menu,placed[0],placed[1]);
        check(cells.count>0);
        for(size_t i=0;i<cells.count;++i) {
          auto c=cells.cells[i];
          check(!blocked(c,m,hud));
          check(separated(c,menu,8*m.dp())); check(separated(c,placed[0],8*m.dp())); check(separated(c,placed[1],8*m.dp()));
          for(size_t j=0;j<i;++j) check(separated(c,cells.cells[j],8*m.dp()));
        }
      }
      auto snapped=snap(117*m.dp(),m.dp()); check(std::abs(snapped-120*m.dp())<.01f);
    }
  check(allocations==baseline);
  char label[32];
  auto measure=[](char const* text) { return (float)std::strlen(text); };
  fitLabel(label,sizeof(label),"A very long custom shortcut",10,measure);
  check(std::string_view(label)=="A very ...");
  fitLabel(label,sizeof(label),u8"Próximo",8,measure); check(std::string_view(label)==u8"Próximo");
  fitLabel(label,sizeof(label),u8"éééééééééééééééééééé",8,measure);
  check(std::string_view(label)==u8"éé...");
  for(float scale : {.25f,.5f,1.f}) for(bool full : {false,true}) {
    Viewport touch{2320,1040,60,20}, game=full ? Viewport{2400,1080,0,0} : touch;
    for(auto point : {std::array<float,2>{0,0},std::array<float,2>{100,200},std::array<float,2>{2320,1040}}) {
      auto input=inputPoint(point,touch,game,game.width*scale,game.height*scale);
      auto back=screenPoint(input,touch,game,game.width*scale,game.height*scale);
      check(std::abs(back[0]-point[0])<.01f && std::abs(back[1]-point[1])<.01f);
      check(input[0]>=0 && input[0]<=game.width*scale && input[1]>=0 && input[1]<=game.height*scale);
    }
  }
  Star::TouchInput::AimAttack attack;
  check(attack.update(Star::TouchInput::AttackMode::Hold,false));
  check(attack.update(Star::TouchInput::AttackMode::Hold,true));
  check(!attack.release(Star::TouchInput::AttackMode::Hold,true));
  check(!attack.held);
  Drawer drawer;
  drawer.next(3,0); check(drawer.open && drawer.page==0);
  drawer.tick(4,true); check(drawer.open);
  drawer.tick(4,false); check(!drawer.open);
  drawer.next(3,5); drawer.next(3,6); check(drawer.page==1);
  drawer.next(3,7); check(drawer.page==2);
  drawer.next(3,8); check(!drawer.open);
  drawer.next(3,9); drawer.close(); check(!drawer.open && drawer.page==0);
  // Contact identities remain independent; releasing one never cancels others.
  Star::TouchInput::Contact move,aim,jump,use;
  check(move.claim(1)&&aim.claim(2)&&jump.claim(3)&&use.claim(4));
  jump.clear(); check(move.owns(1)&&aim.owns(2)&&use.owns(4));
  check(!move.claim(5));
  std::cout<<"Passed "<<checks<<" touch layout checks; geometry allocations: "<<allocations-baseline<<"\n";
}
