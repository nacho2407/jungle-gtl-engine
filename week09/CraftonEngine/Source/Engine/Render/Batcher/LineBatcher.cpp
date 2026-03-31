#include "LineBatcher.h"
#include "Core/EngineTypes.h"
#include "Math/Utils.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float GridFadeStartRatio = 0.72f;
	constexpr float AxisFadeStartRatio = 0.9f;
	constexpr float GridMinVisibleAlpha = 0.05f;
	constexpr float AxisMinVisibleAlpha = 0.85f;

	// FVector 컴포넌트 인덱스 접근 (X=0, Y=1, Z=2)
	float  Comp(const FVector& V, int I) { return (&V.X)[I]; }

	// 두 그리드축 값 + 법선축 값 → FVector 생성
	FVector MakeGridPoint(int A0, int A1, int N, float V0, float V1, float VN)
	{
		FVector P;
		(&P.X)[A0] = V0;
		(&P.X)[A1] = V1;
		(&P.X)[N] = VN;
		return P;
	}

	// 축 인덱스 → 축 색상 (0=X:Red, 1=Y:Green, 2=Z:Blue)
	FVector4 AxisColor(int Axis)
	{
		switch (Axis)
		{
		case 0: return FColor::Red().ToVector4();
		case 1: return FColor::Green().ToVector4();
		default: return FColor::Blue().ToVector4();
		}
	}

	float SnapToGrid(float Value, float Spacing)
	{
		return std::round(Value / Spacing) * Spacing;
	}

	float SnapDownToGrid(float Value, float Spacing)
	{
		return std::floor(Value / Spacing) * Spacing;
	}

	float SnapUpToGrid(float Value, float Spacing)
	{
		return std::ceil(Value / Spacing) * Spacing;
	}

	float ComputeLineFade(float OffsetFromFocus, float FadeStart, float FadeEnd)
	{
		if (FadeEnd <= FadeStart)
		{
			return 1.0f;
		}

		const float Normalized = (std::fabs(OffsetFromFocus) - FadeStart) / (FadeEnd - FadeStart);
		const float LinearFade = Clamp(1.0f - Normalized, 0.0f, 1.0f);
		return LinearFade * LinearFade;
	}

	FVector4 WithAlpha(const FVector4& Color, float Alpha)
	{
		return FVector4(Color.X, Color.Y, Color.Z, Color.W * Clamp(Alpha, 0.0f, 1.0f));
	}

	bool IsAxisLine(float Coordinate, float Spacing)
	{
		return std::fabs(Coordinate) <= (Spacing * 0.25f);
	}

	// 카메라 법선축 거리 기반 동적 그리드 확장
	int32 ComputeDynamicHalfCount(float Spacing, int32 BaseHalfCount, float CameraNormalDist)
	{
		const float BaseExtent = Spacing * static_cast<float>(std::max(BaseHalfCount, 1));
		const float HeightDrivenExtent = (std::fabs(CameraNormalDist) * 2.0f) + (Spacing * 4.0f);
		const float RequiredExtent = std::max(BaseExtent, HeightDrivenExtent);
		return std::max(BaseHalfCount, static_cast<int32>(std::ceil(RequiredExtent / Spacing)));
	}

	// 카메라 forward와 그리드 법선축(NormalIdx)의 교점 계산
	FVector ComputeGridFocusPoint(const FVector& CameraPosition, const FVector& CameraForward, int A0, int A1, int N, float PlaneOffset)
	{
		float FwdN = Comp(CameraForward, N);
		if (std::fabs(FwdN) > EPSILON)
		{
			const float T = (PlaneOffset - Comp(CameraPosition, N)) / FwdN;
			if (T > 0.0f)
			{
				return CameraPosition + (CameraForward * T);
			}
		}

		// 평면과 거의 평행 → 그리드 평면에서 카메라 투영
		FVector PlanarFwd = CameraForward;
		(&PlanarFwd.X)[N] = 0.0f;
		if (PlanarFwd.Length() > EPSILON)
		{
			PlanarFwd.Normalize();
			FVector Result = CameraPosition;
			(&Result.X)[N] = PlaneOffset;
			return Result + (PlanarFwd * (std::fabs(Comp(CameraPosition, N) - PlaneOffset) * 0.5f));
		}

		FVector Result = CameraPosition;
		(&Result.X)[N] = PlaneOffset;
		return Result;
	}

	// 카메라 forward의 dominant axis 인덱스 (절대값이 가장 큰 축)
	int DominantAxis(const FVector& V)
	{
		float AX = std::fabs(V.X), AY = std::fabs(V.Y), AZ = std::fabs(V.Z);
		if (AX >= AY && AX >= AZ) return 0;
		if (AY >= AX && AY >= AZ) return 1;
		return 2;
	}
}

void FLineBatcher::Create(ID3D11Device* InDevice)
{
	Release();
	CreateBuffers(InDevice, 512, sizeof(FLineVertex), 1536);
}

