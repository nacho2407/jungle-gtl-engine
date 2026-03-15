#pragma once

#include <vector>
#include <string>
#include "Math/FMatrix.h"

class UStaticMesh;

struct FRenderItem
{
	FMatrix WorldMatrix = FMatrix::Identity;
	UStaticMesh* Mesh = nullptr;
};

class FScene
{
public:
	void Clear();
public:
	std::vector<FRenderItem> RenderItems;
};