#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Execute/Context/Scene/ViewTypes.h"
#include "Render/Submission/Batching/BatchBuffer.h"

// FLineVertex는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FLineVertex
{
    FVector  Position;
    FVector4 Color;

    FLineVertex() = default;
    FLineVertex(const FVector& InPos, const FVector4& InColor)
        : Position(InPos), Color(InColor) {}
};

// FLineBatch는 렌더 영역의 핵심 동작을 담당합니다.
class FLineBatch
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

    bool          UploadBuffers(ID3D11DeviceContext* Context);
    ID3D11Buffer* GetVBBuffer() const { return LineBuffer.GetVBBuffer(); }
    uint32        GetVBStride() const { return LineBuffer.GetVBStride(); }
    ID3D11Buffer* GetIBBuffer() const { return LineBuffer.GetIBBuffer(); }
    uint32        GetIndexCount() const { return LineBuffer.GetIndexCount(); }
    uint32        GetLineCount() const { return static_cast<uint32>(LineBuffer.Indices.size() / 2); }

private:
    TBatchBuffer<FLineVertex> LineBuffer;
};
