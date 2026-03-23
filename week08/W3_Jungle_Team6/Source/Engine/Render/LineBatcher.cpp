#include "LineBatcher.h"
#include "Core/EngineTypes.h"
#include "Editor/Settings/EditorSettings.h"
#include "Math/Utils.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float GridPlaneZ = 0.0f;
	constexpr float GridFadeStartRatio = 0.72f;
	constexpr float AxisFadeStartRatio = 0.9f;
	constexpr float GridMinVisibleAlpha = 0.02f;
	constexpr float AxisMinVisibleAlpha = 0.85f;

	float SnapToGrid(float Value, float Spacing)
	{
		return std::round(Value / Spacing) * Spacing;
	}

	float ComputeLineFade(float OffsetFromFocus, float FadeStart, float FadeEnd)
	{
		if (FadeEnd <= FadeStart)
		{
			return 1.0f;
		}

		const float Normalized = (std::fabs(OffsetFromFocus) - FadeStart) / (FadeEnd - FadeStart);
		return Clamp(1.0f - Normalized, 0.0f, 1.0f);
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

	MaxVertexCount = 4096;

	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.ByteWidth = sizeof(FLineVertex) * MaxVertexCount;
	VBDesc.Usage = D3D11_USAGE_DYNAMIC;
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(Device->CreateBuffer(&VBDesc, nullptr, &VertexBuffer)))
	{
		Device->Release();
		VertexBuffer = nullptr;
		MaxVertexCount = 0;
		return;
	}
}

void FLineBatcher::Release()
{
	if (VertexBuffer)
	{
		VertexBuffer->Release();
		VertexBuffer = nullptr;
	}

	if (Device)
	{
		Device->Release();
		Device = nullptr;
	}

	MaxVertexCount = 0;
	Vertices.clear();
}

void FLineBatcher::AddLine(const FVector& Start, const FVector& End, const FVector4& InColor)
{
	AddLine(Start, End, InColor, InColor);
}

void FLineBatcher::AddLine(const FVector& Start, const FVector& End, const FVector4& StartColor, const FVector4& EndColor)
{
	Vertices.emplace_back(Start, StartColor);
	Vertices.emplace_back(End, EndColor);
}
void FLineBatcher::AddAABB(const FBoundingBox& Box, const FColor& InColor)
{
	const FVector4 BoxColor = InColor.ToVector4();

	const FVector v0 = FVector(Box.Min.X, Box.Min.Y, Box.Min.Z);
	const FVector v1 = FVector(Box.Max.X, Box.Min.Y, Box.Min.Z);
	const FVector v2 = FVector(Box.Max.X, Box.Max.Y, Box.Min.Z);
	const FVector v3 = FVector(Box.Min.X, Box.Max.Y, Box.Min.Z);
	const FVector v4 = FVector(Box.Min.X, Box.Min.Y, Box.Max.Z);
	const FVector v5 = FVector(Box.Max.X, Box.Min.Y, Box.Max.Z);
	const FVector v6 = FVector(Box.Max.X, Box.Max.Y, Box.Max.Z);
	const FVector v7 = FVector(Box.Min.X, Box.Max.Y, Box.Max.Z);

	AddLine(v0, v1, BoxColor);
	AddLine(v1, v2, BoxColor);
	AddLine(v2, v3, BoxColor);
	AddLine(v3, v0, BoxColor);
	AddLine(v4, v5, BoxColor);
	AddLine(v5, v6, BoxColor);
	AddLine(v6, v7, BoxColor);
	AddLine(v7, v4, BoxColor);
	AddLine(v0, v4, BoxColor);
	AddLine(v1, v5, BoxColor);
	AddLine(v2, v6, BoxColor);
	AddLine(v3, v7, BoxColor);
}

//void FLineBatcher::AddWorldGrid(float InGridSpacing, int InHalfGridCount)
//{
//	if (InGridSpacing <= 0.0f)
//	{
//		return;
//	}
//
//	const float Extent = InGridSpacing * static_cast<float>(InHalfGridCount);
//	const FVector4 GridColor = FColor::White().ToVector4();
//
//	for (int32 i = -InHalfGridCount; i <= InHalfGridCount; ++i)
//	{
//		if (i == 0)
//		{
//			continue;
//		}
//
//		const float Offset = static_cast<float>(i) * InGridSpacing;
//
//		AddLine(
//			FVector(-Extent, Offset, GridPlaneZ),
//			FVector(Extent, Offset, GridPlaneZ),
//			GridColor
//		);
//
//		AddLine(
//			FVector(Offset, -Extent, GridPlaneZ),
//			FVector(Offset, Extent, GridPlaneZ),
//			GridColor
//		);
//	}
//}
//
//void FLineBatcher::AddWorldAxes(float InGridSpacing, int InHalfGridCount)
//{
//	if (InGridSpacing <= 0.0f || InHalfGridCount <= 0)
//	{
//		return;
//	}
//
//	const float Extent = InGridSpacing * static_cast<float>(InHalfGridCount);
//
//	AddLine(
//		FVector(-Extent, 0.0f, 0.0f),
//		FVector(Extent, 0.0f, 0.0f),
//		FColor::Red().ToVector4()
//	);
//
//	AddLine(
//		FVector(0.0f, -Extent, 0.0f),
//		FVector(0.0f, Extent, 0.0f),
//		FColor::Green().ToVector4()
//	);
//
//	AddLine(
//		FVector(0.0f, 0.0f, -Extent),
//		FVector(0.0f, 0.0f, Extent),
//		FColor::Blue().ToVector4()
//	);
//}

