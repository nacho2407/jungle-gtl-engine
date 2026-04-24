// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

/*
    기본 프리미티브 메시 종류를 정의하는 enum입니다.
    주로 메시 버퍼 매니저가 내부 기본 메시를 조회할 때 사용합니다.
*/
enum class EPrimitiveMeshShape
{
    Cube,
    Sphere,
    Plane,
    Quad,
    TexturedQuad,
    TransGizmo,
    RotGizmo,
    ScaleGizmo,
};

// FMeshBufferManager는 관련 객체의 생성, 조회, 수명 관리를 담당합니다.
class FMeshBufferManager : public TSingleton<FMeshBufferManager>
{
    friend class TSingleton<FMeshBufferManager>;

public:
    void Initialize(ID3D11Device* InDevice);
    void Release();

    FMeshBuffer&     GetMeshBuffer(EPrimitiveMeshShape InShape);
    const FMeshData& GetMeshData(EPrimitiveMeshShape InShape) const;

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

    TMap<EPrimitiveMeshShape, FMeshData>              MeshDataMap;
    TMap<EPrimitiveMeshShape, TMeshData<FVertexPNCT>> PNCTMeshDataMap;
    TMap<EPrimitiveMeshShape, FMeshBuffer>            MeshBufferMap;

    bool bIsInitialized = false;
};
