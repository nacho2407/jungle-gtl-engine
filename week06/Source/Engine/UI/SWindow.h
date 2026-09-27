#pragma once

#include "UI/SWidget.h"

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

// 기본 창 클래스 — Rect 기반 레이아웃 노드
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
