// 런타임 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// FWindowsWindow는 에디터 UI 표시와 입력 처리를 담당합니다.
class FWindowsWindow
{
public:
    FWindowsWindow() = default;
    ~FWindowsWindow() = default;

    void Initialize(HWND InHWindow);

    HWND GetHWND() const { return HWindow; }

    float GetWidth() const { return Width; }
    float GetHeight() const { return Height; }

    void OnResized(unsigned int InWidth, unsigned int InHeight);

    /** ScreenToClient 래핑 — 스크린 좌표를 클라이언트 좌표로 변환 */
    POINT ScreenToClientPoint(POINT ScreenPoint) const;

private:
    HWND HWindow = nullptr;
    float Width = 0.f;
    float Height = 0.f;
};
