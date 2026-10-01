// UI 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "UI/SWidget.h"

// FPoint는 2D 화면 좌표를 표현합니다.
struct FPoint
{
    float X = 0.0f;
    float Y = 0.0f;
};

// FRect는 UI 배치와 히트 테스트에 사용하는 사각형 영역입니다.
struct FRect
{
    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;
};

// SWindow는 화면 영역을 가진 기본 UI 창 타입입니다.
class SWindow : public SWidget
{
public:
    SWindow() = default;
    virtual ~SWindow() = default;

    bool IsHover(FPoint coord) const;

    void SetRect(const FRect& InRect) { Rect = InRect; }
    const FRect& GetRect() const { return Rect; }

    virtual bool IsSplitter() const { return false; }

protected:
    FRect Rect;
};
