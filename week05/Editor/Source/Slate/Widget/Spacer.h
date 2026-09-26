#pragma once

#include "Widget.h"

class SSpacer : public SWidget
{
public:
	SSpacer() = default;
	explicit SSpacer(const FVector2& InDesiredSize) : DesiredSize(InDesiredSize) {}

	void SetDesiredSize(const FVector2& InDesiredSize) { DesiredSize = InDesiredSize; }
	FVector2 ComputeDesiredSize() const override { return DesiredSize; }
	void OnPaint(FSlatePaintContext& Painter) override { (void)Painter; }
	bool OnMouseDown(int32 X, int32 Y) override { (void)X; (void)Y; return false; }

private:
	FVector2 DesiredSize = { 0.0f, 0.0f };
};

