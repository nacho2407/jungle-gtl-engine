#include "InputManager.h"

InputManager& InputManager::GetInstance() {
  static InputManager Instance;
  return Instance;
}

void InputManager::Update() {
  for (int i = 0; i < 256; ++i) {
    PrevState[i] = CurrentState[i];
    CurrentState[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
  }
}

bool InputManager::IsKeyDown(int vk) {
  return CurrentState[vk] && !PrevState[vk];
}

bool InputManager::IsKeyUp(int vk) {
  return !CurrentState[vk] && PrevState[vk];
}

bool InputManager::IsKeyHold(int vk) {
  return CurrentState[vk] && PrevState[vk];
}

InputManager::InputManager() {
  for (int i = 0; i < 256; ++i) {
    PrevState[i] = false;
    CurrentState[i] = false;
  }
}

InputManager::~InputManager() {}