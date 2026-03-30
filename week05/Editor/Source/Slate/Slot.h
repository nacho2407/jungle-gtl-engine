#pragma once

#include "Widget.h"

enum class EHAlign { Left, Center, Right, Fill };
enum class EVAlign { Top, Center, Bottom, Fill };
struct FMargin {
	float Left = 0.0f;
	float Top = 0.0f;
	float Right = 0.0f;
	float Bottom = 0.0f;

	FMargin() = default;
	FMargin(float All) : Left(All), Top(All), Right(All), Bottom(All) {}
	FMargin(float H, float V) : Left(H), Top(V), Right(H), Bottom(V) {}
	FMargin(float InLeft, float InTop, float InRight, float InBottom)
		: Left(InLeft), Top(InTop), Right(InRight), Bottom(InBottom) {
	}
};

struct FSlot
{
	SWidget* Widget = nullptr;
	FMargin PaddingInsets;
	EHAlign HAlignment = EHAlign::Fill;
	EVAlign VAlignment = EVAlign::Fill;
	float WidthFill = 0.0f;
	float HeightFill = 0.0f;

	FSlot& operator[](SWidget* W) { Widget = W; return *this; }
	FSlot& AutoHeight() { HeightFill = 0.0f; return *this; }
	FSlot& FillHeight(float Ratio) { HeightFill = Ratio; return *this; }
	FSlot& Padding(FMargin P) { PaddingInsets = P; return *this; }
	FSlot& HAlign(EHAlign A) { HAlignment = A; return *this; }
	FSlot& VAlign(EVAlign A) { VAlignment = A; return *this; }
};