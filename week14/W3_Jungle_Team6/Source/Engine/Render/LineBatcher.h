#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Common/RenderTypes.h"

#include "Editor/Settings/EditorSettings.h"

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
//   3) Flush()         — Dynamic VB 업로드 + Draw Call (D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
//
// 파트 B가 구현, 파트 A(Renderer)가 매 프레임 호출
// ============================================================
class FLineBatcher
{
public:
	FLineBatcher() = default;
	~FLineBatcher() = default;

	// ---- 초기화 / 해제 ----
	void Create(ID3D11Device* Device);
	void Release();

	// ---- 라인 축적 API ----

	// 두 점 사이의 라인 1개 추가
	void AddLine(const FVector& Start, const FVector& End, const FVector4& Color);

	// AABB 와이어프레임 (12개 Edge)
	void AddAABB(const FBoundingBox& Box, const FColor& Color);

	// 월드 그리드 생성 (GridSpacing 간격으로 라인 생성, X축=빨강, Y축=초록)
	// @@@ AddLine <- Z축 + AddGridXY()
	void AddWorldGrid(float GridSpacing, int HalfGridCount);
	void AddWorldAxes(float GridSpacing, int HalfGridCount);
	void AddWorldHelpers(FEditorSettings& Settings);

	// 이번 프레임에 축적된 라인 모두 제거
	void Clear();

	// ---- 렌더링 ----

	// Dynamic VB에 업로드 후 Draw Call 실행
	// ViewProj: 카메라의 View * Projection 행렬
	// %%% Flush(ID3D11DeviceContext* Context, const FMatrix& ViewProj) -> Flush(ID3D11DeviceContext* Context)
	void Flush(ID3D11DeviceContext* Context);

	// 현재 축적된 라인 개수
	uint32 GetLineCount() const;

private:
	TArray<FLineVertex> Vertices;

	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Device* Device = nullptr; // @@@ (필요 시 새 버퍼 생성용)
	uint32 MaxVertexCount = 0;		// VB 크기 (필요 시 재할당)

	int HalfGridCount = 100; // @@@ Grid 크기
	float GridSpacing = 1.0f; // @@@ Grid 간격 %%% GUI에서 소수점은 걸러야 하지 않나?
};
