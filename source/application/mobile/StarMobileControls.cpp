#include "StarApplication.hpp"
#include "StarApplicationController.hpp"
#include "StarByteArray.hpp"
#include "StarFile.hpp"
#include "StarJsonExtra.hpp"
#include "StarLogging.hpp"
#include "StarMemoryUsage.hpp"
#include "StarRenderer_gles.hpp"
#include "StarSignalHandler.hpp"
#include "StarTickRateMonitor.hpp"
#include "StarTime.hpp"
#include "StarGamepadInput.hpp"
#include "StarTouchInput.hpp"
#include "StarPlatformServices_mobile.hpp"
#if STAR_SYSTEM_ANDROID
#include "mobile/android/StarAndroidFileAccessBridge.hpp"
#elif STAR_SYSTEM_IOS
#include "mobile/ios/StarIosFileAccessBridge.hpp"
extern "C" void StarIosBridge_setSdlWindow(void* window);
extern "C" void StarIosBridge_getSafeAreaInsets(float* top, float* left, float* bottom, float* right);
extern "C" int  StarIosBridge_getInterfaceOrientation();
#endif

#include "SDL3/SDL.h"
#if defined(STAR_SYSTEM_ANDROID) || defined(STAR_SYSTEM_IOS) || defined(STAR_SYSTEM_SWITCH)
// See StarMobilePlatform.cpp: on a desktop launcher build the GL symbols come
// from GLEW (via StarRenderer_gles.hpp), so SDL's GLES2 header must not be
// pulled in here or its extern decls clash with GLEW's function-pointer macros.
#include "SDL3/SDL_opengles2.h"
#endif

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#ifdef STAR_SYSTEM_ANDROID
#include <android/log.h>
#include <jni.h>
#endif

#include "mobile/StarMobileControls.hpp"
#include "mobile/StarMobileLauncherSupport.hpp"

