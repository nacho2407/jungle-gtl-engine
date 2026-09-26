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
	float MinWidth = 0.0f;
	float MinHeight = 0.0f;
	int32 Layer = 0;
	int32 ZOrder = 0;

	FSlot& operator[](SWidget* W) { Widget = W; return *this; }
	FSlot& AutoWidth() { WidthFill = 0.0f; return *this; }
	FSlot& FillWidth(float Ratio) { WidthFill = Ratio; return *this; }
	FSlot& AutoHeight() { HeightFill = 0.0f; return *this; }
	FSlot& FillHeight(float Ratio) { HeightFill = Ratio; return *this; }
	FSlot& Padding(FMargin P) { PaddingInsets = P; return *this; }
	FSlot& HAlign(EHAlign A) { HAlignment = A; return *this; }
	FSlot& VAlign(EVAlign A) { VAlignment = A; return *this; }
	FSlot& SetMinWidth(float InMinWidth) { MinWidth = InMinWidth; return *this; }
	FSlot& SetMinHeight(float InMinHeight) { MinHeight = InMinHeight; return *this; }
	FSlot& SetLayer(int32 InLayer) { Layer = InLayer; return *this; }
	FSlot& SetZOrder(int32 InZOrder) { ZOrder = InZOrder; return *this; }
};
