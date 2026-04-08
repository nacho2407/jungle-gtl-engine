#pragma once

#include "Splitter.h"

class SSplitterV : public SSplitter
{
public:
	void ArrangeChildren() override;
	void OnMouseMove(int32 X, int32 Y) override;
	FRect GetSplitterBarRect() const override;
	virtual EMouseCursor GetCursor() const override { return EMouseCursor::ResizeUpDown; }
	FVector2 ComputeDesiredSize() const override;
	FVector2 ComputeMinSize() const override;
};