namespace Star {

String keysName(List<Key> const& keys) {
  StringList keyNames;
  for (auto key : keys)
    keyNames.append(KeyNames.getRight(key));
  return keyNames.join("+");
}

String actionName(MobileTouchAction const& action) {
  switch (action.kind) {
    case MobileTouchActionKind::Key:
      return KeyNames.getRight(action.key);
    case MobileTouchActionKind::KeyMacro:
      return action.keys.empty() ? launcherTextStatic("touchAction.macro", "Macro") : keysName(action.keys);
    case MobileTouchActionKind::MouseButton:
      return action.mouseButton == MouseButton::Left ? launcherTextStatic("touchAction.leftMouse", "Left Mouse")
          : action.mouseButton == MouseButton::Right ? launcherTextStatic("touchAction.rightMouse", "Right Mouse")
          : MouseButtonNames.getRight(action.mouseButton);
    case MobileTouchActionKind::MouseWheelUp:
      return launcherTextStatic("touchAction.scrollUp", "Scroll Up");
    case MobileTouchActionKind::MouseWheelDown:
      return launcherTextStatic("touchAction.scrollDown", "Scroll Down");
    case MobileTouchActionKind::MorePanel: return launcherTextStatic("touchElement.more", "More");
    case MobileTouchActionKind::QuickSlot: {
      String label = strf("{} {}", launcherTextStatic("shortcut.bar", "Bar"), action.quickSlot + 1);
      if (action.quickUse) label += strf(" / {}", launcherTextStatic("shortcut.holdSuffix", "hold to use"));
      return label;
    }
    case MobileTouchActionKind::MiningModeToggle: return launcherTextStatic("touchAction.miningMode", "Free / directional mining");
    case MobileTouchActionKind::TargetLockToggle: return launcherTextStatic("touchAction.targetLock", "Pause / resume automatic target lock");
    case MobileTouchActionKind::TargetLockNext: return launcherTextStatic("touchAction.targetNext", "Next target");
    case MobileTouchActionKind::TouchHandSwap:
      return launcherTextStatic("touchAction.swapAimHand", "Swap aim hand");
    case MobileTouchActionKind::GyroToggle:
      return launcherTextStatic("touchAction.gyroToggle", "Gyro Toggle");
    case MobileTouchActionKind::GamepadAimModeToggle:
      return launcherTextStatic("gamepadAction.aimModeToggle", "Toggle Aim Mode");
    case MobileTouchActionKind::ActionWheel:
      return launcherTextStatic("gamepadAction.actionWheel", "Action Wheel");
    case MobileTouchActionKind::InventoryWheel:
      return launcherTextStatic("gamepadAction.inventoryWheel", "Inventory Wheel");
    case MobileTouchActionKind::UiNavigation:
      return action.uiNavigationDirection == UiNavigationDirection::Up ? launcherTextStatic("gamepadAction.selectionUp", "Selection Up")
          : action.uiNavigationDirection == UiNavigationDirection::Down ? launcherTextStatic("gamepadAction.selectionDown", "Selection Down")
          : action.uiNavigationDirection == UiNavigationDirection::Left ? launcherTextStatic("gamepadAction.selectionLeft", "Selection Left")
          : launcherTextStatic("gamepadAction.selectionRight", "Selection Right");
    default:
      return launcherTextStatic("touchAction.none", "None");
  }
}

MobileTouchAction keyAction(Key key) {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::Key;
  action.key = key;
  return action;
}

MobileTouchAction macroAction(List<Key> keys, bool sequential) {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::KeyMacro;
  action.keys = std::move(keys);
  action.macroSequential = sequential;
  return action;
}

MobileTouchAction mouseAction(MouseButton button) {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::MouseButton;
  action.mouseButton = button;
  return action;
}

MobileTouchAction wheelAction(bool up) {
  MobileTouchAction action;
  action.kind = up ? MobileTouchActionKind::MouseWheelUp : MobileTouchActionKind::MouseWheelDown;
  return action;
}

MobileTouchAction quickSlotAction(int slot, bool use) {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::QuickSlot;
  action.quickSlot = std::clamp(slot, 0, 9);
  action.quickUse = use;
  return action;
}

MobileTouchAction miningModeAction() {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::MiningModeToggle;
  return action;
}

MobileTouchAction targetLockAction(bool next) {
  MobileTouchAction action;
  action.kind = next ? MobileTouchActionKind::TargetLockNext : MobileTouchActionKind::TargetLockToggle;
  return action;
}

void loadTouchAimOptions(MobileTouchConfig& config, Json const& json) {
  config.lockOnEnabled = json.queryBool("touch.lockOnEnabled", false);
  config.lockOnPreferBoss = json.queryBool("touch.lockOnPreferBoss", true);
  config.lockOnRange = std::clamp(json.queryFloat("touch.lockOnRange", 40), 8.0f, 80.0f);
  config.aimAttackMode = (TouchInput::AttackMode)std::clamp(json.queryInt("touch.aimAttackMode", json.optQueryArray("touch.elements") ? 0 : 3), (int64_t)0, (int64_t)3);
  config.aimDeadzone = std::clamp(json.queryFloat("touch.aimDeadzone", 0.15f), 0.0f, 0.60f);
  config.floatingMovement = json.queryBool("touch.floatingMovement", true);
  config.panelLeft = json.queryBool("touch.panelLeft", false);
  config.snapToGrid = json.queryBool("touch.snapToGrid", true);
  config.compactHud = json.queryBool("touch.compactHud", true);
  config.aimCurve = std::clamp(json.queryFloat("touch.aimCurve", 1.4f), 0.5f, 3.0f);
}

MobileTouchAction touchHandSwapAction() {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::TouchHandSwap;
  return action;
}

MobileTouchAction gyroToggleAction() {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::GyroToggle;
  return action;
}

MobileTouchAction gamepadAimModeToggleAction() {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::GamepadAimModeToggle;
  return action;
}

MobileTouchAction actionWheelAction() {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::ActionWheel;
  return action;
}

MobileTouchAction inventoryWheelAction() {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::InventoryWheel;
  return action;
}

MobileTouchAction uiNavigationAction(UiNavigationDirection direction) {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::UiNavigation;
  action.uiNavigationDirection = direction;
  return action;
}

MobileTouchAction noneAction() {
  MobileTouchAction action;
  action.kind = MobileTouchActionKind::None;
  return action;
}

String pressModeName(MobileTouchPressMode mode) {
  switch (mode) {
    case MobileTouchPressMode::SinglePress:
      return "single";
    case MobileTouchPressMode::Repeat:
      return "repeat";
    case MobileTouchPressMode::Toggle:
      return "toggle";
    default:
      return "hold";
  }
}

MobileTouchPressMode pressModeFromName(String const& name, MobileTouchPressMode def) {
  if (name.equals("single", String::CaseInsensitive) || name.equals("singlePress", String::CaseInsensitive))
    return MobileTouchPressMode::SinglePress;
  if (name.equals("repeat", String::CaseInsensitive) || name.equals("rapidFire", String::CaseInsensitive))
    return MobileTouchPressMode::Repeat;
  if (name.equals("toggle", String::CaseInsensitive))
    return MobileTouchPressMode::Toggle;
  if (name.equals("hold", String::CaseInsensitive))
    return MobileTouchPressMode::Hold;
  return def;
}

String perfCounterModeName(PerformanceCounterMode mode) {
  switch (mode) {
    case PerformanceCounterMode::Detailed:
      return "detailed";
    case PerformanceCounterMode::Memory:
      return "memory";
    default:
      return "fps";
  }
}

PerformanceCounterMode perfCounterModeFromName(String const& name, PerformanceCounterMode def) {
  if (name.equals("detailed", String::CaseInsensitive))
    return PerformanceCounterMode::Detailed;
  if (name.equals("memory", String::CaseInsensitive))
    return PerformanceCounterMode::Memory;
  if (name.equals("fps", String::CaseInsensitive))
    return PerformanceCounterMode::Fps;
  return def;
}

std::vector<MobileTouchElement> defaultTouchElements() {
  std::vector<MobileTouchElement> elements = {
    {"joystick", launcherTextStatic("touchElement.joystick", "Joystick"), MobileTouchElementKind::Joystick, true, {0.14f, 0.78f}, 1.15f, keyAction(Key::Space), {}, {}, {}, {}},
    {"aimJoystick", launcherTextStatic("touchElement.aimJoystick", "Aim"), MobileTouchElementKind::AimJoystick, true, {0.66f, 0.78f}, 1.15f, noneAction(), {}, {}, {}, {}},
    {"leftHand", launcherTextStatic("touchElement.leftHand", "L"), MobileTouchElementKind::Button, true, {0.30f, 0.16f}, 0.92f, mouseAction(MouseButton::Left), {}, {}, {}, {}},
    {"rightHand", launcherTextStatic("touchElement.rightHand", "R"), MobileTouchElementKind::Button, true, {0.64f, 0.16f}, 0.92f, mouseAction(MouseButton::Right), {}, {}, {}, {}},
    {"jump", launcherTextStatic("touchElement.jump", "J"), MobileTouchElementKind::Button, true, {0.88f, 0.78f}, 1.15f, keyAction(Key::Space), {}, {}, {}, {}},
    {"interact", launcherTextStatic("touchElement.interact", "E"), MobileTouchElementKind::Button, true, {0.77f, 0.66f}, 1.00f, keyAction(Key::E), {}, {}, {}, {}},
    {"pause", launcherTextStatic("touchElement.pause", "ESC"), MobileTouchElementKind::Button, true, {0.10f, 0.15f}, 0.96f, keyAction(Key::Escape), {}, {}, {}, {}},
    {"chat", launcherTextStatic("touchElement.chat", "T"), MobileTouchElementKind::Button, true, {0.83f, 0.90f}, 0.72f, keyAction(Key::Return), {}, {}, {}, {}},
    {"tech", launcherTextStatic("touchElement.tech", "F"), MobileTouchElementKind::Button, true, {0.92f, 0.58f}, 0.86f, keyAction(Key::F), {}, {}, {}, {}},
    {"inventoryWheel", launcherTextStatic("touchElement.inventoryWheel", "Inv"), MobileTouchElementKind::Button, true, {0.94f, 0.42f}, 0.82f, inventoryWheelAction(), {}, {}, {}, {}, MobileTouchPressMode::Hold},
    {"actionWheel", launcherTextStatic("touchElement.actionWheel", "Menu"), MobileTouchElementKind::Button, true, {0.94f, 0.26f}, 0.82f, actionWheelAction(), {}, {}, {}, {}, MobileTouchPressMode::Hold},
    {"shift", launcherTextStatic("touchElement.shift", "Shift"), MobileTouchElementKind::Button, false, {0.66f, 0.88f}, 0.82f, keyAction(Key::LShift), {}, {}, {}, {}},
    {"targetLock", launcherTextStatic("touchElement.targetLock", "Auto"), MobileTouchElementKind::Button, true, {0.54f, 0.45f}, 0.72f, targetLockAction(), {}, {}, {}, {}, MobileTouchPressMode::SinglePress},
    {"targetNext", launcherTextStatic("touchElement.targetNext", "Next"), MobileTouchElementKind::Button, true, {0.62f, 0.45f}, 0.72f, targetLockAction(true), {}, {}, {}, {}, MobileTouchPressMode::SinglePress},
    {"quick1", "Q1", MobileTouchElementKind::Button, false, {0.50f, 0.32f}, .80f, quickSlotAction(0), {}, {}, {}, {}, MobileTouchPressMode::Hold},
    {"quick2", "Q2", MobileTouchElementKind::Button, false, {0.58f, 0.32f}, .80f, quickSlotAction(1), {}, {}, {}, {}, MobileTouchPressMode::Hold},
    {"quick3", "Q3", MobileTouchElementKind::Button, false, {0.66f, 0.32f}, .80f, quickSlotAction(2), {}, {}, {}, {}, MobileTouchPressMode::Hold},
    {"miningMode", launcherTextStatic("touchElement.miningMode", "Mine"), MobileTouchElementKind::Button, true, {0.42f, 0.32f}, 0.72f, miningModeAction(), {}, {}, {}, {}, MobileTouchPressMode::SinglePress},
    {"singleBlock", launcherTextStatic("touchElement.singleBlock", "1 block"), MobileTouchElementKind::Button, true, {0.76f, 0.30f}, 0.72f, keyAction(Key::LShift), {}, {}, {}, {}, MobileTouchPressMode::Toggle},
    {"swapAimHand", launcherTextStatic("touchElement.swapAimHand", "L/R"), MobileTouchElementKind::Button, true, {0.80f, 0.46f}, 0.72f, touchHandSwapAction(), {}, {}, {}, {}, MobileTouchPressMode::SinglePress},
    {"ctrl", launcherTextStatic("touchElement.ctrl", "Ctrl"), MobileTouchElementKind::Button, false, {0.58f, 0.88f}, 0.82f, keyAction(Key::LCtrl), {}, {}, {}, {}},
    {"gyroToggle", launcherTextStatic("touchElement.gyro", "Gyro"), MobileTouchElementKind::Button, false, {0.74f, 0.88f}, 0.82f, gyroToggleAction(), {}, {}, {}, {}, MobileTouchPressMode::SinglePress},
    {"dpad", launcherTextStatic("touchElement.dpad", "D-PAD"), MobileTouchElementKind::DPad, false, {0.16f, 0.74f}, 1.05f, keyAction(Key::Space),
      keyAction(Key::W), keyAction(Key::S), keyAction(Key::A), keyAction(Key::D)},
    {"perfCounter", launcherTextStatic("touchElement.perfCounter", "Perf"), MobileTouchElementKind::PerformanceCounter, false, {0.02f, 0.98f}, 1.0f, noneAction(), {}, {}, {}, {}}
  };
  MobileTouchAction more; more.kind = MobileTouchActionKind::MorePanel;
  elements.push_back({"crouch", launcherTextStatic("touchElement.crouch", "Down"), MobileTouchElementKind::Button, true, {.20f,.60f}, 1, keyAction(Key::S), {}, {}, {}, {}});
  elements.push_back({"more", launcherTextStatic("touchElement.more", "More"), MobileTouchElementKind::Button, true, {.96f,.25f}, 1, more, {}, {}, {}, {}, MobileTouchPressMode::SinglePress});
  for (auto& e : elements) {
    if (e.kind == MobileTouchElementKind::PerformanceCounter) continue;
    bool stick = e.kind == MobileTouchElementKind::Joystick || e.kind == MobileTouchElementKind::AimJoystick || e.kind == MobileTouchElementKind::DPad;
    e.diameterDp = stick ? 128 : e.id == "jump" ? 56 : e.id == "crouch" ? 44 : 48;
    e.size = 1;
    e.adaptive = true;
    e.panel = !stick && e.id != "jump" && e.id != "crouch" && e.id != "interact"
        && e.id != "rightHand" && e.id != "tech" && e.id != "targetLock" && e.id != "pause" && e.id != "more";
    if (e.id == "leftHand" || e.id == "quick1" || e.id == "quick2" || e.id == "quick3") e.enabled = true;
    auto c = TouchLayout::placement(e.id.utf8(), {874,393,1,1}, e.diameterDp);
    e.position = {c.x / 874, c.y / 393};
  }
  return elements;
}

void applyTouchLayout(std::vector<MobileTouchElement>& elements, int preset) {
  auto incoming = defaultTouchElements();
  for (auto& e : incoming) {
    if (preset == 1) e.enabled = e.id == "joystick" || e.id == "aimJoystick" || e.id == "jump" || e.id == "interact" || e.id == "pause";
    if (preset == 2 && (e.id == "miningMode" || e.id == "singleBlock")) { e.enabled = true; e.panel = false; }
    if (preset == 3) e.enabled = e.id == "pause" || e.id == "more" || e.panel;
  }
  TouchInput::mergeLayout(elements, incoming, [](auto& old, auto const& value) {
    old.position = value.position; old.size = value.size; old.diameterDp = value.diameterDp;
    old.adaptive = value.adaptive; old.panel = value.panel; old.enabled = value.enabled;
  });
  for (auto& e : elements) {
    bool builtIn = std::any_of(incoming.begin(),incoming.end(),[&](auto const& value) { return value.id==e.id; });
    if(!builtIn && e.kind==MobileTouchElementKind::Button) { e.panel=true; e.diameterDp=std::max(44.f,e.diameterDp); }
  }
}

bool touchElementInPanel(MobileTouchElement const& e, MobileTouchConfig const& config) {
  bool manual=config.aimAttackMode==TouchInput::AttackMode::AimOnly || config.aimAttackMode==TouchInput::AttackMode::Release;
  return TouchLayout::inPanel(e.id.utf8(),e.panel,e.kind==MobileTouchElementKind::Button,
      e.action.kind==MobileTouchActionKind::MorePanel,config.compactHud,manual);
}

TouchLayout::Circle touchElementCircle(MobileTouchElement const& e, MobileTouchConfig const& config, TouchLayout::Metrics metrics) {
  if(config.compactHud && e.action.kind==MobileTouchActionKind::MorePanel)
    return TouchLayout::placement("more",metrics,48);
  if(config.compactHud && e.diameterDp<=0 && e.kind==MobileTouchElementKind::Button && !touchElementInPanel(e,config))
    return TouchLayout::placement(e.id.utf8(),metrics,e.id=="jump" ? 56 : 48,e.size);
  if (e.diameterDp > 0) {
    if (e.adaptive) return TouchLayout::placement(e.id.utf8(), metrics, e.diameterDp, e.size);
    return TouchLayout::clamp({e.position[0] * metrics.width, e.position[1] * metrics.height,
        TouchLayout::radius(metrics, e.diameterDp, e.size)}, metrics);
  }
  float radius = TouchInput::controlRadius(std::min(metrics.width, metrics.height), config.size) * std::clamp(e.size,.45f,2.4f);
  if (e.kind == MobileTouchElementKind::Button) radius *= .55f;
  return {e.position[0] * metrics.width, e.position[1] * metrics.height, radius};
}

std::vector<MobileGamepadBinding> defaultGamepadBindings() {
  return {
    {ControllerButton::A, "A", true, keyAction(Key::Space), MobileTouchPressMode::Hold},
    {ControllerButton::B, "B", true, keyAction(Key::F), MobileTouchPressMode::Hold},
    {ControllerButton::X, "X", true, keyAction(Key::E), MobileTouchPressMode::Hold},
    {ControllerButton::Y, "Y", true, keyAction(Key::I), MobileTouchPressMode::SinglePress},
    {ControllerButton::LeftShoulder, "LB", true, inventoryWheelAction(), MobileTouchPressMode::Hold},
    {ControllerButton::RightShoulder, "RB", true, actionWheelAction(), MobileTouchPressMode::Hold},
    {ControllerButton::TriggerLeft, "LT", true, mouseAction(MouseButton::Right), MobileTouchPressMode::Hold},
    {ControllerButton::TriggerRight, "RT", true, mouseAction(MouseButton::Left), MobileTouchPressMode::Hold},
    {ControllerButton::Back, "Back", true, keyAction(Key::I), MobileTouchPressMode::Hold},
    {ControllerButton::Start, "Start", true, keyAction(Key::Escape), MobileTouchPressMode::Hold},
    {ControllerButton::Guide, "Guide", false, noneAction(), MobileTouchPressMode::SinglePress},
    {ControllerButton::LeftStick, "L3", true, keyAction(Key::LCtrl), MobileTouchPressMode::Hold},
    {ControllerButton::RightStick, "R3", true, keyAction(Key::LShift), MobileTouchPressMode::Hold},
    {ControllerButton::DPadUp, "Up", true, uiNavigationAction(UiNavigationDirection::Up), MobileTouchPressMode::SinglePress},
    {ControllerButton::DPadDown, "Down", true, uiNavigationAction(UiNavigationDirection::Down), MobileTouchPressMode::SinglePress},
    {ControllerButton::DPadLeft, "Left", true, uiNavigationAction(UiNavigationDirection::Left), MobileTouchPressMode::SinglePress},
    {ControllerButton::DPadRight, "Right", true, uiNavigationAction(UiNavigationDirection::Right), MobileTouchPressMode::SinglePress},
    {ControllerButton::Misc1, "Share", false, noneAction(), MobileTouchPressMode::SinglePress},
    {ControllerButton::Paddle1, "P1", false, noneAction(), MobileTouchPressMode::SinglePress},
    {ControllerButton::Paddle2, "P2", false, noneAction(), MobileTouchPressMode::SinglePress},
    {ControllerButton::Paddle3, "P3", false, noneAction(), MobileTouchPressMode::SinglePress},
    {ControllerButton::Paddle4, "P4", false, noneAction(), MobileTouchPressMode::SinglePress},
    {ControllerButton::Touchpad, "Pad", false, noneAction(), MobileTouchPressMode::SinglePress}
  };
}

String elementKindName(MobileTouchElementKind kind) {
  switch (kind) {
    case MobileTouchElementKind::Joystick:
      return "joystick";
    case MobileTouchElementKind::AimJoystick:
      return "aimJoystick";
    case MobileTouchElementKind::DPad:
      return "dpad";
    case MobileTouchElementKind::PerformanceCounter:
      return "perfCounter";
    default:
      return "button";
  }
}

MobileTouchElementKind elementKindFromName(String const& name) {
  if (name.equals("joystick", String::CaseInsensitive))
    return MobileTouchElementKind::Joystick;
  if (name.equals("aimJoystick", String::CaseInsensitive) || name.equals("aim", String::CaseInsensitive))
    return MobileTouchElementKind::AimJoystick;
  if (name.equals("dpad", String::CaseInsensitive))
    return MobileTouchElementKind::DPad;
  if (name.equals("perfCounter", String::CaseInsensitive))
    return MobileTouchElementKind::PerformanceCounter;
  return MobileTouchElementKind::Button;
}

List<Key> keysFromTouchAction(MobileTouchAction const& action) {
  if (action.kind == MobileTouchActionKind::Key)
    return {action.key};
  if (action.kind == MobileTouchActionKind::KeyMacro)
    return action.keys;
  return {};
}

JsonArray jsonFromKeys(List<Key> const& keys) {
  JsonArray out;
  for (auto key : keys)
    out.append(KeyNames.getRight(key));
  return out;
}

List<Key> keysFromText(String const& text) {
  List<Key> keys;
  for (auto const& rawToken : text.replace("+", " ").replace(",", " ").replace(";", " ").splitWhitespace()) {
    auto token = rawToken.trim();
    if (token.empty())
      continue;
    if (auto key = KeyNames.maybeLeft(token))
      keys.append(*key);
  }
  return keys;
}

List<Key> keysFromJson(Json const& json) {
  List<Key> keys;
  if (auto array = json.optArray("keys")) {
    for (auto const& keyJson : *array) {
      if (auto key = KeyNames.maybeLeft(keyJson.toString()))
        keys.append(*key);
    }
  }
  return keys;
}

Json jsonFromTouchAction(MobileTouchAction const& action) {
  if (action.kind == MobileTouchActionKind::MorePanel) return JsonObject{{"type", "morePanel"}};
  if (action.kind == MobileTouchActionKind::KeyMacro)
    return JsonObject{{"type", "keys"}, {"keys", jsonFromKeys(action.keys)}, {"sequential", action.macroSequential}};
  if (action.kind == MobileTouchActionKind::MouseButton)
    return JsonObject{{"type", "mouse"}, {"button", MouseButtonNames.getRight(action.mouseButton)}};
  if (action.kind == MobileTouchActionKind::MouseWheelUp)
    return JsonObject{{"type", "wheel"}, {"direction", "up"}};
  if (action.kind == MobileTouchActionKind::MouseWheelDown)
    return JsonObject{{"type", "wheel"}, {"direction", "down"}};
  if (action.kind == MobileTouchActionKind::QuickSlot) return JsonObject{{"type", "quickSlot"}, {"slot", action.quickSlot}, {"use", action.quickUse}};
  if (action.kind == MobileTouchActionKind::MiningModeToggle) return JsonObject{{"type", "miningMode"}};
  if (action.kind == MobileTouchActionKind::TargetLockToggle) return JsonObject{{"type", "targetLock"}};
  if (action.kind == MobileTouchActionKind::TargetLockNext) return JsonObject{{"type", "targetNext"}};
  if (action.kind == MobileTouchActionKind::TouchHandSwap)
    return JsonObject{{"type", "touchHandSwap"}};
  if (action.kind == MobileTouchActionKind::GyroToggle)
    return JsonObject{{"type", "gyroToggle"}};
  if (action.kind == MobileTouchActionKind::GamepadAimModeToggle)
    return JsonObject{{"type", "gamepadAimModeToggle"}};
  if (action.kind == MobileTouchActionKind::ActionWheel)
    return JsonObject{{"type", "actionWheel"}};
  if (action.kind == MobileTouchActionKind::InventoryWheel)
    return JsonObject{{"type", "inventoryWheel"}};
  if (action.kind == MobileTouchActionKind::UiNavigation)
    return JsonObject{{"type", "uiNavigation"}, {"direction", UiNavigationDirectionNames.getRight(action.uiNavigationDirection)}};
  if (action.kind == MobileTouchActionKind::None)
    return JsonObject{{"type", "none"}};
  return JsonObject{{"type", "key"}, {"key", KeyNames.getRight(action.key)}};
}

MobileTouchAction touchActionFromJson(Json const& json, MobileTouchAction def) {
  auto type = json.getString("type", "key");
  if (type.equals("morePanel", String::CaseInsensitive)) { def.kind = MobileTouchActionKind::MorePanel; return def; }
  if (type.equals("keys", String::CaseInsensitive) || type.equals("macro", String::CaseInsensitive)) {
    auto keys = keysFromJson(json);
    return keys.empty() ? def : macroAction(keys, json.getBool("sequential", true));
  }
  if (type.equals("mouse", String::CaseInsensitive))
    return mouseAction(MouseButtonNames.valueLeft(json.getString("button", MouseButtonNames.getRight(def.mouseButton)), def.mouseButton));
  if (type.equals("wheel", String::CaseInsensitive))
    return wheelAction(!json.getString("direction", "up").equals("down", String::CaseInsensitive));
  if (type.equals("quickSlot", String::CaseInsensitive))
    return quickSlotAction((int)std::clamp(json.getInt("slot", 0), (int64_t)0, (int64_t)9), json.getBool("use", false));
  if (type.equals("miningMode", String::CaseInsensitive)) return miningModeAction();
  if (type.equals("targetLock", String::CaseInsensitive)) return targetLockAction();
  if (type.equals("targetNext", String::CaseInsensitive)) return targetLockAction(true);
  if (type.equals("touchHandSwap", String::CaseInsensitive))
    return touchHandSwapAction();
  if (type.equals("gyroToggle", String::CaseInsensitive) || type.equals("gyro", String::CaseInsensitive))
    return gyroToggleAction();
  if (type.equals("gamepadAimModeToggle", String::CaseInsensitive) || type.equals("aimModeToggle", String::CaseInsensitive))
    return gamepadAimModeToggleAction();
  if (type.equals("actionWheel", String::CaseInsensitive) || type.equals("wheelMenu", String::CaseInsensitive))
    return actionWheelAction();
  if (type.equals("inventoryWheel", String::CaseInsensitive) || type.equals("inventoryWheelMenu", String::CaseInsensitive))
    return inventoryWheelAction();
  if (type.equals("uiNavigation", String::CaseInsensitive) || type.equals("selection", String::CaseInsensitive))
    return uiNavigationAction(UiNavigationDirectionNames.valueLeft(json.getString("direction", UiNavigationDirectionNames.getRight(def.uiNavigationDirection)), def.uiNavigationDirection));
  if (type.equals("none", String::CaseInsensitive))
    return noneAction();
  auto keyName = json.getString("key", KeyNames.getRight(def.key));
  return keyAction(KeyNames.valueLeft(keyName, def.key));
}

Json jsonFromTouchElement(MobileTouchElement const& element) {
  JsonObject out{
    {"id", element.id},
    {"label", element.label},
    {"kind", elementKindName(element.kind)},
    {"enabled", element.enabled},
    {"position", JsonArray{element.position[0], element.position[1]}},
    {"size", element.size},
    {"diameterDp", element.diameterDp}, {"adaptive", element.adaptive}, {"panel", element.panel},
    {"pressMode", pressModeName(element.pressMode)},
    {"aimSensitivity", element.aimSensitivity},
    {"opacity", element.opacity},
    {"preciseAim", element.preciseAim},
    {"action", jsonFromTouchAction(element.action)}
  };
  if (element.kind == MobileTouchElementKind::DPad) {
    out["upAction"] = jsonFromTouchAction(element.upAction);
    out["downAction"] = jsonFromTouchAction(element.downAction);
    out["leftAction"] = jsonFromTouchAction(element.leftAction);
    out["rightAction"] = jsonFromTouchAction(element.rightAction);
  }
  if (element.kind == MobileTouchElementKind::PerformanceCounter)
    out["perfCounterMode"] = perfCounterModeName(element.perfCounterMode);
  return out;
}

MobileTouchElement touchElementFromJson(Json const& json, MobileTouchElement def) {
  def.id = json.getString("id", def.id);
  def.label = json.getString("label", def.label);
  def.kind = elementKindFromName(json.getString("kind", elementKindName(def.kind)));
  def.enabled = json.getBool("enabled", def.enabled);
  def.size = json.getFloat("size", def.size);
  def.diameterDp = std::clamp(json.getFloat("diameterDp", 0), 0.f, 256.f);
  def.adaptive = json.getBool("adaptive", false);
  def.panel = json.getBool("panel", false);
  def.opacity = std::clamp(json.getFloat("opacity", def.opacity), 0.0f, 1.0f);
  def.pressMode = pressModeFromName(json.getString("pressMode", pressModeName(def.pressMode)), def.pressMode);
  def.aimSensitivity = json.getFloat("aimSensitivity", def.aimSensitivity);
  def.preciseAim = json.getBool("preciseAim", def.preciseAim);
  def.perfCounterMode = perfCounterModeFromName(json.getString("perfCounterMode", perfCounterModeName(def.perfCounterMode)), def.perfCounterMode);
  if (auto pos = json.optArray("position")) {
    if (pos->size() >= 2)
      def.position = Vec2F(pos->get(0).toFloat(), pos->get(1).toFloat());
  }
  def.action = touchActionFromJson(json.get("action", jsonFromTouchAction(def.action)), def.action);
  def.upAction = touchActionFromJson(json.get("upAction", jsonFromTouchAction(def.upAction)), def.upAction);
  def.downAction = touchActionFromJson(json.get("downAction", jsonFromTouchAction(def.downAction)), def.downAction);
  def.leftAction = touchActionFromJson(json.get("leftAction", jsonFromTouchAction(def.leftAction)), def.leftAction);
  def.rightAction = touchActionFromJson(json.get("rightAction", jsonFromTouchAction(def.rightAction)), def.rightAction);
  return def;
}

JsonArray jsonFromTouchElements(std::vector<MobileTouchElement> const& elements) {
  JsonArray out;
  for (auto const& element : elements)
    out.append(jsonFromTouchElement(element));
  return out;
}

std::vector<MobileTouchElement> touchElementsFromConfig(Json const& config) {
  auto elements = defaultTouchElements();
  if (auto saved = config.optQueryArray("touch.elements")) {
    // Old layouts retain their geometry and visibility, including custom IDs.
    for (auto& e : elements) if (e.id == "more" || e.id == "crouch") e.enabled = false;
    for (auto const& savedElementJson : *saved) {
      String id = savedElementJson.getString("id", "");
      bool merged = false;
      for (auto& element : elements) {
        if (element.id == id) {
          element = touchElementFromJson(savedElementJson, element);
          merged = true;
          break;
        }
      }
      if (!merged && !id.empty())
        elements.push_back(touchElementFromJson(savedElementJson, MobileTouchElement{}));
    }
  }
  MobileTouchConfig view; loadTouchAimOptions(view,config);
  bool hasSecondary=std::any_of(elements.begin(),elements.end(),[&](auto const& e) { return e.enabled && touchElementInPanel(e,view); });
  if(view.compactHud && hasSecondary) {
    auto more=std::find_if(elements.begin(),elements.end(),[](auto const& e) { return e.action.kind==MobileTouchActionKind::MorePanel; });
    if(more!=elements.end()) more->enabled=true;
    else {
      auto defaults=defaultTouchElements();
      auto button=*std::find_if(defaults.begin(),defaults.end(),[](auto const& e) { return e.action.kind==MobileTouchActionKind::MorePanel; });
      size_t sequence=0;
      do { button.id=strf("compactMore{}",sequence++); }
      while(std::any_of(elements.begin(),elements.end(),[&](auto const& e) { return e.id==button.id; }));
      elements.push_back(std::move(button));
    }
  }
  return elements;
}

std::vector<MobileTouchElement> touchElementsForScreen(Json const& config, Vec2U size) {
  auto name=config.queryString("touch.activeLayout", "");
  if(name.empty()) return touchElementsFromConfig(config);
  auto orientation=size[0]>=size[1] ? "landscape" : "portrait";
  float aspect=(float)size[0]/std::max(1u,size[1]), distance=1e30f;
  Json selected;
  for(auto const& profile : config.queryArray("touch.layoutProfiles",{})) {
    if(profile.getString("name","")!=name || profile.getString("orientation",orientation)!=orientation) continue;
    float delta=std::abs(profile.getFloat("aspect",aspect)-aspect);
    if(delta<distance) { selected=profile; distance=delta; }
  }
  if(selected.isNull()) return touchElementsFromConfig(config);
  return touchElementsFromConfig(JsonObject{{"touch",JsonObject{{"elements",selected.getArray("elements",{})}}}});
}

String gamepadStickModeName(MobileGamepadStickMode mode) {
  switch (mode) {
    case MobileGamepadStickMode::Aim:
      return "aim";
    default:
      return "movement";
  }
}

MobileGamepadStickMode gamepadStickModeFromName(String const& name, MobileGamepadStickMode def) {
  if (name.equals("aim", String::CaseInsensitive) || name.equals("cameraAim", String::CaseInsensitive) || name.equals("camera", String::CaseInsensitive))
    return MobileGamepadStickMode::Aim;
  if (name.equals("movement", String::CaseInsensitive) || name.equals("move", String::CaseInsensitive))
    return MobileGamepadStickMode::Movement;
  return def;
}

String directTouchGestureModeName(DirectTouchGestureMode mode) {
  switch (mode) {
    case DirectTouchGestureMode::Touchpad:
      return "touchpad";
    default:
      return "touchscreen";
  }
}

DirectTouchGestureMode directTouchGestureModeFromName(String const& name, DirectTouchGestureMode def) {
  if (name.equals("touchpad", String::CaseInsensitive))
    return DirectTouchGestureMode::Touchpad;
  if (name.equals("touchscreen", String::CaseInsensitive))
    return DirectTouchGestureMode::Touchscreen;
  return def;
}

Json jsonFromGamepadStick(MobileGamepadStickConfig const& stick) {
  return JsonObject{
    {"enabled", stick.enabled},
    {"mode", gamepadStickModeName(stick.mode)},
    {"preciseAim", stick.preciseAim},
    {"deadzone", stick.deadzone},
    {"sensitivity", stick.sensitivity},
    {"invertX", stick.invertX},
    {"invertY", stick.invertY}
  };
}

MobileGamepadStickConfig gamepadStickFromJson(Json const& json, MobileGamepadStickConfig def) {
  def.enabled = json.getBool("enabled", def.enabled);
  def.mode = gamepadStickModeFromName(json.getString("mode", gamepadStickModeName(def.mode)), def.mode);
  def.preciseAim = json.getBool("preciseAim", def.preciseAim);
  def.deadzone = json.getFloat("deadzone", def.deadzone);
  def.sensitivity = json.getFloat("sensitivity", def.sensitivity);
  def.invertX = json.getBool("invertX", def.invertX);
  def.invertY = json.getBool("invertY", def.invertY);
  return def;
}

MobileGamepadConfig gamepadConfigFromConfig(Json const& config) {
  MobileGamepadConfig out;
  out.enabled = config.queryBool("gamepad.enabled", true);
  out.triggerThreshold = config.queryFloat("gamepad.triggerThreshold", out.triggerThreshold);
  out.leftStick = gamepadStickFromJson(config.query("gamepad.leftStick", JsonObject()), out.leftStick);
  out.rightStick = gamepadStickFromJson(config.query("gamepad.rightStick", JsonObject()), out.rightStick);
  return out;
}

JsonArray jsonFromGamepadBindings(std::vector<MobileGamepadBinding> const& bindings) {
  JsonArray out;
  for (auto const& binding : bindings) {
    out.append(JsonObject{
      {"button", ControllerButtonNames.getRight(binding.button)},
      {"label", binding.label},
      {"enabled", binding.enabled},
      {"action", jsonFromTouchAction(binding.action)},
      {"pressMode", pressModeName(binding.pressMode)}
    });
  }
  return out;
}

MobileGamepadBinding gamepadBindingFromJson(Json const& json, MobileGamepadBinding def) {
  def.button = ControllerButtonNames.valueLeft(json.getString("button", ControllerButtonNames.getRight(def.button)), def.button);
  def.label = json.getString("label", def.label);
  def.enabled = json.getBool("enabled", def.enabled);
  def.action = touchActionFromJson(json.get("action", jsonFromTouchAction(def.action)), def.action);
  def.pressMode = pressModeFromName(json.getString("pressMode", pressModeName(def.pressMode)), def.pressMode);

  auto migrateKeySinglePressDefaultToHold = [&](ControllerButton button, Key key) {
    if (def.button == button && def.pressMode == MobileTouchPressMode::SinglePress && def.action.kind == MobileTouchActionKind::Key && def.action.key == key)
      def.pressMode = MobileTouchPressMode::Hold;
  };
  migrateKeySinglePressDefaultToHold(ControllerButton::A, Key::Space);
  migrateKeySinglePressDefaultToHold(ControllerButton::B, Key::Escape);
  migrateKeySinglePressDefaultToHold(ControllerButton::X, Key::E);
  migrateKeySinglePressDefaultToHold(ControllerButton::Back, Key::I);
  migrateKeySinglePressDefaultToHold(ControllerButton::Start, Key::Escape);

  if (def.button == ControllerButton::B && def.action.kind == MobileTouchActionKind::Key && def.action.key == Key::Escape) {
    def.action = keyAction(Key::F);
    def.pressMode = MobileTouchPressMode::Hold;
  }

  if (def.button == ControllerButton::Y && def.action.kind == MobileTouchActionKind::Key && def.action.key == Key::Q) {
    def.action = actionWheelAction();
    def.pressMode = MobileTouchPressMode::Hold;
  }

  if (def.button == ControllerButton::Y && def.action.kind == MobileTouchActionKind::ActionWheel) {
    def.action = keyAction(Key::I);
    def.pressMode = MobileTouchPressMode::SinglePress;
  }

  if (def.button == ControllerButton::LeftShoulder && def.action.kind == MobileTouchActionKind::MouseWheelUp) {
    def.action = inventoryWheelAction();
    def.pressMode = MobileTouchPressMode::Hold;
  }

  if (def.button == ControllerButton::RightShoulder && def.action.kind == MobileTouchActionKind::MouseWheelDown) {
    def.action = actionWheelAction();
    def.pressMode = MobileTouchPressMode::Hold;
  }

  auto migrateDPadMovementDefault = [&](ControllerButton button, Key key, UiNavigationDirection direction) {
    if (def.button == button && def.pressMode == MobileTouchPressMode::Hold && def.action.kind == MobileTouchActionKind::Key && def.action.key == key) {
      def.action = uiNavigationAction(direction);
      def.pressMode = MobileTouchPressMode::SinglePress;
    }
  };
  migrateDPadMovementDefault(ControllerButton::DPadUp, Key::W, UiNavigationDirection::Up);
  migrateDPadMovementDefault(ControllerButton::DPadDown, Key::S, UiNavigationDirection::Down);
  migrateDPadMovementDefault(ControllerButton::DPadLeft, Key::A, UiNavigationDirection::Left);
  migrateDPadMovementDefault(ControllerButton::DPadRight, Key::D, UiNavigationDirection::Right);
  return def;
}

std::vector<MobileGamepadBinding> gamepadBindingsFromConfig(Json const& config) {
  auto bindings = defaultGamepadBindings();
  if (auto saved = config.optQueryArray("gamepad.bindings")) {
    for (auto const& savedBindingJson : *saved) {
      auto button = ControllerButtonNames.maybeLeft(savedBindingJson.getString("button", ""));
      if (!button)
        continue;

      bool merged = false;
      for (auto& binding : bindings) {
        if (binding.button == *button) {
          binding = gamepadBindingFromJson(savedBindingJson, binding);
          merged = true;
          break;
        }
      }
      if (!merged)
        bindings.push_back(gamepadBindingFromJson(savedBindingJson, MobileGamepadBinding{}));
    }
  }
  return bindings;
}

class MobileTouchInputAdapter::Impl {
public:
  explicit Impl(Vec2U* windowSize, Vec2U* renderCanvasSize = nullptr, float* displayScale = nullptr, SafeAreaInsets* safeArea = nullptr, SafeAreaInsets* renderSafeArea = nullptr)
    : m_windowSize(windowSize), m_renderCanvasSize(renderCanvasSize), m_displayScalePtr(displayScale), m_safeAreaPtr(safeArea), m_renderSafeAreaPtr(renderSafeArea ? renderSafeArea : safeArea) {
    m_generatedEvents.reserve(256);
    rebuildOverlayCache();
  }

