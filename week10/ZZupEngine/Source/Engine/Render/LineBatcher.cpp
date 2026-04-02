#include "LineBatcher.h"
#include "Core/EngineTypes.h"
#include "Math/Utils.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float GridPlaneZ = 0.0f;
	constexpr float GridFadeStartRatio = 0.72f;
	constexpr float AxisFadeStartRatio = 0.9f;
	constexpr float GridMinVisibleAlpha = 0.05f;
	constexpr float AxisMinVisibleAlpha = 0.85f;

	// ortho 뷰 방향에 따라 그리드를 그릴 평면을 결정합니다.
	// XY : Top/Bottom (default)
	// XZ : Left/Right (카메라가 Y 축 방향)
	// YZ : Front/Back (카메라가 X 축 방향)
	enum class EGridPlane { XY, XZ, YZ };

	struct FGridPlaneDesc
	{
		int32    A;       // 수평 축 인덱스 (0=X, 1=Y, 2=Z)
		int32    B;       // 수직 축 인덱스
		int32    N;       // 법선 축 인덱스 (그리드 평면이 이 값 = 0 에 위치)
		FVector4 ColorA;  // 수평 축 색상
		FVector4 ColorB;  // 수직 축 색상
	};

	EGridPlane DetermineGridPlane(bool bOrthographic, const FVector& CameraForward)
	{
		if (!bOrthographic) return EGridPlane::XY;

		const float AxX = std::fabs(CameraForward.X);
		const float AxY = std::fabs(CameraForward.Y);
		const float AxZ = std::fabs(CameraForward.Z);

		if (AxX >= AxY && AxX >= AxZ) return EGridPlane::YZ;
		if (AxY >= AxX && AxY >= AxZ) return EGridPlane::XZ;
		return EGridPlane::XY;
	}

	FGridPlaneDesc GetGridPlaneDesc(EGridPlane Plane)
	{
		switch (Plane)
		{
		case EGridPlane::XZ: return { 0, 2, 1, FColor::Red().ToVector4(),   FColor::Blue().ToVector4()  };
		case EGridPlane::YZ: return { 1, 2, 0, FColor::Green().ToVector4(), FColor::Blue().ToVector4()  };
		default:             return { 0, 1, 2, FColor::Red().ToVector4(),   FColor::Green().ToVector4() };
		}
	}

	// 인덱스로 FVector 성분에 접근하는 헬퍼
	inline float  GetComp(const FVector& V, int32 Idx) { return (&V.X)[Idx]; }
	inline float& GetComp(FVector& V,       int32 Idx) { return (&V.X)[Idx]; }

	// A, B, N 축 값으로 FVector를 생성합니다.
	FVector MakeGridPoint(const FGridPlaneDesc& Desc, float vA, float vB, float vN)
	{
		FVector P = FVector::ZeroVector;
		GetComp(P, Desc.A) = vA;
		GetComp(P, Desc.B) = vB;
		GetComp(P, Desc.N) = vN;
		return P;
	}

	// 카메라 위치와 방향에서 그리드 평면(N=0)과의 교점을 구합니다.
	FVector ComputeGridFocusPointOnPlane(const FVector& CameraPos, const FVector& CameraFwd,
	                                     const FGridPlaneDesc& Desc)
	{
		const float PosN = GetComp(CameraPos, Desc.N);
		const float FwdN = GetComp(CameraFwd, Desc.N);

		if (std::fabs(FwdN) > MathUtil::Epsilon)
		{
			const float T = -PosN / FwdN;
			if (T > 0.0f)
			{
				return CameraPos + CameraFwd * T;
			}
		}

		// 평행하거나 교점이 뒤에 있을 때: 카메라 위치를 평면에 투영
		FVector Fallback = CameraPos;
		GetComp(Fallback, Desc.N) = 0.0f;
		return Fallback;
	}

	// 카메라가 그리드 평면으로부터 떨어진 거리(N축 성분)로 동적 반복 횟수를 계산합니다.
	int32 ComputeDynamicHalfCountOnPlane(float Spacing, int32 BaseHalfCount,
	                                     const FVector& CameraPos, const FGridPlaneDesc& Desc)
	{
		const float BaseExtent      = Spacing * static_cast<float>(std::max(BaseHalfCount, 1));
		const float HeightDriven    = (std::fabs(GetComp(CameraPos, Desc.N)) * 2.0f) + (Spacing * 4.0f);
		const float RequiredExtent  = std::max(BaseExtent, HeightDriven);
		return std::max(BaseHalfCount, static_cast<int32>(std::ceil(RequiredExtent / Spacing)));
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
		const float LinearFade = MathUtil::Clamp(1.0f - Normalized, 0.0f, 1.0f);
		// 멀리서 여러 저알파 line이 한 픽셀에 누적되는 현상을 줄이기 위해
		// grid fade를 선형보다 조금 더 빠르게 감쇠시킨다.
		return LinearFade * LinearFade;
	}

	FVector4 WithAlpha(const FVector4& Color, float Alpha)
	{
		return FVector4(Color.X, Color.Y, Color.Z, Color.W * MathUtil::Clamp(Alpha, 0.0f, 1.0f));
	}

	bool IsAxisLine(float Coordinate, float Spacing)
	{
		return std::fabs(Coordinate) <= (Spacing * 0.25f);
	}

	void ReleaseBuffer(ID3D11Buffer*& Buffer)
	{
		if (Buffer)
		{
			Buffer->Release();
			Buffer = nullptr;
		}
	}

	bool CreateDynamicBuffer(ID3D11Device* Device, uint32 ByteWidth, UINT BindFlags, ID3D11Buffer** OutBuffer)
	{
		if (!Device || ByteWidth == 0 || !OutBuffer)
		{
			return false;
		}

		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = ByteWidth;
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = BindFlags;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, OutBuffer));
	}
}