void FLineBatcher::Release()
{
	ReleaseBuffers();
	IndexedVertices.clear();
	Indices.clear();
}

void FLineBatcher::AddLine(const FVector& Start, const FVector& End, const FVector4& InColor)
{
	AddLine(Start, End, InColor, InColor);
}

void FLineBatcher::AddLine(const FVector& Start, const FVector& End, const FVector4& StartColor, const FVector4& EndColor)
{
	const uint32 BaseVertex = static_cast<uint32>(IndexedVertices.size());
	IndexedVertices.emplace_back(Start, StartColor);
	IndexedVertices.emplace_back(End, EndColor);
	Indices.push_back(BaseVertex);
	Indices.push_back(BaseVertex + 1);
}
void FLineBatcher::AddAABB(const FBoundingBox& Box, const FColor& InColor)
{
	const FVector4 BoxColor = InColor.ToVector4();
	const uint32 BaseVertex = static_cast<uint32>(IndexedVertices.size());

	IndexedVertices.emplace_back(FVector(Box.Min.X, Box.Min.Y, Box.Min.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Max.X, Box.Min.Y, Box.Min.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Max.X, Box.Max.Y, Box.Min.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Min.X, Box.Max.Y, Box.Min.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Min.X, Box.Min.Y, Box.Max.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Max.X, Box.Min.Y, Box.Max.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Max.X, Box.Max.Y, Box.Max.Z), BoxColor);
	IndexedVertices.emplace_back(FVector(Box.Min.X, Box.Max.Y, Box.Max.Z), BoxColor);

	static constexpr uint32 AABBEdgeIndices[] =
	{
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	};

	for (uint32 EdgeIndex : AABBEdgeIndices)
	{
		Indices.push_back(BaseVertex + EdgeIndex);
	}
}