  void setConfig(MobileTouchConfig config) {
    m_geometryDirty = true;
    if (!m_gyroAvailable)
      config.gyroEnabled = false;
    if (!config.directTouchGestures && m_config.directTouchGestures)
      cancelDirectTouchGestures();
    if (config.gyroEnabled && !m_config.gyroEnabled) {
      m_gyroRuntimeEnabled = true;
      m_lastGyroFrameMs = 0;
    } else if (!config.gyroEnabled) {
      m_gyroRuntimeEnabled = false;
      m_lastGyroFrameMs = 0;
      m_hasGyroInput = false;
    }
    if (m_config.enabled && !config.enabled)
      cancelAll();
    m_config = config;
  }

  void setGyroAvailable(bool available) {
    m_gyroAvailable = available;
    if (!m_gyroAvailable) {
      m_config.gyroEnabled = false;
      m_gyroRuntimeEnabled = false;
      m_lastGyroFrameMs = 0;
      m_hasGyroInput = false;
    }
  }

  void setElements(std::vector<MobileTouchElement> elements) {
    cancelAll();
    m_elements = std::move(elements);
    rebuildOverlayCache();
  }

  void setHud(TouchLayout::Hud hud) {
    auto game=gameViewport(), touch=touchViewport();
    for(size_t i=0;i<hud.count;++i) {
      auto& rect=hud.regions[i];
      rect.left=(rect.left*game.width+game.left-touch.left)/touch.width;
      rect.right=(rect.right*game.width+game.left-touch.left)/touch.width;
      rect.top=(rect.top*game.height+game.top-touch.top)/touch.height;
      rect.bottom=(rect.bottom*game.height+game.top-touch.top)/touch.height;
    }
    if (m_hud.count != hud.count) m_geometryDirty = true;
    for(size_t i=0;i<hud.count;++i) {
      auto a=m_hud.regions[i], b=hud.regions[i];
      if(a.left!=b.left || a.right!=b.right || a.top!=b.top || a.bottom!=b.bottom) m_geometryDirty=true;
    }
    m_hud = hud; updateGeometry();
  }

  void setContext(unsigned mode) {
    if (mode != m_context) {
      cancelAll();
      m_context = mode;
    }
  }

  void beginFrame() {
    // Preserva liberações geradas entre frames até o cliente recebê-las.
  }

  void endFrame() {
    bool panelHeld = false;
    for (auto const& entry : m_fingers) {
      auto element = findElement(entry.second.elementId);
      panelHeld |= element && touchElementInPanel(*element,m_config);
    }
    m_drawer.tick(Time::monotonicMilliseconds() / 1000.0, panelHeld);
    if (m_config.enabled && m_context == 0)
      emitActionEdges();
  }

  void appendGeneratedEvents(List<InputEvent>& outEvents) {
    outEvents.appendAll(m_generatedEvents);
    m_generatedEvents.clear();
  }

  bool gyroSensorRequested() const {
    return m_gyroAvailable && m_config.gyroEnabled;
  }

  void setGyroInput(std::array<float, 3> const& data, bool hasData, SDL_DisplayOrientation orientation) {
    if (!m_config.gyroEnabled) {
      m_hasGyroInput = false;
      return;
    }

    if (hasData) {
      m_gyroInput = data;
      m_hasGyroInput = true;
      m_lastGyroInputMs = Time::monotonicMilliseconds();
    }
    m_gyroOrientation = orientation;
  }

  bool processSdlEvent(SDL_Event const& event) {
    if (event.type != SDL_EVENT_FINGER_DOWN
        && event.type != SDL_EVENT_FINGER_UP
        && event.type != SDL_EVENT_FINGER_MOTION
        && event.type != SDL_EVENT_FINGER_CANCELED) {
      return false;
    }

    Vec2F pos = toScreen(event.tfinger.x, event.tfinger.y);
    uint64_t finger = event.tfinger.fingerID;
    updateGeometry();
    if (m_context != 0) {
      TouchPhase phase = event.type == SDL_EVENT_FINGER_DOWN ? TouchPhase::Down
          : event.type == SDL_EVENT_FINGER_MOTION ? TouchPhase::Move
          : event.type == SDL_EVENT_FINGER_UP ? TouchPhase::Up : TouchPhase::Cancel;
      float density = m_displayScalePtr ? *m_displayScalePtr : 1.0f;
      emitEvent(TouchPointerEvent{finger, phase, toInputSpace(pos), TouchInput::minimumTarget(density) * inputCanvasSize()[1] / std::max(1.0f, canvasSize()[1])});
      return true;
    }

    if (!m_config.enabled)
      return false;
    if (event.type == SDL_EVENT_FINGER_DOWN)
      assignFinger(finger, pos);
    else if (event.type == SDL_EVENT_FINGER_MOTION)
      updateFinger(finger, pos);
    else if (event.type == SDL_EVENT_FINGER_CANCELED)
      releaseFinger(finger, pos, true);
    else
      releaseFinger(finger, pos);

    return true;
  }

  void cancelAll() {
    emitEvent(TouchPointerEvent{0, TouchPhase::Cancel, {}});
    emitEvent(TargetLockEvent{TargetLockCommand::Cancel});
    for (auto const& entry : m_quickSlotOwners)
      emitEvent(QuickSlotEvent{(unsigned)entry.second.quickSlot, false, entry.second.quickUse, entry.first});
    m_quickSlotOwners.clear();
    clearPulsedActions();
    m_macroEvents.clear();

    for (auto const& pair : m_keyHoldCounts.pairs()) {
      if (pair.second > 0)
        emitEvent(KeyUpEvent{pair.first, InputSource::Touch});
    }
    for (auto const& pair : m_mouseHoldCounts.pairs()) {
      if (pair.second > 0) {
        syncVirtualAimCursor(false, m_config.joystickAimCursorEnabled);
        emitEvent(MouseButtonUpEvent{pair.first, mouseActionPosition(), InputSource::Touch});
      }
    }

    m_drawer.close();
    m_aimAttack.release(m_config.aimAttackMode, true);
    m_aimAttackHeld = false;
    m_lastAimFrameMs = 0;
    closeTouchWheel(false);
    m_fingers.clear();
    m_heldElements.clear();
    m_toggledElements.clear();
    m_dpadHeld.clear();
    m_nextDPadWheelMs.clear();
    m_nextActionRepeatMs.clear();
    m_keyActionOwners.clear();
    m_keyHoldCounts.clear();
    m_mouseActionOwners.clear();
    m_mouseHoldCounts.clear();

    m_joystickActive = false;
    m_joystickFinger = 0;
    m_joystickElementId.clear();
    m_moveVec = {};
    m_joyRight = m_joyLeft = m_joyUp = m_joyDown = false;
    m_aimJoystickActive = false;
    m_aimJoystickFinger = 0;
    m_aimJoystickElementId.clear();
    m_aimVec = {};

    if (m_primaryMouseHeld)
      releaseDirectGestureAction(m_config.directTouchSingleAction, DirectTouchSingleOwner);
    if (m_secondaryMouseHeld)
      releaseDirectGestureAction(m_config.directTouchTwoFingerAction, DirectTouchTwoFingerOwner);

    m_primaryHeld = false;
    m_primaryMouseHeld = false;
    m_aimFinger = 0;
    m_primaryPausedForSecondary = false;

    m_secondaryHeld = false;
    m_secondaryMouseHeld = false;
    m_secondaryFinger = 0;
  }

  static void perfCounterText(PerformanceCounterMode mode, float fps, char* text, size_t capacity) {
    std::snprintf(text, capacity, "%.0f FPS", fps);
    auto append = [&](char const* line) {
      size_t used = std::strlen(text);
      if (*line && used < capacity) std::snprintf(text + used, capacity - used, "\n%s", line);
    };
    char line[256];
    if (mode == PerformanceCounterMode::Memory) {
      auto usage = memoryUsageCached();
      if (!usage.valid()) append("RAM n/a");
      else {
        std::snprintf(line, sizeof(line), "RAM %.0f/%.0fMB", usage.used / 1048576.0, usage.budget / 1048576.0);
        append(line);
      }
      std::snprintf(line, sizeof(line), "img %.0fMB tex %.0fMB",
          memoryAccountBytes(MemoryCategory::AssetCache) / 1048576.0,
          memoryAccountBytes(MemoryCategory::TextureAtlas) / 1048576.0);
      append(line);
    } else if (mode == PerformanceCounterMode::Detailed) {
      static String const keys[] = {"client_update_rate", "client_render_world_total", "client_render_interface"};
      for (auto const& key : keys) { LogMap::copyValue(key, line, sizeof(line)); append(line); }
    }
  }

  float overlayOpacity() {
    int64_t now = Time::monotonicMilliseconds();
    if (!m_fingers.empty() || !m_lastActivityMs) m_lastActivityMs = now;
    return std::clamp(m_config.opacity, 0.0f, 1.0f) * TouchInput::idleFade((float)(now - m_lastActivityMs));
  }

  void renderShortcutIcons(Application& application) {
    updateGeometry();
    m_shortcutRendered.assign(m_elements.size(), false);
    if (!m_config.enabled || m_context != 0 || application.touchInputMode() != 0) return;
    float opacity = overlayOpacity();
    for (size_t i = 0; i < m_elements.size(); ++i) {
      auto const& element = m_elements[i];
      if (!visible(element) || element.opacity <= 0 || element.kind != MobileTouchElementKind::Button
          || element.action.kind != MobileTouchActionKind::QuickSlot) continue;
      Vec2F center = elementCenter(element);
      auto at=toInputSpace(center);
      auto edge=toInputSpace(center+Vec2F(elementRadius(element),0));
      float radius=(edge-at).magnitude();
      center=at;
      m_shortcutRendered[i] = application.renderTouchShortcut({(unsigned)element.action.quickSlot, center, radius, opacity * element.opacity});
    }
  }

  void drawOverlay(float fps, bool directionalMining) {
    if (!m_config.enabled || m_context != 0) return;
    updateGeometry();
    auto draw = ImGui::GetForegroundDrawList();
    auto scale = physicalToDrawScale();
    float rs = std::min(scale[0], scale[1]);
    float density = layoutMetrics().dp() / rs;
    auto ip = [&](Vec2F p) {
      return ImVec2((p[0] + (m_safeAreaPtr ? m_safeAreaPtr->left : 0)) / scale[0],
          (p[1] + (m_safeAreaPtr ? m_safeAreaPtr->top : 0)) / scale[1]);
    };
    float opacity = overlayOpacity();
    float thickness = std::max(1.f, 2 * density);
    m_overlayLabels.clear();
    bool detailed = false;
    for (size_t i = 0; i < m_elements.size(); ++i) {
      auto const& e = m_elements[i];
      if (!visible(e)) continue;
      auto& cache = m_overlayCache[i];
      auto geometry = m_geometry[i];
      Vec2F center{geometry.x, geometry.y};
      bool move = e.kind == MobileTouchElementKind::Joystick;
      bool aim = e.kind == MobileTouchElementKind::AimJoystick;
      bool live = move ? m_joystickActive && m_joystickElementId == e.id
          : aim ? m_aimJoystickActive && m_aimJoystickElementId == e.id : heldElement(e.id);
      bool active = live || (e.action.kind == MobileTouchActionKind::MiningModeToggle && directionalMining)
          || (e.action.kind == MobileTouchActionKind::TargetLockToggle && m_hud.locked)
          || (e.action.kind == MobileTouchActionKind::MorePanel && m_drawer.open)
          || (e.action.kind == MobileTouchActionKind::GyroToggle && m_gyroRuntimeEnabled && m_config.gyroEnabled);
      if (move && live) center = m_joystickOrigin;
      if (aim && live) center = m_aimJoystickOrigin;
      ImVec2 c = ip(center);
      float radius = geometry.radius / rs;
      float op = std::clamp(opacity * e.opacity, 0.f, 1.f);
      ImU32 ink = IM_COL32(231,240,250,(int)(255 * std::min(1.f, op * 2)));
      ImU32 accent = IM_COL32(105,202,255,(int)(255 * std::min(1.f,op * 2.5f)));
      ImU32 border = active ? accent : ink;
      bool itemIcon = i < m_shortcutRendered.size() && m_shortcutRendered[i];
      if (e.kind == MobileTouchElementKind::PerformanceCounter) {
        detailed |= e.perfCounterMode == PerformanceCounterMode::Detailed;
        if (Time::monotonicMilliseconds() >= cache.nextPerfMs) {
          perfCounterText(e.perfCounterMode, fps, cache.perf.data(), cache.perf.size());
          cache.nextPerfMs = Time::monotonicMilliseconds() + 250;
        }
        m_overlayLabels.push_back({c,cache.perf.data(),0,e.opacity});
        continue;
      }
      // Every shape and glyph uses the existing ImGui white/font atlas and
      // normal alpha blending. No GL callbacks, new textures or texture uploads.
      float pressedRadius = radius * (live && !move && !aim ? .92f : 1.f);
      if (!itemIcon) {
        draw->AddCircleFilled(ImVec2(c.x,c.y+2*density), pressedRadius+2*density, IM_COL32(0,0,0,(int)(60*op)),32);
        draw->AddCircleFilled(c,pressedRadius,IM_COL32(10,20,35,(int)((live ? 1.85f : 1.f)*255*op)),32);
      }
      draw->AddCircle(c,pressedRadius,border,32,thickness);
      if (move || aim) {
        ImVec2 knob = live ? ip(move ? m_joystickCurrent : m_aimJoystickCurrent) : c;
        draw->AddCircleFilled(knob,radius*.35f,IM_COL32(156,187,213,(int)(125*op)),24);
        draw->AddCircle(knob,radius*.35f,ink,24,thickness);
        float k = radius * .18f;
        if (aim) {
          draw->AddLine(ImVec2(knob.x-k,knob.y),ImVec2(knob.x+k,knob.y),ink,thickness);
          draw->AddLine(ImVec2(knob.x,knob.y-k),ImVec2(knob.x,knob.y+k),ink,thickness);
        } else {
          drawButtonIcon(draw,knob,radius*.35f,ButtonIcon::Wheel,ink,thickness);
        }
      } else if (e.kind == MobileTouchElementKind::DPad) {
        drawDPad(draw,c,radius,cache,ink,accent,thickness,m_overlayLabels);
      } else {
        size_t first = m_overlayLabels.size();
        if (!itemIcon) {
          if (cache.icon == ButtonIcon::None)
            queueButtonLabel(c,pressedRadius,e.label.utf8Ptr(),m_overlayLabels,cache);
          else drawButtonIcon(draw,c,pressedRadius,cache.icon,ink,thickness);
        }
        for (size_t j=first;j<m_overlayLabels.size();++j) m_overlayLabels[j].opacity=e.opacity;
        // State is conveyed by the icon/ring; long translated badges never
        // protrude into the next control or across the gameplay view.
        if (active) draw->AddCircleFilled(ImVec2(c.x+radius*.60f,c.y-radius*.60f),3*density,accent,12);
      }
    }
    LogMap::setObserved(detailed);
    for (auto const& label : m_overlayLabels)
      drawLabelWithOutline(draw,label.pos,label.text,IM_COL32(240,245,255,(int)(255*std::min(1.f,opacity*2)*label.opacity)),
          IM_COL32(0,0,0,(int)(255*opacity*label.opacity)),label.fontSize);
  }

  bool overlayEnabled() const {
    return m_config.enabled;
  }

  static void drawPreviewControl(ImDrawList* draw, MobileTouchElement const& e, Vec2F center, float radius, float opacity, bool selected) {
    ImVec2 c(center[0],center[1]);
    ImU32 ink=IM_COL32(231,240,250,(int)(255*std::min(1.f,opacity*2)));
    ImU32 accent=selected ? IM_COL32(105,202,255,255) : ink;
    draw->AddCircleFilled(c,radius,IM_COL32(10,20,35,(int)(255*opacity)),32);
    draw->AddCircle(c,radius,accent,32,2);
    if(e.kind==MobileTouchElementKind::Joystick || e.kind==MobileTouchElementKind::AimJoystick || e.kind==MobileTouchElementKind::DPad) {
      draw->AddCircle(c,radius*.35f,ink,24,2);
      draw->AddLine(ImVec2(c.x-radius*.2f,c.y),ImVec2(c.x+radius*.2f,c.y),ink,2);
      draw->AddLine(ImVec2(c.x,c.y-radius*.2f),ImVec2(c.x,c.y+radius*.2f),ink,2);
    } else if(e.kind==MobileTouchElementKind::PerformanceCounter) {
      draw->AddText(c,ink,"60 FPS");
    } else {
      auto icon=buttonIconFor(e);
      if(icon!=ButtonIcon::None) drawButtonIcon(draw,c,radius,icon,ink,2);
      else {
        float font=std::max(8.f,radius*.48f);
        std::array<char,96> label{};
        TouchLayout::fitLabel(label.data(),label.size(),e.label.utf8(),radius*1.55f,[&](char const* t) { return ImGui::GetFont()->CalcTextSizeA(font,FLT_MAX,0,t).x; });
        ImVec2 text=ImGui::GetFont()->CalcTextSizeA(font,FLT_MAX,0,label.data());
        draw->AddText(ImGui::GetFont(),font,ImVec2(c.x-text.x*.5f,c.y-text.y*.5f),ink,label.data());
      }
    }
  }

private:
  enum class FingerRole {
    None,
    Joystick,
    AimJoystick,
    Aim,
    SuppressedTap,
    SecondaryHold,
    ActionButton,
    DPad
  };

  struct FingerState {
    FingerRole role = FingerRole::None;
    Vec2F startPos;
    Vec2F currentPos;
    String elementId;
    String lastDpadDirection;
    MobileTouchAction action;
    MobileTouchPressMode pressMode = MobileTouchPressMode::Hold;
    int64_t downTimeMs = 0;
    bool movedTooFarForTap = false;
    // Touchpad mode (FingerRole::Aim only): the finger's raw position as of
    // the last update, used to compute the next frame's swipe delta. Kept
    // separate from currentPos, which trackTapMotion overwrites unconditionally.
    Vec2F touchpadAnchor;
  };

  static bool insideCircle(Vec2F const& p, Vec2F const& center, float radius) {
    return (p - center).magnitudeSquared() <= radius * radius;
  }

