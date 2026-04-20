#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Types/PrimitiveShapeTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

/*
    엔진이 자주 쓰는 기본 프리미티브 메시와 대응 GPU 버퍼를 관리하는 매니저입니다.
*/
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

    void CreatePrimitiveMeshData();
    void CreateCube();
    void CreatePlane();
    void CreateSphere(int Slices = 20, int Stacks = 20);
    void CreateTranslationGizmo();
    void CreateRotationGizmo();
    void CreateScaleGizmo();
    void CreateQuad();
    void CreateTexturedQuad();

    TMap<EMeshShape, FMeshData> MeshDataMap;
    TMap<EMeshShape, TMeshData<FVertexPNCT>> PNCTMeshDataMap;
    TMap<EMeshShape, FMeshBuffer> MeshBufferMap;

    bool bIsInitialized = false;
};
