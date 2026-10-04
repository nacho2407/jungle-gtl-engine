#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/ShaderTypes.h"
#include "Render/Resource/VertexTypes.h"

#include <cstddef>

// Mesh Vertex 데이터를 어떤 방식으로 해석할지 나타내는 타입입니다.
// Material이 Static/Skeletal 여부를 알지 않도록 RenderCommand가 이 값을 들고 갑니다.
enum class EVertexFactoryType : uint8
{
    StaticMesh,
    SkeletalMesh, // Only for GPU skinning, CPU skining uses StaticMesh
    ProceduralMesh,
    Primitive,
    Billboard,
    SubUV,
    Line,
    Text,
    Gizmo,
    Decal,
};

struct FVertexFactoryDesc
{
    FVertexLayoutDesc VertexLayout;
    FVertexLayoutDesc PositionOnlyLayout;
    FVertexLayoutDesc SelectionLayout;
};

class FVertexFactoryRegistry
{
public:
    // 초기 단계에서는 과한 상속 구조 대신 Enum -> Desc 매핑으로 관리합니다.
    // GPU Skinning처럼 리소스 바인딩 규칙이 복잡해지면 객체 모델로 확장하면 됩니다.
    static const FVertexFactoryDesc& Get(EVertexFactoryType Type)
    {
        static const FVertexLayoutDesc NormalVertexLayout = {
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, Position)) },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, Color)) },
                { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, Normal)) },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, UVs)) },
                { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FNormalVertex, Tangent)) },
            },
            sizeof(FNormalVertex)
        };
        static const FVertexLayoutDesc SkeletalVertexLayout = {
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FSkeletalMeshVertex, Position)) },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FSkeletalMeshVertex, Color)) },
                { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FSkeletalMeshVertex, Normal)) },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<uint32>(offsetof(FSkeletalMeshVertex, UVs)) },
                { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FSkeletalMeshVertex, Tangent)) },
                { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, static_cast<uint32>(offsetof(FSkeletalMeshVertex, BoneIndices)) },
                { "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FSkeletalMeshVertex, BoneWeights)) },
            },
            sizeof(FSkeletalMeshVertex)
        };
        static const FVertexLayoutDesc PrimitiveVertexLayout = {
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FVertex, Position)) },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FVertex, Color)) },
            },
            sizeof(FVertex)
        };
        static const FVertexLayoutDesc TextureVertexLayout = {
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FTextureVertex, Position)) },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<uint32>(offsetof(FTextureVertex, TexCoord)) },
                { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FTextureVertex, Color)) },
            },
            sizeof(FTextureVertex)
        };
        static const FVertexLayoutDesc TexturePositionUVLayout = {
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<uint32>(offsetof(FTextureVertex, Position)) },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<uint32>(offsetof(FTextureVertex, TexCoord)) },
            },
            sizeof(FTextureVertex)
        };
        static const FVertexLayoutDesc PositionOnlyLayout = {
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0 },
            },
            0
        };

        static const FVertexFactoryDesc StaticMeshDesc = {
            NormalVertexLayout,
            PositionOnlyLayout,
            NormalVertexLayout
        };
        static const FVertexFactoryDesc SkeletalMeshDesc = {
            SkeletalVertexLayout,
            PositionOnlyLayout,
            SkeletalVertexLayout
        };
        static const FVertexFactoryDesc DecalDesc = {
            NormalVertexLayout,
            PositionOnlyLayout,
            NormalVertexLayout
        };
        static const FVertexFactoryDesc GizmoDesc = {
            PrimitiveVertexLayout,
            PrimitiveVertexLayout,
            PrimitiveVertexLayout
        };
        static const FVertexFactoryDesc PrimitiveDesc = {
            PrimitiveVertexLayout,
            PositionOnlyLayout,
            PrimitiveVertexLayout
        };
        static const FVertexFactoryDesc TexturedQuadDesc = {
            TextureVertexLayout,
            PositionOnlyLayout,
            PrimitiveVertexLayout
        };
        static const FVertexFactoryDesc TextDesc = {
            TexturePositionUVLayout,
            PositionOnlyLayout,
            PrimitiveVertexLayout
        };

        switch (Type)
        {
        case EVertexFactoryType::SkeletalMesh:
            return SkeletalMeshDesc;
        case EVertexFactoryType::Decal:
            return DecalDesc;
        case EVertexFactoryType::Gizmo:
            return GizmoDesc;
        case EVertexFactoryType::Primitive:
        case EVertexFactoryType::Line:
            return PrimitiveDesc;
        case EVertexFactoryType::Billboard:
        case EVertexFactoryType::SubUV:
            return TexturedQuadDesc;
        case EVertexFactoryType::Text:
            return TextDesc;
        case EVertexFactoryType::StaticMesh:
        case EVertexFactoryType::ProceduralMesh:
        default:
            return StaticMeshDesc;
        }
    }
};