  Vec2F canvasSize() const {
    Vec2U const& ws = *m_windowSize;
    float w = (float)ws[0];
    float h = (float)ws[1];
    if (m_safeAreaPtr) {
      if (ws[0] > m_safeAreaPtr->left + m_safeAreaPtr->right)
        w = (float)(ws[0] - m_safeAreaPtr->left - m_safeAreaPtr->right);
      if (ws[1] > m_safeAreaPtr->top + m_safeAreaPtr->bottom)
        h = (float)(ws[1] - m_safeAreaPtr->top - m_safeAreaPtr->bottom);
    }
    return {w, h};
  }

  TouchLayout::Metrics layoutMetrics() const {
    auto size = canvasSize();
    return {size[0], size[1], m_displayScalePtr ? *m_displayScalePtr : 1, m_config.size};
  }
  bool allowed(MobileTouchElement const& e) const {
    return e.enabled && (m_config.lockOnEnabled || (e.action.kind!=MobileTouchActionKind::TargetLockToggle && e.action.kind!=MobileTouchActionKind::TargetLockNext));
  }
  bool visible(MobileTouchElement const& e) const {
    size_t i = &e - m_elements.data();
    return e.enabled && i < m_geometry.size() && m_geometry[i].visible
        && (m_config.lockOnEnabled || (e.action.kind != MobileTouchActionKind::TargetLockToggle && e.action.kind != MobileTouchActionKind::TargetLockNext));
  }
  void updateGeometry() {
    auto metrics = layoutMetrics();
    if (!m_geometryDirty && metrics.width==m_previousMetrics.width && metrics.height==m_previousMetrics.height
        && metrics.density==m_previousMetrics.density && metrics.scale==m_previousMetrics.scale
        && m_drawer.open==m_previousDrawer.open && m_drawer.page==m_previousDrawer.page) return;
    m_previousMetrics=metrics; m_geometryDirty=false;
    m_geometry.resize(m_elements.size());
    size_t panelCount = 0;
    for (size_t i = 0; i < m_elements.size(); ++i) {
      auto const& e = m_elements[i];
      auto c = touchElementCircle(e,m_config,metrics);
      c.visible = allowed(e) && !touchElementInPanel(e,m_config);
      if (!m_config.lockOnEnabled && (e.action.kind == MobileTouchActionKind::TargetLockToggle || e.action.kind == MobileTouchActionKind::TargetLockNext)) c.visible = false;
      if (e.action.kind == MobileTouchActionKind::MorePanel && m_config.panelLeft) c.x = metrics.width - c.x;
      if (c.visible && (e.adaptive || (m_config.compactHud && e.kind==MobileTouchElementKind::Button)) && e.kind != MobileTouchElementKind::PerformanceCounter)
        c = TouchLayout::avoid(c,metrics,m_hud,m_geometry.data(),i);
      m_geometry[i] = c;
      panelCount += allowed(e) && touchElementInPanel(e,m_config);
    }
    TouchLayout::Circle menu{}, movement{}, aim{};
    menu.visible=movement.visible=aim.visible=false;
    for(size_t i=0;i<m_elements.size();++i) {
      if(m_elements[i].action.kind==MobileTouchActionKind::MorePanel) menu=m_geometry[i];
      if(m_elements[i].kind==MobileTouchElementKind::Joystick) movement=m_geometry[i];
      if(m_elements[i].kind==MobileTouchElementKind::AimJoystick) aim=m_geometry[i];
    }
    auto cells=TouchLayout::panelCells(metrics,m_hud,m_config.panelLeft,menu,movement,aim);
    size_t capacity = std::max<size_t>(1,cells.count);
    m_panelPages = std::max<size_t>(1,(panelCount+capacity-1)/capacity);
    m_drawer.page = std::min(m_drawer.page,m_panelPages-1);
    size_t index = 0;
    for (size_t i=0;i<m_elements.size();++i) {
      auto const& e=m_elements[i];
      if (!allowed(e) || !touchElementInPanel(e,m_config)) continue;
      auto c=cells.cells[index%capacity];
      c.visible=cells.count && m_drawer.open && index/capacity==m_drawer.page;
      if (!m_config.lockOnEnabled && (e.action.kind==MobileTouchActionKind::TargetLockToggle || e.action.kind==MobileTouchActionKind::TargetLockNext)) c.visible=false;
      m_geometry[i]=c;
      ++index;
    }
    // The drawer temporarily replaces any intersecting edge controls. Existing
    // owners keep receiving move/up/cancel events and never transfer pointers.
    if (m_drawer.open) for (size_t i=0;i<m_elements.size();++i) {
      if (touchElementInPanel(m_elements[i],m_config) || m_elements[i].action.kind==MobileTouchActionKind::MorePanel) continue;
      for (size_t j=0;j<m_elements.size();++j)
        if (touchElementInPanel(m_elements[j],m_config) && !TouchLayout::separated(m_geometry[i],m_geometry[j],8*metrics.dp())) m_geometry[i].visible=false;
    }
    m_previousDrawer=m_drawer;
  }
  Vec2F elementCenter(MobileTouchElement const& element) const {
    auto const& c = m_geometry[&element - m_elements.data()];
    return {c.x,c.y};
  }
  float elementRadius(MobileTouchElement const& element) const {
    return m_geometry[&element - m_elements.data()].radius;
  }

  MobileTouchElement const* findElement(String const& id) const {
    for (auto const& element : m_elements) {
      if (element.id == id)
        return &element;
    }
    return nullptr;
  }

  bool heldElement(String const& id) const {
    return m_heldElements.contains(id) || m_toggledElements.contains(id);
  }

  // ImGui draw lists use SDL window coordinates (logical points on iOS, physical
  // pixels on Android).  Derive the physical-pixel-to-ImGui-unit ratio from the
  // full SDL window size so the scale is always the true device pixel ratio,
  // independent of any safe-area inset.  Safe-area offsets are applied separately
  // in drawOverlay() so buttons land at the correct physical position.
  Vec2F physicalToDrawScale() const {
    Vec2U const& ws = *m_windowSize;

    if (ImGui::GetCurrentContext()) {
      ImVec2 displaySize = ImGui::GetIO().DisplaySize;
      if (displaySize.x > 0.0f && displaySize.y > 0.0f)
        return {
          std::max(1.0f, (float)ws[0] / displaySize.x),
          std::max(1.0f, (float)ws[1] / displaySize.y)
        };
    }

#ifdef STAR_SYSTEM_IOS
    if (m_displayScalePtr)
      return Vec2F::filled(std::max(1.0f, std::round(*m_displayScalePtr)));
#endif

    return Vec2F::filled(1.0f);
  }

  float controlRadius() const {
    Vec2F drawScale = physicalToDrawScale();
    float radiusScale = std::max(1.0f, std::min(drawScale[0], drawScale[1]));
    Vec2F drawSize = {
      (float)(*m_windowSize)[0] / drawScale[0],
      (float)(*m_windowSize)[1] / drawScale[1]
    };
    float shortSide = std::min(drawSize[0], drawSize[1]);
    return radiusScale * TouchInput::controlRadius(shortSide, m_config.size);
  }

  float tapMovementThreshold() const {
    float density = m_displayScalePtr ? *m_displayScalePtr : 1.0f;
    return TouchInput::tapThreshold(density);
  }

  bool isTap(FingerState const& state, Vec2F const& releasePos) const {
    if (state.movedTooFarForTap)
      return false;
    if (Time::monotonicMilliseconds() - state.downTimeMs > 220)
      return false;
    return (releasePos - state.startPos).magnitude() <= tapMovementThreshold();
  }

  void trackTapMotion(FingerState& state, Vec2F const& pos) {
    state.currentPos = pos;
    if (!state.movedTooFarForTap && (pos - state.startPos).magnitude() > tapMovementThreshold())
      state.movedTooFarForTap = true;
  }

  Vec2F toScreen(float x, float y) const {
    // SDL finger events can be normalized [0..1] or already in render-space
    // depending on conversion/platform path.
    Vec2F pos;
    if (x >= 0.0f && x <= 1.0f && y >= 0.0f && y <= 1.0f) {
      pos = {
        x * (float)(*m_windowSize)[0],
        y * (float)(*m_windowSize)[1]
      };
    } else if (ImGui::GetCurrentContext()) {
      ImVec2 displaySize = ImGui::GetIO().DisplaySize;
      Vec2F drawScale = physicalToDrawScale();
      if (displaySize.x > 0.0f && displaySize.y > 0.0f && (drawScale[0] > 1.0f || drawScale[1] > 1.0f)
          && x >= 0.0f && x <= displaySize.x && y >= 0.0f && y <= displaySize.y) {
        pos = { x * drawScale[0], y * drawScale[1] };
      } else {
        pos = { x, y };
      }
    } else {
      pos = { x, y };
    }

    // Translate from full-screen physical coords to game-canvas coords by
    // subtracting the safe-area insets (top-left origin offset).
    if (m_safeAreaPtr) {
      pos[0] -= (float)m_safeAreaPtr->left;
      pos[1] -= (float)m_safeAreaPtr->top;
    }
    return pos;
  }

  TouchLayout::Viewport touchViewport() const {
    auto size=canvasSize();
    return {size[0],size[1],m_safeAreaPtr ? (float)m_safeAreaPtr->left : 0,m_safeAreaPtr ? (float)m_safeAreaPtr->top : 0};
  }
  TouchLayout::Viewport gameViewport() const {
    auto inset=m_renderSafeAreaPtr ? *m_renderSafeAreaPtr : SafeAreaInsets{};
    return {std::max(1.f,(float)(*m_windowSize)[0]-inset.left-inset.right),std::max(1.f,(float)(*m_windowSize)[1]-inset.top-inset.bottom),(float)inset.left,(float)inset.top};
  }
  Vec2F toInputSpace(Vec2F const& pos) const {
    auto canvas=inputCanvasSize();
    auto point=TouchLayout::inputPoint({pos[0],pos[1]},touchViewport(),gameViewport(),canvas[0],canvas[1]);
    return {point[0],point[1]};
  }
  Vec2F toScreenSpace(Vec2F const& pos) const {
    auto canvas=inputCanvasSize();
    auto point=TouchLayout::screenPoint({pos[0],pos[1]},touchViewport(),gameViewport(),canvas[0],canvas[1]);
    return {point[0],point[1]};
  }

  Vec2F inputCanvasSize() const {
    return m_renderCanvasSize ? Vec2F(*m_renderCanvasSize) : canvasSize();
  }

  void assignFinger(uint64_t finger, Vec2F const& pos) {
    if (auto old = m_fingers.ptr(finger)) {
      Vec2F oldPos = old->currentPos;
      releaseFinger(finger, oldPos, true);
    }

    m_lastPointerInput = toInputSpace(pos);
    float radius = controlRadius();

    FingerState state;
    state.startPos = pos;
    state.currentPos = pos;
    state.downTimeMs = Time::monotonicMilliseconds();

    bool claimedControl = false;

    // Pass 1: buttons and d-pads. The nearest center wins, so two overlapping
    // hit areas never fire the wrong (first-listed) button.
    MobileTouchElement const* bestElement = nullptr;
    float bestDistance = 0.0f;
    for (auto const& element : m_elements) {
      if (!visible(element))
        continue;
      if (!m_config.lockOnEnabled && (element.action.kind == MobileTouchActionKind::TargetLockToggle || element.action.kind == MobileTouchActionKind::TargetLockNext))
        continue;
      if (element.kind != MobileTouchElementKind::Button && element.kind != MobileTouchElementKind::DPad)
        continue;

      Vec2F center = elementCenter(element);
      float elementRadius = this->elementRadius(element);
      float reach = 1.f;
      if (!insideCircle(pos, center, elementRadius * reach))
        continue;

      float distance = (pos - center).magnitude() / std::max(1.0f, elementRadius * reach);
      if (!bestElement || distance < bestDistance) {
        bestElement = &element;
        bestDistance = distance;
      }
    }

    if (bestElement) {
      for (auto const& pair : m_fingers) {
        if (pair.second.elementId == bestElement->id) {
          m_fingers[finger] = state;
          return;
        }
      }
      state.elementId = bestElement->id;
      if (bestElement->kind == MobileTouchElementKind::Button) {
        state.role = FingerRole::ActionButton;
        state.action = bestElement->action;
        state.pressMode = bestElement->action.kind == MobileTouchActionKind::QuickSlot
            ? MobileTouchPressMode::Hold : bestElement->pressMode;
        if (bestElement->action.kind == MobileTouchActionKind::MiningModeToggle || bestElement->action.kind == MobileTouchActionKind::TargetLockToggle || bestElement->action.kind == MobileTouchActionKind::TargetLockNext)
          state.pressMode = MobileTouchPressMode::SinglePress;
        if (isWheelAction(bestElement->action)) {
          // Hold to open the radial wheel, drag toward an option, release to
          // confirm (release near the middle cancels).
          m_heldElements.add(bestElement->id);
          openTouchWheel(finger, pos, bestElement->action.kind);
        } else {
          pressActionButton(*bestElement);
        }
      } else {
        state.role = FingerRole::DPad;
        updateDPadFinger(state, pos);
      }
      claimedControl = true;
    }

    // Pass 2: joysticks, only if no button took the touch.
    if (!claimedControl) {
      for (auto const& element : m_elements) {
        if (!visible(element))
          continue;

        Vec2F center = elementCenter(element);
        float elementRadius = this->elementRadius(element);
        if (element.kind == MobileTouchElementKind::Joystick && !m_joystickActive
            && (insideCircle(pos,center,elementRadius) || (m_config.floatingMovement && TouchLayout::floatingZone(pos[0],pos[1],layoutMetrics()) && !TouchLayout::blocked({pos[0],pos[1],0},layoutMetrics(),m_hud)))) {
          // Floating joystick: the wide touch zone around the stick starts it
          // right where the thumb lands, so the player never has to find the
          // ring by sight. The ring is drawn at this origin until release.
          state.role = FingerRole::Joystick;
          state.elementId = element.id;
          m_joystickActive = true;
          m_joystickFinger = finger;
          m_joystickElementId = element.id;
          auto origin=TouchLayout::clamp({pos[0],pos[1],elementRadius},layoutMetrics());
          bool safeOrigin=!TouchLayout::blocked(origin,layoutMetrics(),m_hud);
          for(size_t i=0;i<m_elements.size();++i) if(m_elements[i].id!=element.id)
            safeOrigin &= TouchLayout::separated(origin,m_geometry[i],8*layoutMetrics().dp());
          m_joystickOrigin = m_config.floatingMovement && safeOrigin ? Vec2F(origin.x,origin.y) : center;
          m_joystickCurrent = m_joystickOrigin;
          m_moveVec = {};
          claimedControl = true;
          break;
        } else if (element.kind == MobileTouchElementKind::AimJoystick && !m_aimJoystickActive
            && insideCircle(pos, center, elementRadius)) {
          state.role = FingerRole::AimJoystick;
          state.elementId = element.id;
          m_aimJoystickActive = true;
          m_aimJoystickFinger = finger;
          m_aimJoystickElementId = element.id;
          m_aimJoystickOrigin = center;
          m_aimJoystickCurrent = pos;
          ensureAimTarget();
          updateAimJoystickFinger(state, pos);
          if (m_config.aimAttackMode==TouchInput::AttackMode::Hold) {
            // Preserve a short tap even when DOWN and UP share an SDL batch.
            updateVirtualAimTarget();
            setMouseOwner("aimJoystick:attack",m_aimSecondary ? MouseButton::Right : MouseButton::Left,true);
            m_aimAttackHeld=true;
          }
          claimedControl = true;
          break;
        }
      }
    }

    bool occupiedJoystick = false;
    if (!claimedControl) {
      for (auto const& pair : m_fingers) {
        auto const& other = pair.second;
        if (other.role != FingerRole::Joystick && other.role != FingerRole::AimJoystick)
          continue;
        auto element = findElement(other.elementId);
        float reach = radius * (element ? std::clamp(element->size, 0.45f, 2.4f) : 1.0f) * 1.4f;
        Vec2F origin = other.role == FingerRole::Joystick ? m_joystickOrigin : m_aimJoystickOrigin;
        occupiedJoystick |= insideCircle(pos, origin, reach);
      }
    }
    if (claimedControl) {
      // already assigned above
    } else if (occupiedJoystick) {
      state.role = FingerRole::None;
      state.movedTooFarForTap = true;
    } else if (isEdgeTouch(pos)) {
      // Palm rejection: a finger that lands on the very edge of the screen
      // (where the hand rests when holding the phone, or where Android's
      // gesture bar lives) is not allowed to aim, click or mine.
      state.role = FingerRole::None;
      state.movedTooFarForTap = true;
    } else if (!m_config.directTouchGestures) {
      state.role = FingerRole::None;
      state.movedTooFarForTap = true;
    } else if (m_joystickActive && insideCircle(pos, m_joystickOrigin, radius * 1.35f)) {
      // Um segundo contato sobre o joystick não clica nem toma seu controle.
      state.role = FingerRole::None;
      state.movedTooFarForTap = true;
    } else if (m_primaryHeld && m_aimFinger != finger && !m_secondaryHeld) {
      state.role = FingerRole::SecondaryHold;
      m_secondaryHeld = true;
      m_secondaryFinger = finger;
      m_secondaryTouchPos = m_primaryHeld ? m_primaryTouchPos : pos;

      // Let the two-finger gesture's action take precedence while held.
      if (m_primaryMouseHeld) {
        releaseDirectGestureAction(m_config.directTouchSingleAction, DirectTouchSingleOwner);
        m_primaryMouseHeld = false;
        m_primaryPausedForSecondary = true;
      }

      m_secondaryMouseHeld = true;
      emitMouseMove(m_secondaryTouchPos);
      pressDirectGestureAction(m_config.directTouchTwoFingerAction, DirectTouchTwoFingerOwner);
    } else if (m_primaryHeld) {
      // A third touch should not steal ownership of the primary attack/aim
      // button or synthesize a click while the primary button is still held.
      state.role = FingerRole::SuppressedTap;
      state.movedTooFarForTap = true;
    } else {
      state.role = FingerRole::Aim;
      m_aimFinger = finger;
      m_primaryHeld = true;
      m_primaryMouseHeld = true;
      // Cursor tracking always happens regardless of what action is
      // configured below: moving your finger while touching aims the
      // reticle, independent of what fires on press.
      if (m_config.directTouchGestureMode == DirectTouchGestureMode::Touchpad) {
        // Touchpad mode: the cursor stays put on finger-down and only moves
        // by the swipe delta from here (see updateFinger), instead of
        // warping to the finger's position like touchscreen mode does.
        Vec2F cursorPos = m_hasCursorInputPosition ? toScreenSpace(m_cursorInputPosition) : canvasSize() * 0.5f;
        state.touchpadAnchor = pos;
        m_primaryTouchPos = cursorPos;
        emitMouseMove(cursorPos);
      } else {
        m_primaryTouchPos = pos;
        emitMouseMove(pos);
      }
      pressDirectGestureAction(m_config.directTouchSingleAction, DirectTouchSingleOwner);
    }

    m_fingers[finger] = state;
  }

  void updateFinger(uint64_t finger, Vec2F const& pos) {
    auto ptr = m_fingers.ptr(finger);
    if (!ptr)
      return;

    m_lastPointerInput = toInputSpace(pos);
    trackTapMotion(*ptr, pos);

    if (ptr->role == FingerRole::Joystick) {
      m_joystickCurrent = pos;
      float radius = controlRadius();
      if (auto element = findElement(ptr->elementId))
        radius = elementRadius(*element);
      Vec2F delta = pos - m_joystickOrigin;
      float mag = delta.magnitude();
      if (mag > radius)
        delta = delta / mag * radius;

      m_joystickCurrent = m_joystickOrigin + delta;
      auto value = TouchInput::stick(delta[0] / radius, delta[1] / radius, m_config.deadzone);
      m_moveVec = {value[0], value[1]};
    } else if (ptr->role == FingerRole::AimJoystick) {
      updateAimJoystickFinger(*ptr, pos);
    } else if (ptr->role == FingerRole::ActionButton && m_touchWheelActive && m_touchWheelFinger == finger) {
      updateTouchWheel(pos);
    } else if (ptr->role == FingerRole::Aim) {
      if (m_config.directTouchGestureMode == DirectTouchGestureMode::Touchpad) {
        Vec2F delta = pos - ptr->touchpadAnchor;
        ptr->touchpadAnchor = pos;
        Vec2F canvas = canvasSize();
        m_primaryTouchPos = Vec2F(
          std::clamp(m_primaryTouchPos[0] + delta[0], 0.0f, canvas[0]),
          std::clamp(m_primaryTouchPos[1] + delta[1], 0.0f, canvas[1]));
      } else {
        m_primaryTouchPos = pos;
      }
      emitMouseMove(m_primaryTouchPos);
    } else if (ptr->role == FingerRole::SecondaryHold && m_secondaryMouseHeld) {
      m_secondaryTouchPos = m_primaryHeld ? m_primaryTouchPos : pos;
      emitMouseMove(m_secondaryTouchPos);
    } else if (ptr->role == FingerRole::DPad) {
      updateDPadFinger(*ptr, pos);
    }
  }

