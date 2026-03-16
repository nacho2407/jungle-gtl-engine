#pragma once

#include "CoreTypes.h"
#include "Math/FMatrix.h"
#include "Resource/UStaticMesh.h"

#include "FHitProxy.h"

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

	/* 마우스로 Render Item이 선택되어 컴포넌트 계층으로 선택된 것이
	   어느 객체인지 반환해주기 위한 용도 */
	FHitProxy HitProxy;
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

	TArray<FRenderItem> RenderItems;
};