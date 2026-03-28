#pragma once

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

	/**
	 * @brief 단색 라인을 배치에 추가합니다.
	 * @details 시작점과 끝점에 같은 색을 적용하는 편의 오버로드입니다.
	 */
	void AddLine(const FVector& Start, const FVector& End, const FVector4& Color);
	/**
	 * @brief 정점별 색을 가진 라인을 배치에 추가합니다.
	 * @details 선 양 끝 정점에 서로 다른 색을 줄 수 있는 오버로드입니다.
	 * 선마다 alpha를 다르게 실어 보냄
	 */
	void AddLine(const FVector& Start, const FVector& End, const FVector4& StartColor, const FVector4& EndColor);

	// AABB 와이어프레임 (12개 Edge)
	void AddAABB(const FBoundingBox& Box, const FColor& Color);

	/**
	 * @brief 카메라 기준의 grid patch와 축 보조선을 생성합니다.
	 * @details 카메라 위치와 forward 벡터를 이용해 바닥면 위의 focus point를 계산하고,
	 * 그 지점을 grid spacing에 맞춰 스냅한 뒤 현재 프레임에 필요한 선만 배치합니다.
	 * grid는 focus point로부터 멀어질수록 alpha가 감소하도록 정점 색을 구성하며,
	 * 축과 겹치는 grid 선은 생성하지 않아 z-fighting을 줄입니다.
	 * 생성된 모든 선은 LineBatcher의 버텍스 배열에 누적되므로 draw call 수는 증가하지 않습니다.
	 */
	void AddWorldHelpers(const FShowFlags& ShowFlags, float GridSpacing, int32 GridHalfLineCount, const FVector& CameraPosition, const FVector& CameraForward);

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
	TArray<FLineVertex> IndexedVertices;
	TArray<uint32> Indices;

	ID3D11Buffer* IndexedVertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	ID3D11Device* Device = nullptr;
	uint32 MaxIndexedVertexCount = 0;
	uint32 MaxIndexCount = 0;
};