  void releaseFinger(uint64_t finger, Vec2F const& pos, bool canceled = false) {
    auto ptr = m_fingers.ptr(finger);
    if (!ptr)
      return;
    if (ptr->role == FingerRole::AimJoystick && !canceled)
      updateAimJoystickFinger(*ptr, pos);

    bool tap = !canceled && isTap(*ptr, pos);

    switch (ptr->role) {
      case FingerRole::Joystick:
        if (tap && !m_primaryHeld && !m_secondaryHeld && !m_aimAttackHeld
            && m_moveVec.magnitudeSquared() <= m_config.deadzone * m_config.deadzone)
          emitMouseClick(pos);
        m_joystickActive = false;
        m_joystickElementId.clear();
        m_moveVec = {};
        m_joystickFinger = 0;
        break;
      case FingerRole::AimJoystick: {
        setMouseOwner("aimJoystick:attack", m_aimSecondary ? MouseButton::Right : MouseButton::Left, false);
        m_aimAttackHeld = false;
        if (m_aimAttack.release(m_config.aimAttackMode, canceled)) {
          updateVirtualAimTarget();
          startPulsedAction(mouseAction(m_aimSecondary ? MouseButton::Right : MouseButton::Left), "aimJoystick:release");
        }
        m_aimJoystickActive = false;
        m_aimJoystickElementId.clear();
        m_aimJoystickFinger = 0;
        m_aimVec = {};
        break;
      }
      case FingerRole::Aim:
        if (m_primaryMouseHeld) {
          // The cursor position was already kept in sync via emitMouseMove
          // during updateFinger, so releasing here reads that tracked
          // position rather than the raw release pos (in touchpad mode they
          // can differ significantly -- cursor position is delta-tracked,
          // not 1:1 with the finger).
          releaseDirectGestureAction(m_config.directTouchSingleAction, DirectTouchSingleOwner);
          m_primaryMouseHeld = false;
        }
        m_primaryHeld = false;
        m_aimFinger = 0;
        break;
      case FingerRole::SuppressedTap:
        if (tap)
          emitMouseClick(pos);
        break;
      case FingerRole::SecondaryHold:
        if (m_secondaryMouseHeld && m_secondaryFinger == finger) {
          m_secondaryTouchPos = m_primaryHeld ? m_primaryTouchPos : pos;
          emitMouseMove(m_secondaryTouchPos);
          releaseDirectGestureAction(m_config.directTouchTwoFingerAction, DirectTouchTwoFingerOwner);
          m_secondaryMouseHeld = false;
        } else if (tap) {
          Vec2F target = m_primaryHeld ? m_primaryTouchPos : pos;
          emitMouseMove(target);
          emitMouseClick(target, MouseButton::Right);
        }
        m_secondaryHeld = false;
        m_secondaryFinger = 0;

        if (m_primaryPausedForSecondary && m_primaryHeld && !m_primaryMouseHeld) {
          emitMouseMove(m_primaryTouchPos);
          pressDirectGestureAction(m_config.directTouchSingleAction, DirectTouchSingleOwner);
          m_primaryMouseHeld = true;
        }
        m_primaryPausedForSecondary = false;
        break;
      case FingerRole::ActionButton:
        if (m_touchWheelActive && m_touchWheelFinger == finger)
          closeTouchWheel(!canceled);
        releaseActionButton(*ptr);
        break;
      case FingerRole::DPad:
        clearDPad(ptr->elementId);
        break;
      default:
        break;
    }

    auto releasedElement = findElement(ptr->elementId);
    if (releasedElement && touchElementInPanel(*releasedElement,m_config)) m_drawer.close();
    m_fingers.remove(finger);
  }

  void cancelDirectTouchGestures() {
    if (m_primaryMouseHeld) {
      releaseDirectGestureAction(m_config.directTouchSingleAction, DirectTouchSingleOwner);
      m_primaryMouseHeld = false;
    }
    if (m_secondaryMouseHeld) {
      releaseDirectGestureAction(m_config.directTouchTwoFingerAction, DirectTouchTwoFingerOwner);
      m_secondaryMouseHeld = false;
    }

    m_primaryHeld = false;
    m_aimFinger = 0;
    m_primaryPausedForSecondary = false;
    m_secondaryHeld = false;
    m_secondaryFinger = 0;

    for (auto& pair : m_fingers)
      if (pair.second.role == FingerRole::Aim || pair.second.role == FingerRole::SecondaryHold || pair.second.role == FingerRole::SuppressedTap)
        pair.second.role = FingerRole::None;
  }

  MobileTouchAction const& dpadAction(MobileTouchElement const& element, String const& direction) const {
    if (direction == "up")
      return element.upAction;
    if (direction == "down")
      return element.downAction;
    if (direction == "left")
      return element.leftAction;
    if (direction == "right")
      return element.rightAction;
    static MobileTouchAction const empty;
    return empty;
  }

  String dpadDirection(MobileTouchElement const& element, Vec2F const& pos) const {
    Vec2F delta = pos - elementCenter(element);
    float radius = controlRadius() * std::clamp(element.size, 0.45f, 2.4f);
    if (delta.magnitude() < radius * 0.25f)
      return {};
    if (std::abs(delta[0]) > std::abs(delta[1]))
      return delta[0] > 0.0f ? "right" : "left";
    return delta[1] > 0.0f ? "down" : "up";
  }

  void updateDPadFinger(FingerState& state, Vec2F const& pos) {
    auto element = findElement(state.elementId);
    if (!element)
      return;

    String direction = dpadDirection(*element, pos);
    if (direction == state.lastDpadDirection)
      return;
    state.lastDpadDirection = direction;
    String activeId = direction.empty() ? String() : state.elementId + ":" + direction;
    for (auto const& candidate : {"up", "down", "left", "right"}) {
      String id = state.elementId + ":" + candidate;
      if (id != activeId && m_dpadHeld.contains(id)) {
        m_dpadHeld.remove(id);
        setAction(dpadAction(*element, candidate), id, false);
      }
    }

    if (!activeId.empty() && !m_dpadHeld.contains(activeId)) {
      m_dpadHeld.add(activeId);
      MobileTouchAction action = dpadAction(*element, direction);
      setAction(action, activeId, true);
      if (action.kind == MobileTouchActionKind::MouseWheelUp || action.kind == MobileTouchActionKind::MouseWheelDown)
        m_nextDPadWheelMs[activeId] = Time::monotonicMilliseconds() + 180;
    }
  }

  void updateAimJoystickFinger(FingerState& state, Vec2F const& pos) {
    state.currentPos = pos;
    float radius = controlRadius();
    if (auto element = findElement(state.elementId))
      radius = elementRadius(*element);

    Vec2F delta = pos - m_aimJoystickOrigin;
    float mag = delta.magnitude();
    if (mag > radius)
      delta = delta / mag * radius;

    m_aimJoystickCurrent = m_aimJoystickOrigin + delta;
    auto value = TouchInput::stick(delta[0] / radius, delta[1] / radius, m_config.aimDeadzone, m_config.aimCurve);
    m_aimVec = {value[0], value[1]};
    m_aimAttack.update(m_config.aimAttackMode, m_aimVec.magnitudeSquared() > 0.0001f);
  }

  void clearDPad(String const& elementId) {
    if (auto element = findElement(elementId)) {
      for (auto const& candidate : {"up", "down", "left", "right"}) {
        String id = elementId + ":" + candidate;
        if (m_dpadHeld.contains(id)) {
          m_dpadHeld.remove(id);
          setAction(dpadAction(*element, candidate), id, false);
        }
      }
    }
  }

  void repeatDPadWheelActions() {
    int64_t now = Time::monotonicMilliseconds();
    for (size_t i = 0; i < m_elements.size(); ++i) {
      if (m_elements[i].kind != MobileTouchElementKind::DPad)
        continue;
      auto const& element = m_elements[i];
      MobileTouchAction const* actions[] = {&element.upAction, &element.downAction, &element.leftAction, &element.rightAction};
      for (size_t direction = 0; direction < 4; ++direction) {
        auto const& held = m_overlayCache[i].owners[direction];
        if (!m_dpadHeld.contains(held) || m_nextDPadWheelMs.value(held, 0) > now)
          continue;
        auto const& action = *actions[direction];
        if (action.kind == MobileTouchActionKind::MouseWheelUp || action.kind == MobileTouchActionKind::MouseWheelDown) {
          setAction(action, held, true);
          m_nextDPadWheelMs[held] = now + 130;
        }
      }
    }
  }

  void emitActionEdges() {
    updateJoystickKeys();
    updateVirtualAimTarget();
    bool attack = m_aimJoystickActive && m_aimAttack.update(m_config.aimAttackMode, m_aimVec.magnitudeSquared() > 0.0001f);
    if (attack != m_aimAttackHeld) {
      setMouseOwner("aimJoystick:attack", m_aimSecondary ? MouseButton::Right : MouseButton::Left, attack);
      m_aimAttackHeld = attack;
    }
    releaseExpiredPulsedActions();
    processMacroEvents();
    repeatActionButtons();
    repeatDPadWheelActions();

    if (m_primaryHeld && !m_primaryMouseHeld && !m_secondaryMouseHeld) {
      m_primaryMouseHeld = true;
      emitMouseMove(m_primaryTouchPos);
      emitMouseDown(m_primaryTouchPos);
    } else if (!m_primaryHeld && m_primaryMouseHeld) {
      m_primaryMouseHeld = false;
      emitMouseUp(m_primaryTouchPos);
    }
  }

  // Turns the movement stick into W/A/S/D. Two things make it feel much
  // better for a platformer than plain thresholds:
  //  - hysteresis: a direction turns on at 0.30 but only off at 0.20, so a
  //    thumb hovering at the edge does not make the key flicker;
  //  - axis lock: when the stick is mostly horizontal, small vertical drift
  //    is ignored (no accidental crouch/climb/jump), and when it is mostly
  //    vertical, small horizontal drift is ignored (stay on the ladder/rope).
  void updateJoystickKeys() {
    TouchInput::Movement movement{m_joyRight, m_joyLeft, m_joyUp, m_joyDown};
    movement.update(m_moveVec[0], m_moveVec[1]);
    if (m_joyRight != movement.right) setKeyOwner("joystick:right", Key::D, movement.right);
    if (m_joyLeft != movement.left) setKeyOwner("joystick:left", Key::A, movement.left);
    if (m_joyUp != movement.up) setKeyOwner("joystick:up", Key::W, movement.up);
    if (m_joyDown != movement.down) setKeyOwner("joystick:down", Key::S, movement.down);
    m_joyRight = movement.right; m_joyLeft = movement.left;
    m_joyUp = movement.up; m_joyDown = movement.down;
  }

  void ensureAimTarget() {
    if (m_hasAimJoystickTarget)
      return;

    Vec2F canvas = canvasSize();
    if (m_hasCursorInputPosition)
      m_aimJoystickTarget = toScreenSpace(m_cursorInputPosition);
    else
      m_aimJoystickTarget = {canvas[0] * 0.5f, canvas[1] * 0.5f};
    m_hasAimJoystickTarget = true;
  }

  bool aimJoystickPrecise() const {
    if (auto element = findElement(m_aimJoystickElementId))
      return element->preciseAim;
    return false;
  }

  bool directionalAimActive() const {
    return m_aimJoystickActive && !aimJoystickPrecise();
  }

  float directionalAimRadius() const {
    Vec2F canvas = inputCanvasSize();
    return std::max(1.0f, std::min(canvas[0], canvas[1]) * 0.32f);
  }

  void emitDirectionalAim(Vec2F const& screenDirection, float deflection, bool cursorVisible) {
    Vec2F inputDirection{screenDirection[0], -screenDirection[1]};
    emitEvent(DirectionalAimEvent{inputDirection * deflection, directionalAimRadius(), cursorVisible, false, InputSource::Touch});
  }

  Vec2F screenGyroVelocity() const {
    float gx = m_gyroInput[0];
    float gy = m_gyroInput[1];

    switch (m_gyroOrientation) {
      case SDL_ORIENTATION_LANDSCAPE:
        return {-gy, gx};
      case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        return {gy, -gx};
      case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        return {-gx, -gy};
      case SDL_ORIENTATION_PORTRAIT:
      case SDL_ORIENTATION_UNKNOWN:
      default:
        return {gx, gy};
    }
  }

  Vec2F gyroAimDelta(int64_t now) {
    if (!m_config.gyroEnabled || !m_gyroRuntimeEnabled || !m_hasGyroInput || now - m_lastGyroInputMs > 90)
      return {};
    if (m_primaryHeld || m_secondaryHeld) {
      m_lastGyroFrameMs = now;
      return {};
    }

    if (m_lastGyroFrameMs == 0) {
      m_lastGyroFrameMs = now;
      return {};
    }

    float dt = std::clamp((float)(now - m_lastGyroFrameMs) / 1000.0f, 0.0f, 0.05f);
    m_lastGyroFrameMs = now;
    if (dt <= 0.0f)
      return {};

    Vec2F omega = screenGyroVelocity();
    float deadzone = 0.015f;
    if (std::abs(omega[0]) < deadzone)
      omega[0] = 0.0f;
    if (std::abs(omega[1]) < deadzone)
      omega[1] = 0.0f;
    if (omega.magnitudeSquared() <= 0.0001f)
      return {};

    float pixelsPerRadian = controlRadius() * 5.0f * std::clamp(m_config.gyroSensitivity, 0.10f, 12.0f);
    Vec2F delta{-omega[1] * pixelsPerRadian * dt, omega[0] * pixelsPerRadian * dt};
    if (m_config.gyroInvertX)
      delta[0] = -delta[0];
    if (m_config.gyroInvertY)
      delta[1] = -delta[1];
    return delta;
  }

  void updateVirtualAimTarget() {
    int64_t now = Time::monotonicMilliseconds();
    float dt = m_lastAimFrameMs ? std::clamp((now - m_lastAimFrameMs) / 1000.0f, 0.0f, 0.05f) : 0.0f;
    m_lastAimFrameMs = now;
    Vec2F delta;

    if (m_aimJoystickActive && m_aimVec.magnitudeSquared() > 0.0001f) {
      float sensitivity = 1.0f;
      if (auto element = findElement(m_aimJoystickElementId))
        sensitivity = std::clamp(element->aimSensitivity, 0.25f, 4.0f);
      if (!aimJoystickPrecise()) {
        float deflection = std::clamp(m_aimVec.magnitude(), 0.0f, 1.0f);
        Vec2F direction = m_aimVec / std::max(0.0001f, m_aimVec.magnitude());
        emitDirectionalAim(direction, deflection, m_config.joystickAimCursorEnabled);
        return;
      } else {
        float speed = TouchInput::aimTravel(controlRadius(), sensitivity, dt);
        delta += m_aimVec * speed;
      }
    }

    delta += gyroAimDelta(now);
    if (delta.magnitudeSquared() <= 0.0001f)
      return;

    ensureAimTarget();
    Vec2F canvas = canvasSize();
    m_aimJoystickTarget += delta;
    m_aimJoystickTarget[0] = std::clamp(m_aimJoystickTarget[0], 0.0f, canvas[0]);
    m_aimJoystickTarget[1] = std::clamp(m_aimJoystickTarget[1], 0.0f, canvas[1]);
    m_hasAimJoystickTarget = true;
    syncVirtualAimCursor(true, m_config.joystickAimCursorEnabled);
  }

  void repeatActionButtons() {
    int64_t now = Time::monotonicMilliseconds();
    for (auto const& pair : m_fingers) {
      auto const& state = pair.second;
      if (state.role != FingerRole::ActionButton || state.pressMode != MobileTouchPressMode::Repeat)
        continue;
      auto next = m_nextActionRepeatMs.value(state.elementId, 0);
      if (next > now)
        continue;
      startPulsedAction(state.action, state.elementId);
      m_nextActionRepeatMs[state.elementId] = now + 110;
    }
  }

  void pressActionButton(MobileTouchElement const& element) {
    m_heldElements.add(element.id);
    if (element.action.kind == MobileTouchActionKind::QuickSlot) {
      setAction(element.action, element.id, true);
      return;
    }
    if (element.action.kind == MobileTouchActionKind::MiningModeToggle) {
      emitEvent(MiningModeEvent{});
      return;
    }
    if (element.action.kind == MobileTouchActionKind::TargetLockToggle || element.action.kind == MobileTouchActionKind::TargetLockNext) {
      emitEvent(TargetLockEvent{element.action.kind == MobileTouchActionKind::TargetLockNext ? TargetLockCommand::Next : TargetLockCommand::Toggle});
      return;
    }
    if (element.action.kind == MobileTouchActionKind::TouchHandSwap) {
      setMouseOwner("aimJoystick:attack", m_aimSecondary ? MouseButton::Right : MouseButton::Left, false);
      m_aimAttackHeld = false;
      m_aimSecondary = !m_aimSecondary;
      return;
    }
    if (element.action.kind == MobileTouchActionKind::GyroToggle) {
      if (m_gyroAvailable && m_config.gyroEnabled) {
        m_gyroRuntimeEnabled = !m_gyroRuntimeEnabled;
        m_lastGyroFrameMs = 0;
        if (m_gyroRuntimeEnabled)
          ensureAimTarget();
      }
      return;
    }

    if (element.pressMode == MobileTouchPressMode::Toggle) {
      if (m_toggledElements.contains(element.id)) {
        m_toggledElements.remove(element.id);
        setAction(element.action, element.id, false);
      } else {
        m_toggledElements.add(element.id);
        setAction(element.action, element.id, true);
      }
    } else if (element.pressMode == MobileTouchPressMode::SinglePress) {
      startPulsedAction(element.action, element.id);
    } else if (element.pressMode == MobileTouchPressMode::Repeat) {
      startPulsedAction(element.action, element.id);
      m_nextActionRepeatMs[element.id] = Time::monotonicMilliseconds() + 110;
    } else {
      setAction(element.action, element.id, true);
    }
  }

  static bool isWheelAction(MobileTouchAction const& action) {
    return action.kind == MobileTouchActionKind::ActionWheel || action.kind == MobileTouchActionKind::InventoryWheel;
  }

  // --- Touch radial wheels -----------------------------------------------------
  // The game's wheel UI (MainInterface::handleActionWheelEvent) takes a
  // direction with +Y up, like a gamepad stick. Here the "stick" is the finger's
  // drag from where it landed, so the wheel is picked by sliding the thumb.
  void openTouchWheel(uint64_t finger, Vec2F const& pos, MobileTouchActionKind kind) {
    if (m_touchWheelActive)
      return;
    m_touchWheelActive = true;
    m_touchWheelFinger = finger;
    m_touchWheelAnchor = pos;
    m_touchWheelDirection = {};
    m_touchWheelType = kind == MobileTouchActionKind::InventoryWheel ? ActionWheelType::Inventory : ActionWheelType::Actions;
    emitEvent(ActionWheelEvent{true, false, false, m_touchWheelDirection, m_touchWheelType, true});
  }

  void updateTouchWheel(Vec2F const& pos) {
    float reach = std::max(1.0f, controlRadius() * 1.25f);
    Vec2F delta = pos - m_touchWheelAnchor;
    auto direction = TouchInput::wheel(delta[0], delta[1], reach);
    m_touchWheelDirection = {direction[0], direction[1]};
    emitEvent(ActionWheelEvent{false, false, false, m_touchWheelDirection, m_touchWheelType, true});
  }

  void closeTouchWheel(bool confirm) {
    if (!m_touchWheelActive)
      return;
    // Releasing without dragging out of the center cancels instead of
    // picking the first entry by accident.
    bool picked = confirm && m_touchWheelDirection.magnitudeSquared() >= 0.25f;
    emitEvent(ActionWheelEvent{false, picked, !picked, m_touchWheelDirection, m_touchWheelType, true});
    m_touchWheelActive = false;
    m_touchWheelFinger = 0;
    m_touchWheelDirection = {};
  }

  bool isEdgeTouch(Vec2F const& pos) const {
    float w = (float)(*m_windowSize)[0];
    float h = (float)(*m_windowSize)[1];
    return TouchInput::edge(pos[0], pos[1], w, h);
  }

  void releaseActionButton(FingerState const& state) {
    m_heldElements.remove(state.elementId);
    if (state.pressMode == MobileTouchPressMode::Hold)
      setAction(state.action, state.elementId, false);
    else if (state.pressMode == MobileTouchPressMode::Repeat) {
      m_nextActionRepeatMs.remove(state.elementId);
      cancelPulsedAction(state.elementId);
    }
  }