void FLineBatcher::Create(ID3D11Device* InDevice)
{
	Release();

	Device = InDevice;
	if (!Device)
	{
		return;
	}

	Device->AddRef();

	MaxIndexedVertexCount = 512;
	MaxIndexCount = 1536;

	if (!CreateDynamicBuffer(Device, sizeof(FLineVertex) * MaxIndexedVertexCount, D3D11_BIND_VERTEX_BUFFER, &IndexedVertexBuffer) ||
		!CreateDynamicBuffer(Device, sizeof(uint32) * MaxIndexCount, D3D11_BIND_INDEX_BUFFER, &IndexBuffer))
	{
		Release();
		return;
	}
}

void FLineBatcher::Release()
{
	ReleaseBuffer(IndexedVertexBuffer);
	ReleaseBuffer(IndexBuffer);

	if (Device)
	{
		Device->Release();
		Device = nullptr;
	}

	MaxIndexedVertexCount = 0;
	MaxIndexCount = 0;
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

void FLineBatcher::AddWorldHelpers(const FShowFlags& ShowFlags, float GridSpacing, int32 GridHalfLineCount,
	const FVector& CameraPosition, const FVector& CameraForward, bool bOrthographic)
{
	const float Spacing = GridSpacing;
	const int32 BaseHalfCount = std::max(GridHalfLineCount, 1);

	if (Spacing <= 0.0f) return;

	// 뷰 방향에 맞는 그리드 평면 결정
	const EGridPlane    Plane = DetermineGridPlane(bOrthographic, CameraForward);
	const FGridPlaneDesc Desc = GetGridPlaneDesc(Plane);

	// 카메라 → 그리드 평면 교점을 포커스 포인트로 사용
	const FVector FocusPoint   = ComputeGridFocusPointOnPlane(CameraPosition, CameraForward, Desc);
	const float   CenterA      = SnapToGrid(GetComp(FocusPoint, Desc.A), Spacing);
	const float   CenterB      = SnapToGrid(GetComp(FocusPoint, Desc.B), Spacing);
	const int32   DynamicHalf  = ComputeDynamicHalfCountOnPlane(Spacing, BaseHalfCount, CameraPosition, Desc);
	const float   BaseExtent   = Spacing * static_cast<float>(DynamicHalf);

	// 포커스 기반 범위
	const float FocusMinA = CenterA - BaseExtent;
	const float FocusMaxA = CenterA + BaseExtent;
	const float FocusMinB = CenterB - BaseExtent;
	const float FocusMaxB = CenterB + BaseExtent;

	// 카메라가 속한 셀까지 범위를 확장 (카메라 위치 기반)
	const float CameraA    = GetComp(CameraPosition, Desc.A);
	const float CameraB    = GetComp(CameraPosition, Desc.B);
	const float MinA = std::min(FocusMinA, SnapDownToGrid(CameraA, Spacing));
	const float MaxA = std::max(FocusMaxA, SnapUpToGrid(CameraA, Spacing));
	const float MinB = std::min(FocusMinB, SnapDownToGrid(CameraB, Spacing));
	const float MaxB = std::max(FocusMaxB, SnapUpToGrid(CameraB, Spacing));

	const int32 MinAIdx = static_cast<int32>(std::floor((MinA - CenterA) / Spacing));
	const int32 MaxAIdx = static_cast<int32>(std::ceil( (MaxA - CenterA) / Spacing));
	const int32 MinBIdx = static_cast<int32>(std::floor((MinB - CenterB) / Spacing));
	const int32 MaxBIdx = static_cast<int32>(std::ceil( (MaxB - CenterB) / Spacing));

	const float ExtentA       = std::max(std::fabs(MinA - GetComp(FocusPoint, Desc.A)),
	                                     std::fabs(MaxA - GetComp(FocusPoint, Desc.A)));
	const float ExtentB       = std::max(std::fabs(MinB - GetComp(FocusPoint, Desc.B)),
	                                     std::fabs(MaxB - GetComp(FocusPoint, Desc.B)));
	const float FadeStartA    = ExtentA * GridFadeStartRatio;
	const float FadeStartB    = ExtentB * GridFadeStartRatio;
	const float AxisFadeStA   = ExtentA * AxisFadeStartRatio;
	const float AxisFadeStB   = ExtentB * AxisFadeStartRatio;
	const float AxisBias      = std::max(Spacing * 0.001f, 0.001f);

	// 축선 표시 여부: 해당 축이 그리드 범위 안에 들어오는지 확인
	const bool bShowAxisA = (MinB <= 0.0f) && (MaxB >= 0.0f); // B=0 라인 (A 축)
	const bool bShowAxisB = (MinA <= 0.0f) && (MaxA >= 0.0f); // A=0 라인 (B 축)

	if (ShowFlags.bGrid)
	{
		const FVector4 GridColor = FColor::Gray().ToVector4();

		// B 방향으로 스윕: 상수 B 라인 (A 축 방향으로 뻗음)
		for (int32 BIdx = MinBIdx; BIdx <= MaxBIdx; ++BIdx)
		{
			const float WorldB = CenterB + static_cast<float>(BIdx) * Spacing;
			if (bShowAxisA && IsAxisLine(WorldB, Spacing)) continue;

			const float Alpha = ComputeLineFade(WorldB - GetComp(FocusPoint, Desc.B), FadeStartB, ExtentB);
			if (Alpha > GridMinVisibleAlpha)
			{
				AddLine(
					MakeGridPoint(Desc, MinA, WorldB, GridPlaneZ),
					MakeGridPoint(Desc, MaxA, WorldB, GridPlaneZ),
					WithAlpha(GridColor, Alpha));
			}
		}

		// A 방향으로 스윕: 상수 A 라인 (B 축 방향으로 뻗음)
		for (int32 AIdx = MinAIdx; AIdx <= MaxAIdx; ++AIdx)
		{
			const float WorldA = CenterA + static_cast<float>(AIdx) * Spacing;
			if (bShowAxisB && IsAxisLine(WorldA, Spacing)) continue;

			const float Alpha = ComputeLineFade(WorldA - GetComp(FocusPoint, Desc.A), FadeStartA, ExtentA);
			if (Alpha > GridMinVisibleAlpha)
			{
				AddLine(
					MakeGridPoint(Desc, WorldA, MinB, GridPlaneZ),
					MakeGridPoint(Desc, WorldA, MaxB, GridPlaneZ),
					WithAlpha(GridColor, Alpha));
			}
		}
	}

	if (ShowFlags.bAxis)
	{
		// A 축선 (B=0)
		if (bShowAxisA)
		{
			const float Alpha = std::max(AxisMinVisibleAlpha,
				ComputeLineFade(-GetComp(FocusPoint, Desc.B), AxisFadeStB, ExtentB));
			AddLine(
				MakeGridPoint(Desc, MinA, 0.0f, AxisBias),
				MakeGridPoint(Desc, MaxA, 0.0f, AxisBias),
				WithAlpha(Desc.ColorA, Alpha));
		}

		// B 축선 (A=0)
		if (bShowAxisB)
		{
			const float Alpha = std::max(AxisMinVisibleAlpha,
				ComputeLineFade(-GetComp(FocusPoint, Desc.A), AxisFadeStA, ExtentA));
			AddLine(
				MakeGridPoint(Desc, 0.0f, MinB, AxisBias),
				MakeGridPoint(Desc, 0.0f, MaxB, AxisBias),
				WithAlpha(Desc.ColorB, Alpha));
		}

		// XY 평면에서만: 원점을 통과하는 Z(Blue) 수직 축을 추가로 그립니다.
		if (Plane == EGridPlane::XY && bShowAxisA && bShowAxisB)
		{
			const float AxisHeight = std::max(Spacing * static_cast<float>(BaseHalfCount), Spacing * 10.0f);
			AddLine(
				FVector(0.0f, 0.0f, -AxisHeight),
				FVector(0.0f, 0.0f,  AxisHeight),
				FColor::Blue().ToVector4());
		}
	}
}

void FLineBatcher::Clear()
{
	IndexedVertices.clear();
	Indices.clear();
}

void FLineBatcher::Flush(ID3D11DeviceContext* Context)
{
	if (!Context || !Device)
	{
		return;
	}

	const uint32 RequiredIndexedVertexCount = static_cast<uint32>(IndexedVertices.size());
	const uint32 RequiredIndexCount = static_cast<uint32>(Indices.size());
	if (RequiredIndexedVertexCount == 0 || RequiredIndexCount == 0)
	{
		return;
	}

	if (!IndexedVertexBuffer || RequiredIndexedVertexCount > MaxIndexedVertexCount)
	{
		ReleaseBuffer(IndexedVertexBuffer);
		MaxIndexedVertexCount = RequiredIndexedVertexCount * 2;
		if (!CreateDynamicBuffer(Device, sizeof(FLineVertex) * MaxIndexedVertexCount, D3D11_BIND_VERTEX_BUFFER, &IndexedVertexBuffer))
		{
			MaxIndexedVertexCount = 0;
			return;
		}
	}

	if (!IndexBuffer || RequiredIndexCount > MaxIndexCount)
	{
		ReleaseBuffer(IndexBuffer);
		MaxIndexCount = RequiredIndexCount * 2;
		if (!CreateDynamicBuffer(Device, sizeof(uint32) * MaxIndexCount, D3D11_BIND_INDEX_BUFFER, &IndexBuffer))
		{
			MaxIndexCount = 0;
			return;
		}
	}

	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	if (FAILED(Context->Map(IndexedVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
	{
		return;
	}

	memcpy(MappedResource.pData, IndexedVertices.data(), sizeof(FLineVertex) * RequiredIndexedVertexCount);
	Context->Unmap(IndexedVertexBuffer, 0);

	if (FAILED(Context->Map(IndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
	{
		return;
	}

	memcpy(MappedResource.pData, Indices.data(), sizeof(uint32) * RequiredIndexCount);
	Context->Unmap(IndexBuffer, 0);

	UINT Stride = sizeof(FLineVertex);
	UINT Offset = 0;
	Context->IASetVertexBuffers(0, 1, &IndexedVertexBuffer, &Stride, &Offset);
	Context->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Context->DrawIndexed(RequiredIndexCount, 0, 0);
}

uint32 FLineBatcher::GetLineCount() const
{
	return static_cast<uint32>(Indices.size() / 2);
}
