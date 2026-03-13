#pragma once

#include <Windows.h>

enum class EKeyState { None, Down, Press, Up };

class InputManager {
 public:
  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;

  static InputManager& GetInstance();
  void Update();

  bool IsKeyDown(int key);
  bool IsKeyHold(int key);
  bool IsKeyUp(int key);

 public:
  bool CurrentState[256];
  bool PrevState[256];

 private:
  InputManager();
  ~InputManager();
};
