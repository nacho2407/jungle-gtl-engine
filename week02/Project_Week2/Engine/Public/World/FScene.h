#pragma once

#include <vector>
#include <string>
#include "Math/FMatrix.h"

enum class EPrimitiveType
{
	None,
	Sphere,
	Cube,
	StaticMesh
};

struct FRenderItem
{
	FMatrix WorldMatrix = FMatrix::Identity;
	EPrimitiveType PrimitiveType = EPrimitiveType::None;

	float SphereRadius = 0.0f; // @@@ Sphere만을 위한건데 이렇게 처리해야함??
	std::string MeshName;
};

class FScene
{
public:
	void Clear();
public:
	std::vector<FRenderItem> RenderItems;
};