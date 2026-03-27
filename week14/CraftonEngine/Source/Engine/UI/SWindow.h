#pragma once

#include "Core/CoreTypes.h"

struct FPoint
{
	float X = 0.0f;
	float Y = 0.0f;
};

struct FRect
{
	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
};

// 기본 창 클래스 — 모든 UI 위젯의 베이스
class SWindow
{
public:
	SWindow() = default;
	virtual ~SWindow() = default;

	bool IsHover(FPoint coord) const;

	void SetRect(const FRect& InRect) { Rect = InRect; }
	const FRect& GetRect() const { return Rect; }

protected:
	FRect Rect;
};
