// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/Resources/Meshes/PrimitiveMeshTypes.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

// FMeshBufferManager는 관련 객체의 생성, 조회, 수명 관리를 담당합니다.
class FMeshBufferManager : public TSingleton<FMeshBufferManager>
{
    friend class TSingleton<FMeshBufferManager>;

public:
    void Initialize(ID3D11Device* InDevice);
    void Release();

    FMeshBuffer&     GetMeshBuffer(EMeshShape InShape);
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

    TMap<EMeshShape, FMeshData>              MeshDataMap;
    TMap<EMeshShape, TMeshData<FVertexPNCT>> PNCTMeshDataMap;
    TMap<EMeshShape, FMeshBuffer>            MeshBufferMap;

    bool bIsInitialized = false;
};