  // Owner tokens for the two direct-touch gestures (single-finger "Aim",
  // two-finger "SecondaryHold"): distinct from any element id since these
  // gestures aren't backed by a configured MobileTouchElement.
  static constexpr char const* DirectTouchSingleOwner = "directTouchSingle";
  static constexpr char const* DirectTouchTwoFingerOwner = "directTouchTwoFinger";

  // Press/release for the two configurable direct-touch gesture actions.
  // Mirrors pressActionButton/releaseActionButton's Hold-mode + GyroToggle
  // special case, but takes the action directly since there's no backing
  // element -- these gestures are always "press on finger-down, release on
  // finger-up" (no press-mode picker; that matches how they behaved before
  // being made configurable).
  void pressDirectGestureAction(MobileTouchAction const& action, char const* owner) {
    if (action.kind == MobileTouchActionKind::GyroToggle) {
      if (m_gyroAvailable && m_config.gyroEnabled) {
        m_gyroRuntimeEnabled = !m_gyroRuntimeEnabled;
        m_lastGyroFrameMs = 0;
        if (m_gyroRuntimeEnabled)
          ensureAimTarget();
      }
      return;
    }
    setAction(action, owner, true);
  }

  void releaseDirectGestureAction(MobileTouchAction const& action, char const* owner) {
    if (action.kind == MobileTouchActionKind::GyroToggle)
      return;
    setAction(action, owner, false);
  }

  void setKeyOwner(String const& owner, Key key, bool desired) {
    String token = owner + ":" + KeyNames.getRight(key);

    if (desired && !m_keyActionOwners.contains(token)) {
      m_keyActionOwners.add(token);
      unsigned count = m_keyHoldCounts.value(key, 0);
      m_keyHoldCounts.set(key, count + 1);
      if (count == 0)
        emitEvent(KeyDownEvent{key, noMods(), InputSource::Touch});
    } else if (!desired && m_keyActionOwners.contains(token)) {
      m_keyActionOwners.remove(token);
      unsigned count = m_keyHoldCounts.value(key, 0);
      if (count <= 1) {
        m_keyHoldCounts.remove(key);
        emitEvent(KeyUpEvent{key, InputSource::Touch});
      } else {
        m_keyHoldCounts.set(key, count - 1);
      }
    }
  }

  void setMouseOwner(String const& owner, MouseButton button, bool desired) {
    String token = owner + ":" + MouseButtonNames.getRight(button);

    if (desired && !m_mouseActionOwners.contains(token)) {
      m_mouseActionOwners.add(token);
      unsigned count = m_mouseHoldCounts.value(button, 0);
      m_mouseHoldCounts.set(button, count + 1);
      if (count == 0) {
        if (directionalAimActive())
          updateVirtualAimTarget();
        else
          syncVirtualAimCursor(false, m_config.joystickAimCursorEnabled);
        emitEvent(MouseButtonDownEvent{button, mouseActionPosition(), InputSource::Touch});
      }
    } else if (!desired && m_mouseActionOwners.contains(token)) {
      m_mouseActionOwners.remove(token);
      unsigned count = m_mouseHoldCounts.value(button, 0);
      if (count <= 1) {
        m_mouseHoldCounts.remove(button);
        if (directionalAimActive())
          updateVirtualAimTarget();
        else
          syncVirtualAimCursor(false, m_config.joystickAimCursorEnabled);
        emitEvent(MouseButtonUpEvent{button, mouseActionPosition(), InputSource::Touch});
      } else {
        m_mouseHoldCounts.set(button, count - 1);
      }
    }
  }

  void setAction(MobileTouchAction const& action, String const& owner, bool desired) {
    if (action.kind == MobileTouchActionKind::QuickSlot) {
      if (desired && !m_quickSlotOwners.contains(owner)) {
        m_quickSlotOwners[owner] = action;
        emitEvent(QuickSlotEvent{(unsigned)action.quickSlot, true, action.quickUse, owner});
      } else if (!desired && m_quickSlotOwners.contains(owner)) {
        m_quickSlotOwners.remove(owner);
        emitEvent(QuickSlotEvent{(unsigned)action.quickSlot, false, action.quickUse, owner});
      }
    } else if (action.kind == MobileTouchActionKind::MorePanel) {
      if (desired) m_drawer.next(m_panelPages,Time::monotonicMilliseconds()/1000.0);
    } else if (action.kind == MobileTouchActionKind::MiningModeToggle) {
      if (desired) emitEvent(MiningModeEvent{});
    } else if (action.kind == MobileTouchActionKind::TargetLockToggle || action.kind == MobileTouchActionKind::TargetLockNext) {
      if (desired) emitEvent(TargetLockEvent{action.kind == MobileTouchActionKind::TargetLockNext ? TargetLockCommand::Next : TargetLockCommand::Toggle});
    } else if (action.kind == MobileTouchActionKind::Key) {
      setKeyOwner(owner, action.key, desired);
    } else if (action.kind == MobileTouchActionKind::KeyMacro) {
      if (action.macroSequential) {
        // Sequence macro: on trigger, schedule an ordered run of key taps.
        // Releasing is a no-op â€” the run completes on its own timeline.
        if (desired && !action.keys.empty() && !hasActiveMacro(owner))
          enqueueMacro(action.keys, owner);
      } else {
        // Chord macro: hold every key down together for the button's lifetime.
        for (auto key : action.keys)
          setKeyOwner(owner, key, desired);
      }
    } else if (action.kind == MobileTouchActionKind::MouseButton) {
      setMouseOwner(owner, action.mouseButton, desired);
    } else if (desired && action.kind == MobileTouchActionKind::MouseWheelUp) {
      syncVirtualAimCursor(false, m_config.joystickAimCursorEnabled);
      emitEvent(MouseWheelEvent{MouseWheel::Up, mouseActionPosition(), InputSource::Touch});
    } else if (desired && action.kind == MobileTouchActionKind::MouseWheelDown) {
      syncVirtualAimCursor(false, m_config.joystickAimCursorEnabled);
      emitEvent(MouseWheelEvent{MouseWheel::Down, mouseActionPosition(), InputSource::Touch});
    }
  }

  bool actionNeedsRelease(MobileTouchAction const& action) const {
    // Sequence macros self-terminate, so they don't need a paired release.
    if (action.kind == MobileTouchActionKind::KeyMacro)
      return !action.macroSequential;
    return action.kind == MobileTouchActionKind::Key || action.kind == MobileTouchActionKind::MouseButton;
  }

  // --- Sequential macro playback ---------------------------------------------
  // A macro run is expanded into a list of timed key up/down events. Each key in
  // the sequence is pressed for MacroKeyHoldMs then released, with MacroKeyGapMs
  // before the next key. Every key instance gets a unique owner token so it plays
  // through setKeyOwner's reference counting and coexists with other held keys.
  static constexpr int64_t MacroKeyHoldMs = 45;
  static constexpr int64_t MacroKeyGapMs = 55;

  int64_t macroRunDurationMs(size_t keyCount) const {
    return (int64_t)keyCount * (MacroKeyHoldMs + MacroKeyGapMs);
  }

  bool hasActiveMacro(String const& owner) const {
    String prefix = owner + "#macro";
    for (auto const& event : m_macroEvents) {
      if (event.ownerToken.beginsWith(prefix))
        return true;
    }
    return false;
  }

  void enqueueMacro(List<Key> const& keys, String const& owner) {
    int64_t base = Time::monotonicMilliseconds();
    int64_t instance = ++m_macroInstanceCounter;
    int64_t cursor = base;
    size_t step = 0;
    for (auto key : keys) {
      String token = strf("{}#macro{}#{}", owner, instance, step);
      m_macroEvents.append(MacroKeyEvent{key, true, cursor, token});
      m_macroEvents.append(MacroKeyEvent{key, false, cursor + MacroKeyHoldMs, token});
      cursor += MacroKeyHoldMs + MacroKeyGapMs;
      ++step;
    }
  }

  void processMacroEvents() {
    if (m_macroEvents.empty())
      return;

    int64_t now = Time::monotonicMilliseconds();
    for (auto it = m_macroEvents.begin(); it != m_macroEvents.end();) {
      if (it->atMs <= now) {
        setKeyOwner(it->ownerToken, it->key, it->down);
        it = m_macroEvents.erase(it);
      } else {
        ++it;
      }
    }
  }

  void startPulsedAction(MobileTouchAction const& action, String const& owner) {
    setAction(action, owner, true);
    if (!actionNeedsRelease(action))
      return;

    m_pulsedActions[owner] = action;
    m_pulsedActionReleaseMs[owner] = Time::monotonicMilliseconds() + 55;
  }

  void cancelPulsedAction(String const& owner) {
    if (auto action = m_pulsedActions.ptr(owner))
      setAction(*action, owner, false);
    m_pulsedActions.remove(owner);
    m_pulsedActionReleaseMs.remove(owner);
  }

  void releaseExpiredPulsedActions() {
    int64_t now = Time::monotonicMilliseconds();
    for (auto it = m_pulsedActionReleaseMs.begin(); it != m_pulsedActionReleaseMs.end();) {
      if (it->second <= now) {
        if (auto action = m_pulsedActions.ptr(it->first))
          setAction(*action, it->first, false);
        m_pulsedActions.remove(it->first);
        it = m_pulsedActionReleaseMs.erase(it);
      } else {
        ++it;
      }
    }
  }

  void clearPulsedActions() {
    StringList owners;
    for (auto const& pair : m_pulsedActions.pairs())
      owners.append(pair.first);

    for (auto const& owner : owners)
      cancelPulsedAction(owner);
  }

  void syncVirtualAimCursor(bool force = false, bool cursorVisible = true) {
    if (!m_hasAimJoystickTarget)
      return;

    Vec2F inputPosition = toInputSpace(m_aimJoystickTarget);
    if (!force && m_hasCursorInputPosition && (inputPosition - m_cursorInputPosition).magnitudeSquared() <= 0.01f)
      return;

    m_cursorInputPosition = inputPosition;
    m_hasCursorInputPosition = true;
    emitEvent(MouseMoveEvent{{0, 0}, m_cursorInputPosition, cursorVisible, InputSource::Touch});
  }

  Vec2F mouseActionPosition() const {
    return m_hasCursorInputPosition ? m_cursorInputPosition : m_lastPointerInput;
  }

  void emitMouseMove(Vec2F const& pos) {
    m_cursorInputPosition = toInputSpace(pos);
    m_hasCursorInputPosition = true;
    m_aimJoystickTarget = pos;
    m_hasAimJoystickTarget = true;
    emitEvent(MouseMoveEvent{{0, 0}, m_cursorInputPosition, true, InputSource::Touch});
  }

  void emitMouseDown(Vec2F const& pos, MouseButton button = MouseButton::Left) {
    emitEvent(MouseButtonDownEvent{button, toInputSpace(pos), InputSource::Touch});
  }

  void emitMouseUp(Vec2F const& pos, MouseButton button = MouseButton::Left) {
    emitEvent(MouseButtonUpEvent{button, toInputSpace(pos), InputSource::Touch});
  }

  void emitMouseClick(Vec2F const& pos, MouseButton button = MouseButton::Left) {
    emitMouseDown(pos, button);
    emitMouseUp(pos, button);
  }

  void emitEvent(InputEvent const& event) {
    m_generatedEvents.append(event);
  }

  // A deferred label to be drawn (with a contrast outline) after the inversion
  // blended shape pass has completed. Text can't use the inversion blend, so its
  // draw commands must land under ImGui's default blend state.
  enum class ButtonIcon {
    None, Jump, Hand, Pickaxe, Cube, Pause, Chat, Bolt, Shift, Bag, Gyro, ArrowUp, ArrowDown, Wheel, More, Lock, Next, Hands
  };

  // Picks a vector icon from what the button does. Known default ids first,
  // then the bound action, so custom buttons the player adds get a fitting
  // icon too. Anything unrecognized keeps its text label.
  static ButtonIcon buttonIconFor(MobileTouchElement const& element) {
    if (element.action.kind == MobileTouchActionKind::MorePanel) return ButtonIcon::More;
    if (element.action.kind == MobileTouchActionKind::TargetLockToggle) return ButtonIcon::Lock;
    if (element.action.kind == MobileTouchActionKind::TargetLockNext) return ButtonIcon::Next;
    if (element.action.kind == MobileTouchActionKind::MiningModeToggle) return ButtonIcon::Pickaxe;
    if (element.action.kind == MobileTouchActionKind::TouchHandSwap) return ButtonIcon::Hands;
    if (element.id == "crouch") return ButtonIcon::ArrowDown;
    if (element.id == "singleBlock") return ButtonIcon::Cube;
    if (element.id == "jump") return ButtonIcon::Jump;
    if (element.id == "interact") return ButtonIcon::Hand;
    if (element.id == "leftHand") return ButtonIcon::Pickaxe;
    if (element.id == "rightHand") return ButtonIcon::Cube;
    if (element.id == "pause") return ButtonIcon::Pause;
    if (element.id == "chat") return ButtonIcon::Chat;
    if (element.id == "tech") return ButtonIcon::Bolt;
    if (element.id == "shift") return ButtonIcon::Shift;
    if (element.id == "gyroToggle") return ButtonIcon::Gyro;
    if (element.id == "inventoryWheel") return ButtonIcon::Bag;
    if (element.id == "actionWheel") return ButtonIcon::Wheel;

    MobileTouchAction const& a = element.action;
    switch (a.kind) {
      case MobileTouchActionKind::GyroToggle: return ButtonIcon::Gyro;
      case MobileTouchActionKind::InventoryWheel: return ButtonIcon::Bag;
      case MobileTouchActionKind::ActionWheel: return ButtonIcon::Wheel;
      case MobileTouchActionKind::MouseWheelUp: return ButtonIcon::ArrowUp;
      case MobileTouchActionKind::MouseWheelDown: return ButtonIcon::ArrowDown;
      case MobileTouchActionKind::MouseButton:
        return a.mouseButton == MouseButton::Left ? ButtonIcon::Pickaxe
             : a.mouseButton == MouseButton::Right ? ButtonIcon::Cube : ButtonIcon::None;
      case MobileTouchActionKind::Key:
        if (a.key == Key::Space) return ButtonIcon::Jump;
        if (a.key == Key::E) return ButtonIcon::Hand;
        if (a.key == Key::Escape) return ButtonIcon::Pause;
        if (a.key == Key::Return) return ButtonIcon::Chat;
        if (a.key == Key::F) return ButtonIcon::Bolt;
        if (a.key == Key::LShift) return ButtonIcon::Shift;
        if (a.key == Key::I) return ButtonIcon::Bag;
        return ButtonIcon::None;
      default:
        return ButtonIcon::None;
    }
  }

  // Strokes a polyline with plain AddLine segments (independent of the ImGui
  // version's AddPolyline/PathStroke signatures).
  static void strokeLines(ImDrawList* draw, std::initializer_list<ImVec2> points, bool closed, ImU32 col, float t) {
    auto pts=points.begin();
    for (size_t i=0; i+1<points.size(); ++i)
      draw->AddLine(pts[i],pts[i+1],col,t);
    if (closed && points.size()>2) draw->AddLine(pts[points.size()-1],pts[0],col,t);
  }

  static void strokeArc(ImDrawList* draw, ImVec2 c, float rad, float a0, float a1, ImU32 col, float t) {
    int const segments = 12;
    ImVec2 prev(c.x + std::cos(a0) * rad, c.y + std::sin(a0) * rad);
    for (int i = 1; i <= segments; ++i) {
      float a = a0 + (a1 - a0) * (float)i / (float)segments;
      ImVec2 next(c.x + std::cos(a) * rad, c.y + std::sin(a) * rad);
      draw->AddLine(prev, next, col, t);
      prev = next;
    }
  }

  // Draws the icon inside a button circle of the given radius. All shapes use
  // the same difference-blend pass as the rest of the overlay, so they stay
  // visible on any background.
  static void drawButtonIcon(ImDrawList* draw, ImVec2 c, float r, ButtonIcon icon, ImU32 col, float baseThickness) {
    float k = r * 0.70f;
    float t = std::max(baseThickness, r * 0.09f);
    auto P = [&](float x, float y) { return ImVec2(c.x + x * k, c.y + y * k); };

    switch (icon) {
      case ButtonIcon::Jump:
        // Double chevron pointing up.
        strokeLines(draw, {P(-0.75f, 0.05f), P(0.0f, -0.65f), P(0.75f, 0.05f)}, false, col, t);
        strokeLines(draw, {P(-0.75f, 0.65f), P(0.0f, -0.05f), P(0.75f, 0.65f)}, false, col, t);
        break;
      case ButtonIcon::ArrowUp:
        strokeLines(draw, {P(-0.7f, 0.3f), P(0.0f, -0.4f), P(0.7f, 0.3f)}, false, col, t);
        break;
      case ButtonIcon::ArrowDown:
        strokeLines(draw, {P(-0.7f, -0.3f), P(0.0f, 0.4f), P(0.7f, -0.3f)}, false, col, t);
        break;
      case ButtonIcon::Hand:
        // Open hand: palm, four fingers and a thumb.
        draw->AddRect(P(-0.55f, -0.05f), P(0.55f, 0.8f), col, k * 0.25f, 0, t);
        draw->AddLine(P(-0.4f, -0.05f), P(-0.4f, -0.55f), col, t);
        draw->AddLine(P(-0.13f, -0.05f), P(-0.13f, -0.8f), col, t);
        draw->AddLine(P(0.14f, -0.05f), P(0.14f, -0.8f), col, t);
        draw->AddLine(P(0.41f, -0.05f), P(0.41f, -0.55f), col, t);
        draw->AddLine(P(-0.55f, 0.35f), P(-0.9f, 0.05f), col, t);
        break;
      case ButtonIcon::Pickaxe:
        // Mining/attack: curved head on a diagonal handle.
        draw->AddLine(P(-0.75f, 0.8f), P(0.3f, -0.25f), col, t);
        draw->AddBezierQuadratic(P(-0.55f, -0.55f), P(0.6f, -0.95f), P(0.85f, 0.25f), col, t);
        break;
      case ButtonIcon::Cube:
        // Place/use: an isometric block.
        strokeLines(draw, {P(0.0f, -0.85f), P(0.75f, -0.42f), P(0.75f, 0.42f), P(0.0f, 0.85f), P(-0.75f, 0.42f), P(-0.75f, -0.42f)}, true, col, t);
        strokeLines(draw, {P(-0.75f, -0.42f), P(0.0f, 0.0f), P(0.75f, -0.42f)}, false, col, t);
        draw->AddLine(P(0.0f, 0.0f), P(0.0f, 0.85f), col, t);
        break;
      case ButtonIcon::Pause:
        draw->AddLine(P(-0.3f, -0.65f), P(-0.3f, 0.65f), col, t * 1.6f);
        draw->AddLine(P(0.3f, -0.65f), P(0.3f, 0.65f), col, t * 1.6f);
        break;
      case ButtonIcon::Chat:
        // Speech bubble with three dots.
        draw->AddRect(P(-0.85f, -0.65f), P(0.85f, 0.35f), col, k * 0.3f, 0, t);
        strokeLines(draw, {P(-0.35f, 0.35f), P(-0.55f, 0.85f), P(0.05f, 0.35f)}, false, col, t);
        draw->AddCircleFilled(P(-0.4f, -0.15f), t * 0.7f, col, 12);
        draw->AddCircleFilled(P(0.0f, -0.15f), t * 0.7f, col, 12);
        draw->AddCircleFilled(P(0.4f, -0.15f), t * 0.7f, col, 12);
        break;
      case ButtonIcon::Bolt:
        // Tech/ability: lightning bolt.
        strokeLines(draw, {P(0.25f, -0.9f), P(-0.45f, 0.1f), P(0.0f, 0.1f), P(-0.25f, 0.9f), P(0.45f, -0.15f), P(0.0f, -0.15f)}, true, col, t);
        break;
      case ButtonIcon::Shift:
        strokeLines(draw, {P(0.0f, -0.85f), P(0.75f, 0.0f), P(0.32f, 0.0f), P(0.32f, 0.75f), P(-0.32f, 0.75f), P(-0.32f, 0.0f), P(-0.75f, 0.0f)}, true, col, t);
        break;
      case ButtonIcon::Bag:
        // Inventory: backpack with handle and pocket.
        draw->AddRect(P(-0.6f, -0.15f), P(0.6f, 0.85f), col, k * 0.3f, 0, t);
        draw->AddBezierQuadratic(P(-0.35f, -0.15f), P(0.0f, -1.0f), P(0.35f, -0.15f), col, t);
        draw->AddLine(P(-0.6f, 0.35f), P(0.6f, 0.35f), col, t);
        break;
      case ButtonIcon::Gyro:
        // Gyro/tilt: a dot with two orbiting arcs.
        draw->AddCircleFilled(c, k * 0.22f, col, 16);
        strokeArc(draw, c, k * 0.85f, -2.4f, -0.75f, col, t);
        strokeArc(draw, c, k * 0.85f, 0.75f, 2.4f, col, t);
        break;
      case ButtonIcon::Wheel:
        // Radial menu: a ring split into sectors.
        draw->AddCircle(c, k * 0.85f, col, 32, t);
        draw->AddCircleFilled(c, k * 0.18f, col, 12);
        draw->AddLine(P(0.0f, -0.85f), P(0.0f, -0.25f), col, t);
        draw->AddLine(P(0.74f, 0.42f), P(0.22f, 0.12f), col, t);
        draw->AddLine(P(-0.74f, 0.42f), P(-0.22f, 0.12f), col, t);
        break;
      case ButtonIcon::More:
        for (float x : {-.6f,0.f,.6f}) draw->AddCircleFilled(P(x,0),k*.13f,col,8);
        break;
      case ButtonIcon::Lock:
        draw->AddCircle(c,k*.6f,col,16,t);
        draw->AddLine(P(-1,0),P(-.4f,0),col,t); draw->AddLine(P(.4f,0),P(1,0),col,t);
        draw->AddLine(P(0,-1),P(0,-.4f),col,t); draw->AddLine(P(0,.4f),P(0,1),col,t);
        break;
      case ButtonIcon::Next:
        strokeLines(draw,{P(-.7f,-.7f),P(0,0),P(-.7f,.7f)},false,col,t);
        strokeLines(draw,{P(0,-.7f),P(.7f,0),P(0,.7f)},false,col,t);
        break;
      case ButtonIcon::Hands:
        draw->AddLine(P(-.8f,-.3f),P(.8f,-.3f),col,t);
        draw->AddLine(P(-.8f,.3f),P(.8f,.3f),col,t);
        strokeLines(draw,{P(.3f,-.8f),P(.8f,-.3f),P(.3f,.1f)},false,col,t);
        strokeLines(draw,{P(-.3f,-.1f),P(-.8f,.3f),P(-.3f,.8f)},false,col,t);
        break;
      case ButtonIcon::None:
      default:
        break;
    }
  }

