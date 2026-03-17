#pragma once

#include "Math/FVector.h"

class FApplication;

class FPropertyPanel
{
public:
	FPropertyPanel() = default;
	~FPropertyPanel() = default;

public:
	void Render(FApplication* App);

private:
	bool DrawFloat3Control(const char* Label, FVector& Value, float Speed);
};