void FLineBatcher::AddWorldHelpers(const FShowFlags& ShowFlags, float GridSpacing, int32 GridHalfLineCount, const FVector& CameraPosition, const FVector& CameraForward, bool bIsOrtho)
{
	const float Spacing = GridSpacing;
	const int32 BaseHalfCount = std::max(GridHalfLineCount, 1);

	if (Spacing <= 0.0f)
	{
		return;
	}

	// ── 그리드 평면 결정 ──
	// Perspective: 항상 XY 평면 (법선=Z)
	// Ortho: 카메라 forward의 주축을 법선으로 → 해당 축에 수직한 평면
	int N = 2; // 법선축 인덱스 (기본 Z)
	if (bIsOrtho)
	{
		N = DominantAxis(CameraForward);
	}

	// 그리드 2축: 법선이 아닌 나머지 두 축
	const int A0 = (N == 0) ? 1 : 0;
	const int A1 = (N == 2) ? 1 : 2;
	constexpr float PlaneOffset = 0.0f;

	const FVector FocusPoint = ComputeGridFocusPoint(CameraPosition, CameraForward, A0, A1, N, PlaneOffset);
	const float Focus0 = Comp(FocusPoint, A0);
	const float Focus1 = Comp(FocusPoint, A1);

	const float Center0 = SnapToGrid(Focus0, Spacing);
	const float Center1 = SnapToGrid(Focus1, Spacing);
	const float CameraNormalDist = Comp(CameraPosition, N) - PlaneOffset;
	const int32 DynamicHalfCount = ComputeDynamicHalfCount(Spacing, BaseHalfCount, CameraNormalDist);
	const float BaseGridExtent = Spacing * static_cast<float>(DynamicHalfCount);

	const float FocusMin0 = Center0 - BaseGridExtent;
	const float FocusMax0 = Center0 + BaseGridExtent;
	const float FocusMin1 = Center1 - BaseGridExtent;
	const float FocusMax1 = Center1 + BaseGridExtent;

	const float CamMin0 = SnapDownToGrid(Comp(CameraPosition, A0), Spacing);
	const float CamMax0 = SnapUpToGrid(Comp(CameraPosition, A0), Spacing);
	const float CamMin1 = SnapDownToGrid(Comp(CameraPosition, A1), Spacing);
	const float CamMax1 = SnapUpToGrid(Comp(CameraPosition, A1), Spacing);

	const float Min0 = std::min(FocusMin0, CamMin0);
	const float Max0 = std::max(FocusMax0, CamMax0);
	const float Min1 = std::min(FocusMin1, CamMin1);
	const float Max1 = std::max(FocusMax1, CamMax1);

	const int32 Min0Idx = static_cast<int32>(std::floor((Min0 - Center0) / Spacing));
	const int32 Max0Idx = static_cast<int32>(std::ceil((Max0 - Center0) / Spacing));
	const int32 Min1Idx = static_cast<int32>(std::floor((Min1 - Center1) / Spacing));
	const int32 Max1Idx = static_cast<int32>(std::ceil((Max1 - Center1) / Spacing));

	const float Extent0 = std::max(std::fabs(Min0 - Focus0), std::fabs(Max0 - Focus0));
	const float Extent1 = std::max(std::fabs(Min1 - Focus1), std::fabs(Max1 - Focus1));
	const float GridFadeStart0 = Extent0 * GridFadeStartRatio;
	const float GridFadeStart1 = Extent1 * GridFadeStartRatio;
	const float AxisFadeStart0 = Extent0 * AxisFadeStartRatio;
	const float AxisFadeStart1 = Extent1 * AxisFadeStartRatio;
	const float AxisBias = std::max(Spacing * 0.001f, 0.001f);

	// 원점(0)이 범위 안에 있을 때 축 표시
	const bool bShowAxis0 = (Min1 <= 0.0f) && (Max1 >= 0.0f); // A0 방향 축 (A1=0 라인)
	const bool bShowAxis1 = (Min0 <= 0.0f) && (Max0 >= 0.0f); // A1 방향 축 (A0=0 라인)

	// ── 그리드 라인 ──
	if (ShowFlags.bGrid)
	{
		const FVector4 GridColor = FColor::Gray().ToVector4();

		// A1 방향 스윕 → A0 방향 라인
		for (int32 I = Min1Idx; I <= Max1Idx; ++I)
		{
			const float W1 = Center1 + (static_cast<float>(I) * Spacing);
			if (bShowAxis0 && IsAxisLine(W1, Spacing)) continue;

			const float Alpha = ComputeLineFade(W1 - Focus1, GridFadeStart1, Extent1);
			if (Alpha > GridMinVisibleAlpha)
			{
				AddLine(
					MakeGridPoint(A0, A1, N, Min0, W1, PlaneOffset),
					MakeGridPoint(A0, A1, N, Max0, W1, PlaneOffset),
					WithAlpha(GridColor, Alpha));
			}
		}

		// A0 방향 스윕 → A1 방향 라인
		for (int32 I = Min0Idx; I <= Max0Idx; ++I)
		{
			const float W0 = Center0 + (static_cast<float>(I) * Spacing);
			if (bShowAxis1 && IsAxisLine(W0, Spacing)) continue;

			const float Alpha = ComputeLineFade(W0 - Focus0, GridFadeStart0, Extent0);
			if (Alpha > GridMinVisibleAlpha)
			{
				AddLine(
					MakeGridPoint(A0, A1, N, W0, Min1, PlaneOffset),
					MakeGridPoint(A0, A1, N, W0, Max1, PlaneOffset),
					WithAlpha(GridColor, Alpha));
			}
		}
	}

	// ── 월드 축 (A0 방향 — A1=0 라인) ──
	if (bShowAxis0)
	{
		const float Alpha = std::max(AxisMinVisibleAlpha, ComputeLineFade(-Focus1, AxisFadeStart1, Extent1));
		AddLine(
			MakeGridPoint(A0, A1, N, Min0, 0.0f, AxisBias),
			MakeGridPoint(A0, A1, N, Max0, 0.0f, AxisBias),
			WithAlpha(AxisColor(A0), Alpha));
	}

	// ── 월드 축 (A1 방향 — A0=0 라인) ──
	if (bShowAxis1)
	{
		const float Alpha = std::max(AxisMinVisibleAlpha, ComputeLineFade(-Focus0, AxisFadeStart0, Extent0));
		AddLine(
			MakeGridPoint(A0, A1, N, 0.0f, Min1, AxisBias),
			MakeGridPoint(A0, A1, N, 0.0f, Max1, AxisBias),
			WithAlpha(AxisColor(A1), Alpha));
	}

	// ── 법선축 (A0=0, A1=0 일 때 수직선) ──
	if (bShowAxis0 && bShowAxis1)
	{
		const float AxisLen = std::max(Spacing * static_cast<float>(BaseHalfCount), Spacing * 10.0f);
		AddLine(
			MakeGridPoint(A0, A1, N, 0.0f, 0.0f, -AxisLen),
			MakeGridPoint(A0, A1, N, 0.0f, 0.0f, AxisLen),
			AxisColor(N));
	}
}

void FLineBatcher::Clear()
{
	IndexedVertices.clear();
	Indices.clear();
}

void FLineBatcher::DrawBatch(ID3D11DeviceContext* Context)
{
	if (!Context || !Device) return;

	const uint32 VertexCount = static_cast<uint32>(IndexedVertices.size());
	const uint32 IndexCount = static_cast<uint32>(Indices.size());
	if (VertexCount == 0 || IndexCount == 0) return;

	VB.EnsureCapacity(Device, VertexCount);
	IB.EnsureCapacity(Device, IndexCount);

	if (!VB.Update(Context, IndexedVertices.data(), VertexCount)) return;
	if (!IB.Update(Context, Indices.data(), IndexCount)) return;

	VB.Bind(Context);
	IB.Bind(Context);
	Context->DrawIndexed(IndexCount, 0, 0);
}

uint32 FLineBatcher::GetLineCount() const
{
	return static_cast<uint32>(Indices.size() / 2);
}