  struct OverlayLabel {
    ImVec2 pos;
    char const* text;
    // 0 = default ImGui font size. >0 = scaled label (used by buttons so the
    // text stays readable on high-density phone screens).
    float fontSize = 0.0f;
    float opacity = 1.0f;
  };
  struct OverlayCache {
    std::array<String,4> owners;
    std::array<String,4> directions;
    std::array<char,768> perf{};
    std::array<char,96> fittedLabel{};
    ButtonIcon icon=ButtonIcon::None;
    ImFont* font=nullptr;
    ImVec2 textSize{};
    float radius=-1, baseFontSize=0, fontSize=0;
    int64_t nextPerfMs=0;
  };
  void rebuildOverlayCache() {
    m_geometryDirty=true;
    m_geometry.resize(m_elements.size());
    m_shortcutRendered.resize(m_elements.size());
    m_overlayCache.clear(); m_overlayCache.resize(m_elements.size());
    updateGeometry();
    m_overlayLabels.clear(); m_overlayLabels.reserve(m_elements.size()*4);
    for(size_t i=0;i<m_elements.size();++i) {
      auto& cache=m_overlayCache[i];
      cache.icon=buttonIconFor(m_elements[i]);
      char const* suffix[]={"up","down","left","right"};
      cache.directions={launcherTextStatic("touchElement.dpadUp","W"),launcherTextStatic("touchElement.dpadDown","S"),
          launcherTextStatic("touchElement.dpadLeft","A"),launcherTextStatic("touchElement.dpadRight","D")};
      for(size_t j=0;j<4;++j) cache.owners[j]=m_elements[i].id+":"+suffix[j];
    }
  };


  // Draws text with a solid dark outline behind a light glyph so labels stay
  // legible on any background (light glyph shows on dark scenery, dark outline
  // shows on light scenery) â€” the touch-control analogue of Minecraft's
  // drop-shadowed HUD text.
  static void drawLabelWithOutline(ImDrawList* draw, ImVec2 pos, char const* text, ImU32 color, ImU32 outline, float fontSize = 0.0f) {
    static ImVec2 const offsets[8] = {
      {-1.0f, -1.0f}, {0.0f, -1.0f}, {1.0f, -1.0f},
      {-1.0f,  0.0f},                {1.0f,  0.0f},
      {-1.0f,  1.0f}, {0.0f,  1.0f}, {1.0f,  1.0f}
    };
    if (fontSize > 0.0f) {
      ImFont* font = ImGui::GetFont();
      for (auto const& o : offsets)
        draw->AddText(font, fontSize, ImVec2(pos.x + o.x, pos.y + o.y), outline, text);
      draw->AddText(font, fontSize, pos, color, text);
      return;
    }
    for (auto const& o : offsets)
      draw->AddText(ImVec2(pos.x + o.x, pos.y + o.y), outline, text);
    draw->AddText(pos, color, text);
  }

  static void queueButtonLabel(ImVec2 center, float radius, char const* label, std::vector<OverlayLabel>& labels, OverlayCache& cache) {
    // Size the label to the button and center it, so short ("J") and long
    // ("Shift", "ESC") labels both sit in the middle of the circle.
    float baseSize = ImGui::GetFontSize();
    if(cache.radius != radius || cache.font != ImGui::GetFont() || cache.baseFontSize != baseSize) {
      cache.fontSize=std::max(8.f,radius*.48f);
      auto measure=[&](char const* text) { return ImGui::GetFont()->CalcTextSizeA(cache.fontSize,FLT_MAX,0,text).x; };
      TouchLayout::fitLabel(cache.fittedLabel.data(),cache.fittedLabel.size(),label,radius*1.55f,measure);
      cache.textSize=ImGui::GetFont()->CalcTextSizeA(cache.fontSize,FLT_MAX,0,cache.fittedLabel.data());
      cache.radius=radius; cache.font=ImGui::GetFont(); cache.baseFontSize=baseSize;
    }
    labels.push_back({ImVec2(center.x-cache.textSize.x*.5f,center.y-cache.textSize.y*.5f),cache.fittedLabel.data(),cache.fontSize});
  }

  void drawDPad(ImDrawList* draw, ImVec2 center, float radius, OverlayCache const& cache, ImU32 base, ImU32 fill, float thickness, std::vector<OverlayLabel>& labels) const {
    float arm = radius * 0.42f;
    auto drawArm = [&](size_t direction, ImVec2 c) {
      char const* label=cache.directions[direction].utf8Ptr();
      bool held = m_dpadHeld.contains(cache.owners[direction]);
      draw->AddRect(ImVec2(c.x - arm, c.y - arm), ImVec2(c.x + arm, c.y + arm), base, arm * 0.25f, 0, thickness);
      if (held)
        draw->AddRectFilled(ImVec2(c.x - arm, c.y - arm), ImVec2(c.x + arm, c.y + arm), fill, arm * 0.25f);
      labels.push_back({ImVec2(c.x - arm * 0.22f, c.y - arm * 0.38f), label});
    };

    drawArm(0,ImVec2(center.x,center.y-arm*1.25f));
    drawArm(1,ImVec2(center.x,center.y+arm*1.25f));
    drawArm(2,ImVec2(center.x-arm*1.25f,center.y));
    drawArm(3,ImVec2(center.x+arm*1.25f,center.y));
  }

  std::vector<TouchLayout::Circle> m_geometry;
  bool m_geometryDirty=true;
  TouchLayout::Metrics m_previousMetrics{};
  TouchLayout::Drawer m_previousDrawer;
  TouchLayout::Hud m_hud;
  TouchLayout::Drawer m_drawer;
  size_t m_panelPages=1;
  Vec2U* m_windowSize;
  Vec2U* m_renderCanvasSize = nullptr;
  [[maybe_unused]] float* m_displayScalePtr = nullptr;
  SafeAreaInsets* m_safeAreaPtr = nullptr;
  SafeAreaInsets* m_renderSafeAreaPtr = nullptr;
  MobileTouchConfig m_config;
  std::vector<MobileTouchElement> m_elements = defaultTouchElements();
  std::vector<OverlayCache> m_overlayCache;
  std::vector<OverlayLabel> m_overlayLabels;
  bool m_gyroAvailable = true;
  List<InputEvent> m_generatedEvents;

  StableHashMap<uint64_t, FingerState> m_fingers;
  StringSet m_heldElements;
  StringSet m_toggledElements;
  StringSet m_dpadHeld;
  StringMap<int64_t> m_nextDPadWheelMs;
  StringMap<int64_t> m_nextActionRepeatMs;
  StringMap<MobileTouchAction> m_pulsedActions;
  StringMap<int64_t> m_pulsedActionReleaseMs;
  StringSet m_keyActionOwners;
  HashMap<Key, unsigned> m_keyHoldCounts;
  std::vector<bool> m_shortcutRendered;
  StringMap<MobileTouchAction> m_quickSlotOwners;
  StringSet m_mouseActionOwners;
  HashMap<MouseButton, unsigned> m_mouseHoldCounts;

  struct MacroKeyEvent {
    Key key;
    bool down;
    int64_t atMs;
    String ownerToken;
  };
  List<MacroKeyEvent> m_macroEvents;
  int64_t m_macroInstanceCounter = 0;

  unsigned m_context = 2;
  bool m_aimSecondary = false;
  bool m_aimAttackHeld = false;
  int64_t m_lastAimFrameMs = 0;
  TouchInput::AimAttack m_aimAttack;
  bool m_joystickActive = false;
  uint64_t m_joystickFinger = 0;
  uint64_t m_aimFinger = 0;
  String m_joystickElementId;
  Vec2F m_joystickOrigin;
  Vec2F m_joystickCurrent;
  Vec2F m_moveVec;
  bool m_joyRight = false;
  bool m_joyLeft = false;
  bool m_joyUp = false;
  bool m_joyDown = false;

  bool m_aimJoystickActive = false;
  uint64_t m_aimJoystickFinger = 0;
  String m_aimJoystickElementId;
  Vec2F m_aimJoystickOrigin;
  Vec2F m_aimJoystickCurrent;
  Vec2F m_aimJoystickTarget;
  Vec2F m_aimVec;
  bool m_hasAimJoystickTarget = false;
  std::array<float, 3> m_gyroInput{};
  bool m_hasGyroInput = false;
  bool m_gyroRuntimeEnabled = false;
  int64_t m_lastGyroInputMs = 0;
  int64_t m_lastGyroFrameMs = 0;
  SDL_DisplayOrientation m_gyroOrientation = SDL_ORIENTATION_UNKNOWN;

  bool m_primaryHeld = false;
  bool m_primaryMouseHeld = false;
  int64_t m_lastActivityMs = 0;
  bool m_touchWheelActive = false;
  uint64_t m_touchWheelFinger = 0;
  Vec2F m_touchWheelAnchor;
  Vec2F m_touchWheelDirection;
  ActionWheelType m_touchWheelType = ActionWheelType::Actions;
  Vec2F m_primaryTouchPos;
  bool m_primaryPausedForSecondary = false;

  bool m_secondaryHeld = false;
  bool m_secondaryMouseHeld = false;
  uint64_t m_secondaryFinger = 0;
  Vec2F m_secondaryTouchPos;
  Vec2F m_lastPointerInput;
  Vec2F m_cursorInputPosition;
  bool m_hasCursorInputPosition = false;

};


class MobileGamepadInputAdapter::Impl {
public:
  explicit Impl(Vec2U* renderCanvasSize)
    : m_renderCanvasSize(renderCanvasSize) {
    resetCursor();
  }

  void setConfig(MobileGamepadConfig config) {
    if (!config.enabled)
      cancelAll();
    m_config = config;
  }

  void setBindings(std::vector<MobileGamepadBinding> bindings) {
    cancelAll();
    m_bindings = std::move(bindings);
  }

  void beginFrame() {
    // appendGeneratedEvents drains the queue. Keep releases queued by
    // setBindings/setConfig between frames until the client receives them.
    if (!m_config.enabled)
      cancelAll();
  }

  void endFrame() {
    if (!m_config.enabled)
      return;

    if (m_actionWheelActive) {
      clearStickMovement("gamepad:leftStick");
      clearStickMovement("gamepad:rightStick");
      emitEvent(ActionWheelEvent{false, false, false, actionWheelDirection(), m_actionWheelType});
    } else {
      updateStickMovement("gamepad:leftStick", m_config.leftStick, m_leftStick);
      updateStickMovement("gamepad:rightStick", m_config.rightStick, m_rightStick);
      updateStickAim();
    }
    releaseExpiredPulsedActions();
    processMacroEvents();
    repeatButtons();
  }

  void appendGeneratedEvents(List<InputEvent>& outEvents) {
    outEvents.appendAll(m_generatedEvents);
    m_generatedEvents.clear();
  }

  bool processInputEvent(InputEvent const& event, List<InputEvent>& passthroughEvents) {
    if (!m_config.enabled) {
      passthroughEvents.append(event);
      return false;
    }

    if (auto axis = event.ptr<ControllerAxisEvent>()) {
      updateAxisState(axis->controllerAxis, axis->controllerAxisValue);

      if (axis->controllerAxis == ControllerAxis::LeftX || axis->controllerAxis == ControllerAxis::LeftY
          || axis->controllerAxis == ControllerAxis::RightX || axis->controllerAxis == ControllerAxis::RightY)
        return true;

      if (axis->controllerAxis == ControllerAxis::TriggerLeft) {
        updateTrigger(ControllerButton::TriggerLeft, axis->controllerAxisValue);
        return true;
      }

      if (axis->controllerAxis == ControllerAxis::TriggerRight) {
        updateTrigger(ControllerButton::TriggerRight, axis->controllerAxisValue);
        return true;
      }

      return true;
    }

    if (auto button = event.ptr<ControllerButtonDownEvent>()) {
      pressButton(button->controllerButton);
      return true;
    }

    if (auto button = event.ptr<ControllerButtonUpEvent>()) {
      releaseButton(button->controllerButton);
      return true;
    }

    if (auto move = event.ptr<MouseMoveEvent>()) {
      m_cursorPosition = move->mousePosition;
      m_hasDirectionalAim = false;
    } else if (event.is<DirectionalAimEvent>()) {
      // A touch aim joystick is another pointer owner, just like a mouse.
      m_hasDirectionalAim = false;
    }
    if(auto down=event.ptr<MouseButtonDownEvent>(); down && down->source==InputSource::Mouse)
      m_hasDirectionalAim=false;
    if(auto key=event.ptr<KeyDownEvent>(); key && key->source==InputSource::Mouse)
      m_hasDirectionalAim=false;
    // Preserve ordering when physical pointer/keyboard input follows generated
    // gamepad input in the same SDL poll batch.
    appendGeneratedEvents(passthroughEvents);
    passthroughEvents.append(event);
    return false;
  }

  void cancelAll() {
    if (m_config.enabled)
      emitEvent(TargetLockEvent{TargetLockCommand::Cancel});
    for (auto const& entry : m_quickSlotOwners)
      emitEvent(QuickSlotEvent{(unsigned)entry.second.quickSlot, false, entry.second.quickUse, entry.first});
    m_quickSlotOwners.clear();
    clearPulsedActions();
    closeActionWheel(false);
    m_macroEvents.clear();

    for (auto const& pair : m_keyHoldCounts.pairs()) {
      if (pair.second > 0)
        emitEvent(KeyUpEvent{pair.first, InputSource::Gamepad});
    }
    for (auto const& pair : m_mouseHoldCounts.pairs()) {
      if (pair.second > 0)
        emitEvent(MouseButtonUpEvent{pair.first, m_cursorPosition, InputSource::Gamepad});
    }

    m_heldButtons.clear();
    m_toggledButtons.clear();
    m_nextButtonRepeatMs.clear();
    m_keyActionOwners.clear();
    m_keyHoldCounts.clear();
    m_mouseActionOwners.clear();
    m_mouseHoldCounts.clear();
    m_activeButtons.clear();
    m_triggerStates.clear();
    m_aimReturnFilter = {};
    m_rawLeftStick = {};
    m_rawRightStick = {};
    m_leftStick = {};
    m_rightStick = {};
    m_hasDirectionalAim = false;
    m_lastAimFrameMs = Time::monotonicMilliseconds();
  }

private:
  struct ActiveButton {
    MobileTouchAction action;
    MobileTouchPressMode pressMode = MobileTouchPressMode::SinglePress;
  };

  Vec2F canvasSize() const {
    if (m_renderCanvasSize && (*m_renderCanvasSize)[0] > 0 && (*m_renderCanvasSize)[1] > 0)
      return Vec2F(*m_renderCanvasSize);
    return {1280.0f, 720.0f};
  }

  void resetCursor() {
    Vec2F canvas = canvasSize();
    m_cursorPosition = {canvas[0] * 0.5f, canvas[1] * 0.5f};
    m_lastAimFrameMs = Time::monotonicMilliseconds();
  }

  void updateAxisState(ControllerAxis axis, float value) {
    if (axis == ControllerAxis::LeftX)
      m_rawLeftStick[0] = value;
    else if (axis == ControllerAxis::LeftY)
      m_rawLeftStick[1] = value;
    else if (axis == ControllerAxis::RightX)
      m_rawRightStick[0] = value;
    else if (axis == ControllerAxis::RightY)
      m_rawRightStick[1] = value;

    auto filter = [](Vec2F raw, MobileGamepadStickConfig const& stick) {
      auto v = GamepadInput::radialStick(raw[0], raw[1], stick.deadzone, stick.invertX, stick.invertY);
      return stick.enabled ? Vec2F{v[0], v[1]} : Vec2F{};
    };
    m_leftStick = filter(m_rawLeftStick, m_config.leftStick);
    m_rightStick = filter(m_rawRightStick, m_config.rightStick);
  }

  void updateTrigger(ControllerButton button, float value) {
    bool previous = m_triggerStates.value(button, false);
    bool pressed = GamepadInput::triggerPressed(value, m_config.triggerThreshold, previous);
    m_triggerStates.set(button, pressed);
    if (pressed != previous)
      updateVirtualButton(button, pressed);
  }

  MobileGamepadBinding const* bindingForButton(ControllerButton button) const {
    for (auto const& binding : m_bindings) {
      if (binding.button == button)
        return &binding;
    }
    return nullptr;
  }

  String buttonOwner(ControllerButton button) const {
    return String("gamepad:") + ControllerButtonNames.getRight(button);
  }

  void updateVirtualButton(ControllerButton button, bool desired) {
    if (desired)
      pressButton(button);
    else
      releaseButton(button);
  }

  void pressButton(ControllerButton button) {
    if (button == ControllerButton::Invalid || m_heldButtons.contains(buttonOwner(button)))
      return;
    auto binding = bindingForButton(button);
    if (!binding || !binding->enabled)
      return;

    String owner = buttonOwner(button);
    m_heldButtons.add(owner);
    ActiveButton active{binding->action, binding->pressMode};
    m_activeButtons[owner] = active;

    if (binding->action.kind == MobileTouchActionKind::QuickSlot) {
      m_activeButtons[owner].pressMode = MobileTouchPressMode::Hold;
      setAction(binding->action, owner, true);
      return;
    }
    if (binding->action.kind == MobileTouchActionKind::MiningModeToggle || binding->action.kind == MobileTouchActionKind::TargetLockToggle || binding->action.kind == MobileTouchActionKind::TargetLockNext) {
      m_activeButtons[owner].pressMode = MobileTouchPressMode::SinglePress;
      startPulsedAction(binding->action, owner);
      return;
    }

    if (binding->action.kind == MobileTouchActionKind::GamepadAimModeToggle) {
      startPulsedAction(binding->action, owner);
      return;
    }

    if (binding->action.kind == MobileTouchActionKind::ActionWheel || binding->action.kind == MobileTouchActionKind::InventoryWheel) {
      openActionWheel(owner, binding->action.kind == MobileTouchActionKind::InventoryWheel ? ActionWheelType::Inventory : ActionWheelType::Actions);
      return;
    }

    if (binding->pressMode == MobileTouchPressMode::Toggle) {
      if (m_toggledButtons.contains(owner)) {
        m_toggledButtons.remove(owner);
        setAction(binding->action, owner, false);
      } else {
        m_toggledButtons.add(owner);
        setAction(binding->action, owner, true);
      }
    } else if (binding->pressMode == MobileTouchPressMode::Hold) {
      setAction(binding->action, owner, true);
    } else {
      startPulsedAction(binding->action, owner);
      if (binding->pressMode == MobileTouchPressMode::Repeat)
        m_nextButtonRepeatMs[owner] = Time::monotonicMilliseconds() + 120;
    }
    if (binding->action.kind == MobileTouchActionKind::UiNavigation && binding->pressMode != MobileTouchPressMode::Toggle)
      m_nextButtonRepeatMs[owner] = Time::monotonicMilliseconds() + 350;
  }

