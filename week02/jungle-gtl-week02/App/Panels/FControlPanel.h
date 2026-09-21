#pragma once

#include "Math/FVector.h"

class FApplication;

class FControlPanel
{
public:
	FControlPanel() = default;
	~FControlPanel() = default;

public:
	void Render(FApplication* App);

private:
	bool DrawFloatControl(const char* Label, float& Value, float Speed);
	bool DrawFloat3Control(const char* Label, FVector& Value, float Speed);
};