void FLineBatcher::AddWorldHelpers(const FEditorSettings& Settings, const FVector& CameraPosition, const FVector& CameraForward)
{
	const float Spacing = Settings.GridSpacing;
	const int32 BaseHalfCount = std::max(Settings.GridHalfLineCount, 1);

	if (Spacing <= 0.0f)
	{
		return;
	}

	const FVector FocusPoint = ComputeGridFocusPoint(CameraPosition, CameraForward);
	const float CenterX = SnapToGrid(FocusPoint.X, Spacing);
	const float CenterY = SnapToGrid(FocusPoint.Y, Spacing);
	const int32 DynamicHalfCount = ComputeDynamicHalfCount(Spacing, BaseHalfCount, CameraPosition);
	const float GridExtent = Spacing * static_cast<float>(DynamicHalfCount);
	const float MinX = CenterX - GridExtent;
	const float MaxX = CenterX + GridExtent;
	const float MinY = CenterY - GridExtent;
	const float MaxY = CenterY + GridExtent;

	const float GridFadeStart = GridExtent * GridFadeStartRatio;
	const float AxisFadeStart = GridExtent * AxisFadeStartRatio;
	const float AxisHeightBias = std::max(Spacing * 0.001f, 0.001f);

	const bool bShowXAxis = (MinY <= 0.0f) && (MaxY >= 0.0f);
	const bool bShowYAxis = (MinX <= 0.0f) && (MaxX >= 0.0f);

	if (Settings.ShowFlags.bGrid)
	{
		const FVector4 GridColor = FColor::White().ToVector4();

		for (int32 i = -DynamicHalfCount; i <= DynamicHalfCount; ++i)
		{
			const float WorldY = CenterY + (static_cast<float>(i) * Spacing);
			if (!(bShowXAxis && IsAxisLine(WorldY, Spacing)))
			{
				const float Alpha = ComputeLineFade(WorldY - FocusPoint.Y, GridFadeStart, GridExtent);
				if (Alpha > GridMinVisibleAlpha)
				{
					AddLine(
						FVector(MinX, WorldY, GridPlaneZ),
						FVector(MaxX, WorldY, GridPlaneZ),
						WithAlpha(GridColor, Alpha)
					);
				}
			}

			const float WorldX = CenterX + (static_cast<float>(i) * Spacing);
			if (!(bShowYAxis && IsAxisLine(WorldX, Spacing)))
			{
				const float Alpha = ComputeLineFade(WorldX - FocusPoint.X, GridFadeStart, GridExtent);
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
		const float Alpha = std::max(AxisMinVisibleAlpha, ComputeLineFade(-FocusPoint.Y, AxisFadeStart, GridExtent));
		AddLine(
			FVector(MinX, 0.0f, AxisHeightBias),
			FVector(MaxX, 0.0f, AxisHeightBias),
			WithAlpha(FColor::Red().ToVector4(), Alpha)
		);
	}

	if (bShowYAxis)
	{
		const float Alpha = std::max(AxisMinVisibleAlpha, ComputeLineFade(-FocusPoint.X, AxisFadeStart, GridExtent));
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
	Vertices.clear();
}

void FLineBatcher::Flush(ID3D11DeviceContext* Context)
{
	if (!Context || !Device || Vertices.empty())
	{
		return;
	}

	const uint32 RequiredVertexCount = static_cast<uint32>(Vertices.size());

	if (!VertexBuffer || RequiredVertexCount > MaxVertexCount)
	{
		if (VertexBuffer)
		{
			VertexBuffer->Release();
			VertexBuffer = nullptr;
		}

		MaxVertexCount = RequiredVertexCount * 2;

		D3D11_BUFFER_DESC VBDesc = {};
		VBDesc.ByteWidth = sizeof(FLineVertex) * MaxVertexCount;
		VBDesc.Usage = D3D11_USAGE_DYNAMIC;
		VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		VBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		if (FAILED(Device->CreateBuffer(&VBDesc, nullptr, &VertexBuffer)))
		{
			VertexBuffer = nullptr;
			MaxVertexCount = 0;
			return;
		}
	}

	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	if (FAILED(Context->Map(VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
	{
		return;
	}

	memcpy(MappedResource.pData, Vertices.data(), sizeof(FLineVertex) * RequiredVertexCount);
	Context->Unmap(VertexBuffer, 0);

	UINT Stride = sizeof(FLineVertex);
	UINT Offset = 0;
	Context->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Context->Draw(RequiredVertexCount, 0);
}

uint32 FLineBatcher::GetLineCount() const
{
	return static_cast<uint32>(Vertices.size() / 2);
}