  void releaseButton(ControllerButton button) {
    String owner = buttonOwner(button);
    if (!m_heldButtons.contains(owner))
      return;

    m_heldButtons.remove(owner);
    if (auto active = m_activeButtons.ptr(owner)) {
      if (active->action.kind == MobileTouchActionKind::ActionWheel || active->action.kind == MobileTouchActionKind::InventoryWheel) {
        if (owner == m_actionWheelOwner)
          closeActionWheel(true);
      } else if (active->pressMode == MobileTouchPressMode::Hold)
        setAction(active->action, owner, false);
      else if (active->pressMode == MobileTouchPressMode::Repeat) {
        m_nextButtonRepeatMs.remove(owner);
        cancelPulsedAction(owner);
      }
    }
    m_nextButtonRepeatMs.remove(owner);
    m_activeButtons.remove(owner);
  }

  void repeatButtons() {
    int64_t now = Time::monotonicMilliseconds();
    for (auto const& owner : m_heldButtons.values()) {
      auto active = m_activeButtons.ptr(owner);
      if (!active)
        continue;
      bool navigation = active->action.kind == MobileTouchActionKind::UiNavigation
          && active->pressMode != MobileTouchPressMode::Toggle;
      if (!navigation && active->pressMode != MobileTouchPressMode::Repeat)
        continue;
      auto next = m_nextButtonRepeatMs.value(owner, 0);
      if (next > now)
        continue;
      startPulsedAction(active->action, owner);
      m_nextButtonRepeatMs[owner] = now + (navigation ? 100 : 120);
    }
  }

  void clearStickMovement(String const& owner) {
    setKeyOwner(owner + ":right", Key::D, false);
    setKeyOwner(owner + ":left", Key::A, false);
    setKeyOwner(owner + ":up", Key::W, false);
    setKeyOwner(owner + ":down", Key::S, false);
  }

  void updateStickMovement(String const& owner, MobileGamepadStickConfig const& stick, Vec2F const& value) {
    if (!stick.enabled || stick.mode != MobileGamepadStickMode::Movement) {
      clearStickMovement(owner);
      return;
    }

    setKeyOwner(owner + ":right", Key::D, value[0] > 0.30f);
    setKeyOwner(owner + ":left", Key::A, value[0] < -0.30f);
    setKeyOwner(owner + ":up", Key::W, value[1] < -0.30f);
    setKeyOwner(owner + ":down", Key::S, value[1] > 0.30f);
  }

  MobileGamepadStickConfig* activeAimStick(Vec2F& value) {
    if (m_config.rightStick.enabled && m_config.rightStick.mode == MobileGamepadStickMode::Aim && m_rightStick.magnitudeSquared() > 0.0001f) {
      value = {m_rightStick[0], -m_rightStick[1]};
      return &m_config.rightStick;
    }
    if (m_config.leftStick.enabled && m_config.leftStick.mode == MobileGamepadStickMode::Aim && m_leftStick.magnitudeSquared() > 0.0001f) {
      value = {m_leftStick[0], -m_leftStick[1]};
      return &m_config.leftStick;
    }
    return nullptr;
  }

  MobileGamepadStickConfig* aimModeToggleStick() {
    if (m_config.rightStick.mode == MobileGamepadStickMode::Aim)
      return &m_config.rightStick;
    if (m_config.leftStick.mode == MobileGamepadStickMode::Aim)
      return &m_config.leftStick;
    return &m_config.rightStick;
  }

  void updateStickAim() {
    Vec2F stickValue;
    auto stick = activeAimStick(stickValue);
    if (!stick) {
      m_aimReturnFilter.accept(0, Time::monotonicMilliseconds() / 1000.0);
      m_lastAimFrameMs = Time::monotonicMilliseconds();
      // Keep the last target relative to the player while walking/mining.
      // Real mouse input clears this latch, so it retains control at rest.
      if (m_hasDirectionalAim && !m_actionWheelActive)
        emitEvent(m_lastDirectionalAim);
      return;
    }

    if (m_actionWheelActive)
      return;

    Vec2F canvas = canvasSize();
    int64_t now = Time::monotonicMilliseconds();
    float dt = std::clamp((float)(now - m_lastAimFrameMs) / 1000.0f, 0.0f, 0.05f);
    m_lastAimFrameMs = now;

    float magnitude = stickValue.magnitude();
    if (!m_aimReturnFilter.accept(magnitude, now / 1000.0)) {
      if (m_hasDirectionalAim)
        emitEvent(m_lastDirectionalAim);
      return;
    }
    bool precisionHeld = m_keyHoldCounts.value(Key::LShift, 0) > 0
        || m_keyHoldCounts.value(Key::RShift, 0) > 0;
    Vec2F aim = stickValue / magnitude * GamepadInput::aimResponse(magnitude, stick->sensitivity);
    if (stick->preciseAim) {
      m_hasDirectionalAim = false;
      float shortSide = std::max(1.0f, std::min(canvas[0], canvas[1]));
      float speed = shortSide * 1.35f * std::clamp(stick->sensitivity, 0.10f, 5.0f);
      m_cursorPosition += aim * speed * dt * (precisionHeld ? 0.20f : 1.0f);
      m_cursorPosition[0] = std::clamp(m_cursorPosition[0], 0.0f, canvas[0]);
      m_cursorPosition[1] = std::clamp(m_cursorPosition[1], 0.0f, canvas[1]);
      emitEvent(MouseMoveEvent{{0.0f, 0.0f}, m_cursorPosition, m_config.joystickAimCursorEnabled, InputSource::Gamepad});
    } else {
      if (precisionHeld) {
        Vec2F target = m_hasDirectionalAim ? m_lastDirectionalAim.direction : Vec2F{};
        aim = target + aim * dt * 0.40f;
        float length = aim.magnitude();
        if (length > 1.0f)
          aim /= length;
      }
      Vec2F center{canvas[0] * 0.5f, canvas[1] * 0.5f};
      // The screen-space estimate is only a fallback for non-game UI. In
      // gameplay the frontend resolves the real player anchor/tool reach.
      float radius = std::min(canvas[0], canvas[1]) * 0.34f;
      m_cursorPosition = center + aim * radius;
      m_cursorPosition[0] = std::clamp(m_cursorPosition[0], 0.0f, canvas[0]);
      m_cursorPosition[1] = std::clamp(m_cursorPosition[1], 0.0f, canvas[1]);
      m_lastDirectionalAim = DirectionalAimEvent{aim, radius, m_config.joystickAimCursorEnabled, true};
      m_hasDirectionalAim = true;
      emitEvent(m_lastDirectionalAim);
    }
  }

  void setKeyOwner(String const& owner, Key key, bool desired) {
    String token = owner + ":" + KeyNames.getRight(key);
    if (desired && !m_keyActionOwners.contains(token)) {
      m_keyActionOwners.add(token);
      unsigned count = m_keyHoldCounts.value(key, 0);
      m_keyHoldCounts.set(key, count + 1);
      if (count == 0)
        emitEvent(KeyDownEvent{key, noMods(), InputSource::Gamepad});
    } else if (!desired && m_keyActionOwners.contains(token)) {
      m_keyActionOwners.remove(token);
      unsigned count = m_keyHoldCounts.value(key, 0);
      if (count <= 1) {
        m_keyHoldCounts.remove(key);
        emitEvent(KeyUpEvent{key, InputSource::Gamepad});
      } else {
        m_keyHoldCounts.set(key, count - 1);
      }
    }
  }

  void setMouseOwner(String const& owner, MouseButton button, bool desired) {
    String token = owner + ":" + MouseButtonNames.getRight(button);
    if (desired && !m_mouseActionOwners.contains(token)) {
      m_mouseActionOwners.add(token);
      unsigned count = m_mouseHoldCounts.value(button, 0);
      m_mouseHoldCounts.set(button, count + 1);
      if (count == 0)
        updateStickAim();
      if (count == 0)
        emitEvent(MouseButtonDownEvent{button, m_cursorPosition, InputSource::Gamepad});
    } else if (!desired && m_mouseActionOwners.contains(token)) {
      m_mouseActionOwners.remove(token);
      unsigned count = m_mouseHoldCounts.value(button, 0);
      if (count <= 1) {
        m_mouseHoldCounts.remove(button);
        updateStickAim();
        emitEvent(MouseButtonUpEvent{button, m_cursorPosition, InputSource::Gamepad});
      } else {
        m_mouseHoldCounts.set(button, count - 1);
      }
    }
  }

  void setAction(MobileTouchAction const& action, String const& owner, bool desired) {
    if (action.kind == MobileTouchActionKind::QuickSlot) {
      if (desired && !m_quickSlotOwners.contains(owner)) {
        m_quickSlotOwners[owner] = action;
        emitEvent(QuickSlotEvent{(unsigned)action.quickSlot, true, action.quickUse, owner});
      } else if (!desired && m_quickSlotOwners.contains(owner)) {
        m_quickSlotOwners.remove(owner);
        emitEvent(QuickSlotEvent{(unsigned)action.quickSlot, false, action.quickUse, owner});
      }
    } else if (action.kind == MobileTouchActionKind::MiningModeToggle) {
      if (desired) emitEvent(MiningModeEvent{});
    } else if (action.kind == MobileTouchActionKind::TargetLockToggle || action.kind == MobileTouchActionKind::TargetLockNext) {
      if (desired) emitEvent(TargetLockEvent{action.kind == MobileTouchActionKind::TargetLockNext ? TargetLockCommand::Next : TargetLockCommand::Toggle});
    } else if (action.kind == MobileTouchActionKind::Key) {
      setKeyOwner(owner, action.key, desired);
    } else if (action.kind == MobileTouchActionKind::KeyMacro) {
      if (action.macroSequential) {
        if (desired && !action.keys.empty() && !hasActiveMacro(owner))
          enqueueMacro(action.keys, owner);
      } else {
        for (auto key : action.keys)
          setKeyOwner(owner, key, desired);
      }
    } else if (action.kind == MobileTouchActionKind::MouseButton) {
      setMouseOwner(owner, action.mouseButton, desired);
    } else if (desired && action.kind == MobileTouchActionKind::MouseWheelUp) {
      emitEvent(MouseWheelEvent{MouseWheel::Up, m_cursorPosition});
    } else if (desired && action.kind == MobileTouchActionKind::MouseWheelDown) {
      emitEvent(MouseWheelEvent{MouseWheel::Down, m_cursorPosition});
    } else if (desired && action.kind == MobileTouchActionKind::GamepadAimModeToggle) {
      auto stick = aimModeToggleStick();
      stick->preciseAim = !stick->preciseAim;
      m_hasDirectionalAim = false;
    } else if (action.kind == MobileTouchActionKind::ActionWheel || action.kind == MobileTouchActionKind::InventoryWheel) {
      if (desired)
        openActionWheel(owner, action.kind == MobileTouchActionKind::InventoryWheel ? ActionWheelType::Inventory : ActionWheelType::Actions);
      else
        closeActionWheel(true);
    } else if (desired && action.kind == MobileTouchActionKind::UiNavigation) {
      emitEvent(UiNavigationEvent{action.uiNavigationDirection});
    }
  }

  bool actionNeedsRelease(MobileTouchAction const& action) const {
    if (action.kind == MobileTouchActionKind::KeyMacro)
      return !action.macroSequential;
    return action.kind == MobileTouchActionKind::Key || action.kind == MobileTouchActionKind::MouseButton;
  }

  // Sequential macro playback (mirrors the touch adapter). See that adapter for
  // the design notes on timing and per-key owner tokens.
  static constexpr int64_t MacroKeyHoldMs = 45;
  static constexpr int64_t MacroKeyGapMs = 55;

  bool hasActiveMacro(String const& owner) const {
    String prefix = owner + "#macro";
    for (auto const& event : m_macroEvents) {
      if (event.ownerToken.beginsWith(prefix))
        return true;
    }
    return false;
  }

  void enqueueMacro(List<Key> const& keys, String const& owner) {
    int64_t instance = ++m_macroInstanceCounter;
    int64_t cursor = Time::monotonicMilliseconds();
    size_t step = 0;
    for (auto key : keys) {
      String token = strf("{}#macro{}#{}", owner, instance, step);
      m_macroEvents.append(MacroKeyEvent{key, true, cursor, token});
      m_macroEvents.append(MacroKeyEvent{key, false, cursor + MacroKeyHoldMs, token});
      cursor += MacroKeyHoldMs + MacroKeyGapMs;
      ++step;
    }
  }

  void processMacroEvents() {
    if (m_macroEvents.empty())
      return;

    int64_t now = Time::monotonicMilliseconds();
    List<MacroKeyEvent> pending;
    for (auto const& event : m_macroEvents) {
      if (event.atMs <= now)
        setKeyOwner(event.ownerToken, event.key, event.down);
      else
        pending.append(event);
    }
    m_macroEvents = std::move(pending);
  }

  void startPulsedAction(MobileTouchAction const& action, String const& owner) {
    setAction(action, owner, true);
    if (!actionNeedsRelease(action))
      return;
    m_pulsedActions[owner] = action;
    m_pulsedActionReleaseMs[owner] = Time::monotonicMilliseconds() + 55;
  }

  void cancelPulsedAction(String const& owner) {
    if (auto action = m_pulsedActions.ptr(owner))
      setAction(*action, owner, false);
    m_pulsedActions.remove(owner);
    m_pulsedActionReleaseMs.remove(owner);
  }

  void releaseExpiredPulsedActions() {
    int64_t now = Time::monotonicMilliseconds();
    StringList expired;
    for (auto const& pair : m_pulsedActionReleaseMs.pairs()) {
      if (pair.second <= now)
        expired.append(pair.first);
    }
    for (auto const& owner : expired)
      cancelPulsedAction(owner);
  }

  void clearPulsedActions() {
    StringList owners;
    for (auto const& pair : m_pulsedActions.pairs())
      owners.append(pair.first);
    for (auto const& owner : owners)
      cancelPulsedAction(owner);
  }

  Vec2F actionWheelDirection() const {
    if (m_rightStick.magnitudeSquared() >= m_leftStick.magnitudeSquared())
      return m_rightStick;
    return m_leftStick;
  }

  void openActionWheel(String const& owner, ActionWheelType type) {
    if (m_actionWheelActive)
      return;
    m_actionWheelActive = true;
    m_actionWheelOwner = owner;
    m_actionWheelType = type;
    emitEvent(ActionWheelEvent{true, false, false, actionWheelDirection(), m_actionWheelType});
  }

  void closeActionWheel(bool confirm) {
    if (!m_actionWheelActive)
      return;
    emitEvent(ActionWheelEvent{false, confirm, !confirm, actionWheelDirection(), m_actionWheelType});
    m_actionWheelActive = false;
    m_actionWheelOwner = {};
    m_actionWheelType = ActionWheelType::Actions;
  }

  void emitEvent(InputEvent const& event) {
    m_generatedEvents.append(event);
  }

  Vec2U* m_renderCanvasSize = nullptr;
  MobileGamepadConfig m_config;
  std::vector<MobileGamepadBinding> m_bindings = defaultGamepadBindings();
  List<InputEvent> m_generatedEvents;
  Vec2F m_rawLeftStick;
  Vec2F m_rawRightStick;
  Vec2F m_leftStick;
  Vec2F m_rightStick;
  Vec2F m_cursorPosition;
  bool m_hasDirectionalAim = false;
  DirectionalAimEvent m_lastDirectionalAim;
  HashMap<ControllerButton, bool> m_triggerStates;
  GamepadInput::AimReturnFilter m_aimReturnFilter;
  int64_t m_lastAimFrameMs = 0;
  StringSet m_heldButtons;
  StringSet m_toggledButtons;
  bool m_actionWheelActive = false;
  String m_actionWheelOwner;
  ActionWheelType m_actionWheelType = ActionWheelType::Actions;
  StringMap<ActiveButton> m_activeButtons;
  StringMap<int64_t> m_nextButtonRepeatMs;
  StringMap<MobileTouchAction> m_pulsedActions;
  StringMap<int64_t> m_pulsedActionReleaseMs;
  StringSet m_keyActionOwners;
  HashMap<Key, unsigned> m_keyHoldCounts;
  StringMap<MobileTouchAction> m_quickSlotOwners;
  StringSet m_mouseActionOwners;
  HashMap<MouseButton, unsigned> m_mouseHoldCounts;

  struct MacroKeyEvent {
    Key key;
    bool down;
    int64_t atMs;
    String ownerToken;
  };
  List<MacroKeyEvent> m_macroEvents;
  int64_t m_macroInstanceCounter = 0;
};



MobileTouchInputAdapter::MobileTouchInputAdapter(Vec2U* windowSize, Vec2U* renderCanvasSize, float* displayScale, SafeAreaInsets* safeArea, SafeAreaInsets* renderSafeArea)
  : m_impl(make_unique<Impl>(windowSize, renderCanvasSize, displayScale, safeArea, renderSafeArea)) {}

MobileTouchInputAdapter::~MobileTouchInputAdapter() = default;

void MobileTouchInputAdapter::setConfig(MobileTouchConfig config) { m_impl->setConfig(std::move(config)); }
void MobileTouchInputAdapter::setHud(TouchLayout::Hud const& hud) { m_impl->setHud(hud); }
void MobileTouchInputAdapter::setContext(unsigned mode) { m_impl->setContext(mode); }
void MobileTouchInputAdapter::setGyroAvailable(bool available) { m_impl->setGyroAvailable(available); }
void MobileTouchInputAdapter::setElements(std::vector<MobileTouchElement> elements) { m_impl->setElements(std::move(elements)); }
void MobileTouchInputAdapter::beginFrame() { m_impl->beginFrame(); }
void MobileTouchInputAdapter::endFrame() { m_impl->endFrame(); }
void MobileTouchInputAdapter::appendGeneratedEvents(List<InputEvent>& outEvents) { m_impl->appendGeneratedEvents(outEvents); }
bool MobileTouchInputAdapter::gyroSensorRequested() const { return m_impl->gyroSensorRequested(); }
void MobileTouchInputAdapter::setGyroInput(std::array<float, 3> const& data, bool hasData, SDL_DisplayOrientation orientation) { m_impl->setGyroInput(data, hasData, orientation); }
bool MobileTouchInputAdapter::processSdlEvent(SDL_Event const& event) { return m_impl->processSdlEvent(event); }
void MobileTouchInputAdapter::cancelAll() { m_impl->cancelAll(); }
void MobileTouchInputAdapter::renderShortcutIcons(Application& application) { m_impl->renderShortcutIcons(application); }
void MobileTouchInputAdapter::drawOverlay(float fps, bool directionalMining) { m_impl->drawOverlay(fps, directionalMining); }
void MobileTouchInputAdapter::drawPreviewControl(ImDrawList* draw, MobileTouchElement const& element, Vec2F center, float radius, float opacity, bool selected) {
  Impl::drawPreviewControl(draw,element,center,radius,opacity,selected);
}
bool MobileTouchInputAdapter::overlayEnabled() const { return m_impl->overlayEnabled(); }

MobileGamepadInputAdapter::MobileGamepadInputAdapter(Vec2U* renderCanvasSize)
  : m_impl(make_unique<Impl>(renderCanvasSize)) {}

MobileGamepadInputAdapter::~MobileGamepadInputAdapter() = default;

void MobileGamepadInputAdapter::setConfig(MobileGamepadConfig config) { m_impl->setConfig(std::move(config)); }
void MobileGamepadInputAdapter::setBindings(std::vector<MobileGamepadBinding> bindings) { m_impl->setBindings(std::move(bindings)); }
void MobileGamepadInputAdapter::beginFrame() { m_impl->beginFrame(); }
void MobileGamepadInputAdapter::endFrame() { m_impl->endFrame(); }
void MobileGamepadInputAdapter::appendGeneratedEvents(List<InputEvent>& outEvents) { m_impl->appendGeneratedEvents(outEvents); }
bool MobileGamepadInputAdapter::processInputEvent(InputEvent const& event, List<InputEvent>& passthroughEvents) { return m_impl->processInputEvent(event, passthroughEvents); }
void MobileGamepadInputAdapter::cancelAll() { m_impl->cancelAll(); }


}
