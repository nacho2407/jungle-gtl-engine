#pragma once

#include "BatcherBase.h"
#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ViewTypes.h"

// ============================================================
// FLineVertex — 라인 렌더링용 버텍스 (Position + Color)
// ============================================================
struct FLineVertex
{
	FVector Position;
	FVector4 Color;

	FLineVertex(const FVector& InPos, const FVector4& InColor) : Position(InPos), Color(InColor) {}
};

// ============================================================
// FLineBatcher — 라인을 모아서 한 번에 GPU로 그리는 배처
//
// 사용 흐름:
//   1) Clear()         — 매 프레임 시작 시 이전 라인 제거
//   2) AddLine / AddAABB 등으로 라인 축적
//   3) DrawBatch()     — Dynamic VB 업로드 + Draw Call (D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
// ============================================================
class FLineBatcher : public FBatcherBase
{
public:
	// ---- 초기화 / 해제 ----
	void Create(ID3D11Device* InDevice);
	void Release();

	// ---- 라인 축적 API ----
	void AddLine(const FVector& Start, const FVector& End, const FVector4& Color);
	void AddLine(const FVector& Start, const FVector& End, const FVector4& StartColor, const FVector4& EndColor);
	void AddAABB(const FBoundingBox& Box, const FColor& Color);
	void AddWorldHelpers(const FShowFlags& ShowFlags, float GridSpacing, int32 GridHalfLineCount, const FVector& CameraPosition, const FVector& CameraForward);

	// 이번 프레임에 축적된 라인 모두 제거
	void Clear();

	// ---- 렌더링 ----
	void DrawBatch(ID3D11DeviceContext* Context);

	// 현재 축적된 라인 개수
	uint32 GetLineCount() const;

private:
	TArray<FLineVertex> IndexedVertices;
	TArray<uint32> Indices;
};
