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
		// 멀리서 여러 저알파 line이 한 픽셀에 누적되는 현상을 줄이기 위해
		// grid fade를 선형보다 조금 더 빠르게 감쇠시킨다.
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

	int32 ComputeDynamicHalfCount(float Spacing, int32 BaseHalfCount, const FVector& CameraPosition)
	{
		const float BaseExtent = Spacing * static_cast<float>(std::max(BaseHalfCount, 1));
		// 카메라 높이에 따른 필요한 grid 크기
		const float HeightDrivenExtent = (std::fabs(CameraPosition.Z) * 2.0f) + (Spacing * 4.0f);
		const float RequiredExtent = std::max(BaseExtent, HeightDrivenExtent);
		return std::max(BaseHalfCount, static_cast<int32>(std::ceil(RequiredExtent / Spacing)));
	}

	FVector ComputeGridFocusPoint(const FVector& CameraPosition, const FVector& CameraForward)
	{
		if (std::fabs(CameraForward.Z) > EPSILON) // if Z가 거의 EPSILON -> 평면과 평행 -> 교차 계산 X
		{
			const float T = (GridPlaneZ - CameraPosition.Z) / CameraForward.Z;
			if (T > 0.0f) // 카메라 앞쪽 방향만 사용
			{
				return CameraPosition + (CameraForward * T);
			}
		}

		FVector PlanarForward(CameraForward.X, CameraForward.Y, 0.0f); // 평행한 경우 -> Z 성분 제거 -> XY 평면 방향만 사용
		if (PlanarForward.Length() > EPSILON)
		{
			PlanarForward.Normalize();
			// 카메라 아래 지점 + 앞으로 조금 이동
			return FVector(CameraPosition.X, CameraPosition.Y, GridPlaneZ) + (PlanarForward * (std::fabs(CameraPosition.Z) * 0.5f));
		}

		return FVector(CameraPosition.X, CameraPosition.Y, GridPlaneZ);
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

void FLineBatcher::AddWorldHelpers(const FShowFlags& ShowFlags, float GridSpacing, int32 GridHalfLineCount, const FVector& CameraPosition, const FVector& CameraForward)
{
	const float Spacing = GridSpacing;
	const int32 BaseHalfCount = std::max(GridHalfLineCount, 1);

	if (Spacing <= 0.0f)
	{
		return;
	}

	const FVector FocusPoint = ComputeGridFocusPoint(CameraPosition, CameraForward);
	const float CenterX = SnapToGrid(FocusPoint.X, Spacing);
	const float CenterY = SnapToGrid(FocusPoint.Y, Spacing);
	const int32 DynamicHalfCount = ComputeDynamicHalfCount(Spacing, BaseHalfCount, CameraPosition);
	const float BaseGridExtent = Spacing * static_cast<float>(DynamicHalfCount);
	const float FocusMinX = CenterX - BaseGridExtent;
	const float FocusMaxX = CenterX + BaseGridExtent;
	const float FocusMinY = CenterY - BaseGridExtent;
	const float FocusMaxY = CenterY + BaseGridExtent;

	// focus point 기반 패치를 유지하되, 카메라가 포함된 셀까지는 항상 grid가 이어지도록 범위를 확장한다.
	const float CameraMinX = SnapDownToGrid(CameraPosition.X, Spacing);
	const float CameraMaxX = SnapUpToGrid(CameraPosition.X, Spacing);
	const float CameraMinY = SnapDownToGrid(CameraPosition.Y, Spacing);
	const float CameraMaxY = SnapUpToGrid(CameraPosition.Y, Spacing);

	const float MinX = std::min(FocusMinX, CameraMinX);
	const float MaxX = std::max(FocusMaxX, CameraMaxX);
	const float MinY = std::min(FocusMinY, CameraMinY);
	const float MaxY = std::max(FocusMaxY, CameraMaxY);

	const int32 MinXIndex = static_cast<int32>(std::floor((MinX - CenterX) / Spacing));
	const int32 MaxXIndex = static_cast<int32>(std::ceil((MaxX - CenterX) / Spacing));
	const int32 MinYIndex = static_cast<int32>(std::floor((MinY - CenterY) / Spacing));
	const int32 MaxYIndex = static_cast<int32>(std::ceil((MaxY - CenterY) / Spacing));

	const float GridExtentX = std::max(std::fabs(MinX - FocusPoint.X), std::fabs(MaxX - FocusPoint.X));
	const float GridExtentY = std::max(std::fabs(MinY - FocusPoint.Y), std::fabs(MaxY - FocusPoint.Y));
	const float GridFadeStartX = GridExtentX * GridFadeStartRatio;
	const float GridFadeStartY = GridExtentY * GridFadeStartRatio;
	const float AxisFadeStartX = GridExtentX * AxisFadeStartRatio;
	const float AxisFadeStartY = GridExtentY * AxisFadeStartRatio;
	const float AxisHeightBias = std::max(Spacing * 0.001f, 0.001f);

	const bool bShowXAxis = (MinY <= 0.0f) && (MaxY >= 0.0f);
	const bool bShowYAxis = (MinX <= 0.0f) && (MaxX >= 0.0f);

	if (ShowFlags.bGrid)
	{
		const FVector4 GridColor = FColor::Gray().ToVector4();

		for (int32 YIndex = MinYIndex; YIndex <= MaxYIndex; ++YIndex)
		{
			const float WorldY = CenterY + (static_cast<float>(YIndex) * Spacing);
			if (!(bShowXAxis && IsAxisLine(WorldY, Spacing)))
			{
				const float Alpha = ComputeLineFade(WorldY - FocusPoint.Y, GridFadeStartY, GridExtentY);
				if (Alpha > GridMinVisibleAlpha)
				{
					AddLine(
						FVector(MinX, WorldY, GridPlaneZ),
						FVector(MaxX, WorldY, GridPlaneZ),
						WithAlpha(GridColor, Alpha)
					);
				}
			}

		}

		for (int32 XIndex = MinXIndex; XIndex <= MaxXIndex; ++XIndex)
		{
			const float WorldX = CenterX + (static_cast<float>(XIndex) * Spacing);
			if (!(bShowYAxis && IsAxisLine(WorldX, Spacing)))
			{
				const float Alpha = ComputeLineFade(WorldX - FocusPoint.X, GridFadeStartX, GridExtentX);
				if (Alpha > GridMinVisibleAlpha)
				{
					AddLine(
						FVector(WorldX, MinY, GridPlaneZ),
						FVector(WorldX, MaxY, GridPlaneZ),
						WithAlpha(GridColor, Alpha)
					);
				}
			}
		}
	}

	if (bShowXAxis)
	{
		const float Alpha = std::max(AxisMinVisibleAlpha, ComputeLineFade(-FocusPoint.Y, AxisFadeStartY, GridExtentY));
		AddLine(
			FVector(MinX, 0.0f, AxisHeightBias),
			FVector(MaxX, 0.0f, AxisHeightBias),
			WithAlpha(FColor::Red().ToVector4(), Alpha)
		);
	}

	if (bShowYAxis)
	{
		const float Alpha = std::max(AxisMinVisibleAlpha, ComputeLineFade(-FocusPoint.X, AxisFadeStartX, GridExtentX));
		AddLine(
			FVector(0.0f, MinY, AxisHeightBias),
			FVector(0.0f, MaxY, AxisHeightBias),
			WithAlpha(FColor::Green().ToVector4(), Alpha)
		);
	}

	if (bShowXAxis && bShowYAxis)
	{
		const float AxisHeight = std::max(Spacing * static_cast<float>(BaseHalfCount), Spacing * 10.0f);
		AddLine(
			FVector(0.0f, 0.0f, -AxisHeight),
			FVector(0.0f, 0.0f, AxisHeight),
			FColor::Blue().ToVector4()
		);
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
