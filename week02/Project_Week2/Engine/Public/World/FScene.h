#pragma once

#include <vector>
#include <string>
#include "Math/FMatrix.h"

class UStaticMesh;

// 하이라이트
enum class ERenderCullMode
{
	Back,
	Front,
	None
};

struct FRenderItem
{
	FMatrix WorldMatrix = FMatrix::Identity;
	UStaticMesh* Mesh = nullptr;
	FVector4 Color = FVector4(1, 1, 1, 1);

	ERenderCullMode CullMode = ERenderCullMode::Back;
	bool bDepthEnable = true;
	bool bUseVertexColor = true;
};

class FScene
{
public:
	void Clear();
public:
	std::vector<FRenderItem> RenderItems;
};