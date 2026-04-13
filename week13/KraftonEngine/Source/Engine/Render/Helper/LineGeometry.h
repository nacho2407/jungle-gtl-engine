#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ViewTypes.h"
#include "Render/Resource/Buffer.h"

// FLineVertex — 라인 렌더링용 버텍스 (Position + Color)
struct FLineVertex
{
	FVector Position;
	FVector4 Color;

	FLineVertex(const FVector& InPos, const FVector4& InColor) : Position(InPos), Color(InColor) {}
};

// FLineGeometry — 동적 VB/IB를 직접 소유하는 라인 지오메트리 헬퍼.
class FLineGeometry
{
public:
	void Create(ID3D11Device* InDevice);
	void Release();

	void AddLine(const FVector& Start, const FVector& End, const FVector4& Color);
	void AddLine(const FVector& Start, const FVector& End, const FVector4& StartColor, const FVector4& EndColor);
	void AddAABB(const FBoundingBox& Box, const FColor& Color);
	void AddWorldHelpers(const FShowFlags& ShowFlags, float GridSpacing, int32 GridHalfLineCount,
		const FVector& CameraPosition, const FVector& CameraForward, bool bIsOrtho = false);

	void Clear();

	bool UploadBuffers(ID3D11DeviceContext* Context);
	ID3D11Buffer* GetVBBuffer() const { return VB.GetBuffer(); }
	uint32 GetVBStride() const { return VB.GetStride(); }
	ID3D11Buffer* GetIBBuffer() const { return IB.GetBuffer(); }
	uint32 GetIndexCount() const { return static_cast<uint32>(Indices.size()); }
	uint32 GetLineCount() const { return static_cast<uint32>(Indices.size() / 2); }

private:
	TArray<FLineVertex> IndexedVertices;
	TArray<uint32> Indices;

	FDynamicVertexBuffer VB;
	FDynamicIndexBuffer  IB;
	ID3D11Device* Device = nullptr;
};
