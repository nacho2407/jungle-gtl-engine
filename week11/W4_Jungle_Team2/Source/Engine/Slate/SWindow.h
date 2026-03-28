#pragma once

#include "SlateUtils.h"
#include "SWidget.h"

/**
 * Root UI 영역
 * 자기 자식 하나 또는 여러 개를 통해 UI tree 의 루트 역할
 * Hover 검사
 */
class SWindow : public SWidget
{
public:
	bool IsHover(FPoint Coord) const;

	// Rect Get Set
	FRect GetRect() const { return Rect; }
	void SetRect(FRect InRect) { Rect = InRect; }

	// Child Get Set
	void SetChild(SWidget* InChild) { Child = InChild; }
	const SWidget* GetChild() const { return Child; }

	// Widget Hit Test
	SWidget* HitTest(int X, int Y) override;

private:
	// SWindow 크기
	FRect Rect;
	// SWindow 자식
	SWidget* Child = nullptr;
};

