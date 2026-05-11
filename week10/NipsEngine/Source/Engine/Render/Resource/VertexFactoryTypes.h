#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/String.h"
#include "Render/Resource/ShaderPaths.h"
#include "Render/Resource/ShaderTypes.h"

struct FRenderCommand;
struct ID3D11DeviceContext;

// Mesh Vertex 데이터를 어떤 방식으로 해석할지 나타내는 타입입니다.
// Material이 Static/Skeletal 여부를 알지 않도록 RenderCommand가 이 값을 들고 갑니다.
enum class EVertexFactoryType : uint8
{
    StaticMesh,
    SkeletalMesh,
    ProceduralMesh,
    Primitive,
    Billboard,
    SubUV,
    Line,
    Text,
    Gizmo,
    Decal,
};

// VertexFactory별 Shader Entry 정책입니다.
// 같은 Material PS라도 StaticMeshVS / SkeletalMeshVS처럼 VS만 갈아끼울 수 있게 분리합니다.
struct FVertexFactoryDesc
{
    FString VertexShaderPath;
    FString DepthPassVSPath;
    FString ShadowPassVSPath;
    FString SelectionPassVSPath;
    FString BasePassVSEntry;
    FString DepthPassVSEntry;
    FString ShadowPassVSEntry;
    FString SelectionPassVSEntry;
    FVertexLayoutDesc VertexLayout;
};

class FVertexFactoryRegistry
{
public:
    // 초기 단계에서는 과한 상속 구조 대신 Enum -> Desc 매핑으로 관리합니다.
    // GPU Skinning처럼 리소스 바인딩 규칙이 복잡해지면 객체 모델로 확장하면 됩니다.
    static const FVertexFactoryDesc& Get(EVertexFactoryType Type)
    {
        static const FVertexFactoryDesc StaticMeshDesc = {
            FShaderPaths::MaterialUberLit,
            FShaderPaths::DepthPrepass,
            FShaderPaths::Shadow,
            FShaderPaths::EditorSelectionMask,
            "mainVS",
            "DepthPrepassVS",
            "ShadowVS",
            "VSStaticMesh",
            {}
        };
        static const FVertexFactoryDesc SkeletalMeshDesc = {
            FShaderPaths::MaterialUberLit,
            FShaderPaths::DepthPrepass,
            FShaderPaths::Shadow,
            FShaderPaths::EditorSelectionMask,
            "SkeletalMeshVS",
            "DepthPrepassVS",
            "ShadowVS",
            "VSSkeletalMesh",
            {}
        };
        static const FVertexFactoryDesc DecalDesc = {
            FShaderPaths::MaterialDecal,
            FShaderPaths::DepthPrepass,
            FShaderPaths::Shadow,
            FShaderPaths::EditorSelectionMask,
            "mainVS",
            "DepthPrepassVS",
            "ShadowVS",
            "VSStaticMesh",
            {}
        };
        static const FVertexFactoryDesc GizmoDesc = {
            FShaderPaths::EditorGizmo,
            FShaderPaths::EditorGizmo,
            FShaderPaths::EditorGizmo,
            FShaderPaths::EditorGizmo,
            "VS",
            "VS",
            "VS",
            "VS",
            {}
        };

        switch (Type)
        {
        case EVertexFactoryType::SkeletalMesh:
            return SkeletalMeshDesc;
        case EVertexFactoryType::Decal:
            return DecalDesc;
        case EVertexFactoryType::Gizmo:
            return GizmoDesc;
        case EVertexFactoryType::StaticMesh:
        case EVertexFactoryType::ProceduralMesh:
        default:
            return StaticMeshDesc;
        }
    }
};

inline void BindVertexFactoryResources(
    ID3D11DeviceContext* Context,
    EVertexFactoryType Type,
    const FRenderCommand& Cmd)
{
    // 현재 CPU Skinning은 이미 갱신된 VertexBuffer를 넘기므로 추가 리소스가 없습니다.
    // 이후 GPU Skinning을 넣으면 여기서 BoneMatrixBuffer 같은 VF 전용 리소스를 바인딩합니다.
    (void)Context;
    (void)Type;
    (void)Cmd;
}
