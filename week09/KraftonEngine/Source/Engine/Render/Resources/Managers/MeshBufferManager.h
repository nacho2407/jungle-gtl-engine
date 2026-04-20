#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/D3D11/Buffers/Buffers.h"

class FMeshBufferManager : public TSingleton<FMeshBufferManager>
{
    friend class TSingleton<FMeshBufferManager>;

public:
    void Initialize(ID3D11Device* InDevice);
    void Release();

    FMeshBuffer& GetMeshBuffer(EMeshShape InShape);
    const FMeshData& GetMeshData(EMeshShape InShape) const;

private:
    FMeshBufferManager() = default;

    // CPU 메시 데이터 생성
    void CreatePrimitiveMeshData();
    void CreateCube();
    void CreatePlane();
    void CreateSphere(int Slices = 20, int Stacks = 20);
    void CreateTranslationGizmo();
    void CreateRotationGizmo();
    void CreateScaleGizmo();
    void CreateQuad();
    void CreateTexturedQuad();

    // CPU 메시 데이터
    TMap<EMeshShape, FMeshData> MeshDataMap;
    TMap<EMeshShape, TMeshData<FVertexPNCT>> PNCTMeshDataMap;

    // GPU 버퍼
    TMap<EMeshShape, FMeshBuffer> MeshBufferMap;

    bool bIsInitialized = false;
};
