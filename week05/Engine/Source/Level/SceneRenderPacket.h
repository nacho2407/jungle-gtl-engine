#pragma once

#include "CoreMinimal.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class USubUVComponent;
class UBillboardComponent;

struct ENGINE_API FSceneMeshPrimitive
{
	// 씬 기하로 렌더링할 스태틱 메시 컴포넌트다.
	UStaticMeshComponent* Component = nullptr;
};

struct ENGINE_API FSceneTextPrimitive
{
	// 텍스트 렌더 기능이 실제 메시로 확장할 텍스트 컴포넌트다.
	UTextRenderComponent* Component = nullptr;
};

struct ENGINE_API FSceneSubUVPrimitive
{
	// 스프라이트 기능이 실제 메시로 확장할 SubUV 컴포넌트다.
	USubUVComponent* Component = nullptr;
};

struct ENGINE_API FSceneBillboardPrimitive
{
	// 빌보드 기능이 실제 메시로 확장할 빌보드 컴포넌트다.
	UBillboardComponent* Component = nullptr;
};

struct ENGINE_API FSceneRenderPacket
{
	// 이 뷰에서 월드로부터 수집한 메시 프리미티브 목록이다.
	TArray<FSceneMeshPrimitive> MeshPrimitives;
	// 이 뷰에서 월드로부터 수집한 텍스트 프리미티브 목록이다.
	TArray<FSceneTextPrimitive> TextPrimitives;
	// 이 뷰에서 월드로부터 수집한 애니메이션 스프라이트 프리미티브 목록이다.
	TArray<FSceneSubUVPrimitive> SubUVPrimitives;
	// 이 뷰에서 월드로부터 수집한 빌보드 프리미티브 목록이다.
	TArray<FSceneBillboardPrimitive> BillboardPrimitives;

	// 각 프리미티브 버킷에 같은 reserve 힌트를 적용한다.
	void Reserve(size_t PrimitiveCountHint)
	{
		MeshPrimitives.reserve(PrimitiveCountHint);
		TextPrimitives.reserve(PrimitiveCountHint);
		SubUVPrimitives.reserve(PrimitiveCountHint);
		BillboardPrimitives.reserve(PrimitiveCountHint);
	}

	// 패킷 안의 모든 프리미티브 버킷을 비운다.
	void Clear()
	{
		MeshPrimitives.clear();
		TextPrimitives.clear();
		SubUVPrimitives.clear();
		BillboardPrimitives.clear();
	}
};
