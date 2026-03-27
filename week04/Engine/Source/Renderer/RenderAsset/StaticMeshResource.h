#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/D3D11/D3D11Common.h"

struct FSubMesh
{
    FString DefaultMaterialName; // .mtl에서 읽어온 기본 머티리얼 이름 (슬롯 식별용)
    uint32  StartIndexLocation = 0;
    uint32  IndexCount = 0;
};

struct FStaticMeshResource
{
    // --- GPU 렌더링용 데이터 (VRAM) ---
    TComPtr<ID3D11Buffer> VertexBuffer;
    TComPtr<ID3D11Buffer> IndexBuffer;

    uint32 VertexCount = 0;
    uint32 IndexCount = 0;
    uint32 VertexStride = 0;

    TArray<FSubMesh> SubMeshes;

    // --- CPU 충돌/Picking용 데이터 (RAM) ---
    TArray<FVector> CPU_Positions;
    TArray<uint32>  CPU_Indices;
    Geometry::FAABB BoundingBox;

    void Reset()
    {
        VertexBuffer.Reset();
        IndexBuffer.Reset();
        VertexCount = 0;
        IndexCount = 0;
        VertexStride = 0;
        SubMeshes.clear();
        CPU_Positions.clear();
        CPU_Indices.clear();
        BoundingBox = Geometry::FAABB();
    }
};