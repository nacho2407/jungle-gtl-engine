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
    // 1. 렌더러가 사용할 GPU 버퍼 (VRAM)
    TComPtr<ID3D11Buffer> VertexBuffer;
    TComPtr<ID3D11Buffer> IndexBuffer;
    uint32                VertexCount = 0;
    uint32                IndexCount = 0;

    TArray<FSubMesh> SubMeshes;

    // 2. 컴포넌트가 Picking에 사용할 CPU 복사본 (RAM)
    TArray<FVector> CPU_Positions;
    TArray<uint32>  CPU_Indices;
    Geometry::FAABB BoundingBox;

    // ... Reset() 등 생략 ...
};