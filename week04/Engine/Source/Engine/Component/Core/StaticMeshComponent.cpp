#include "Core/CoreMinimal.h"
#include "StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "Engine/Component/Core/ComponentProperty.h"

namespace Engine::Component
{
    UStaticMeshComponent::UStaticMeshComponent() {}

    UStaticMeshComponent::~UStaticMeshComponent() {}

    EBasicMeshType UStaticMeshComponent::GetBasicMeshType() const { return EBasicMeshType::None; }

    void UStaticMeshComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        // 부모 클래스 기능(Transform 등) 추가
        UPrimitiveComponent::DescribeProperties(Builder);

        FComponentPropertyOptions Options;
        Options.ExpectedAssetPathKind = EComponentAssetPathKind::StaticMeshFile;

        // Key값
        Builder.AddAssetPath(
            "ObjStaticMeshAsset", L"Static Mesh", [this]() { return GetMeshPath(); },
            [this](const FString& InPath) { SetMeshPath(InPath); }, Options);
    }

    void UStaticMeshComponent::Serialize(bool bIsLoading, void* JsonHandle)
    {
        if (bIsLoading)
        {
            // TODO: FObjManager를 통한 에셋 로드 연동
            // StaticMesh = FObjManager::LoadObjStaticMesh(AssetPath);
        }
    }

    FString UStaticMeshComponent::GetMeshPath() const
    {
        return StaticMesh ? StaticMesh->GetAssetPathFileName() : "";
    }

    void UStaticMeshComponent::SetMeshPath(const FString& InPath)
    {
        // TODO: FObjManager 연동
    }

    bool UStaticMeshComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        return false;
    }

    Geometry::FAABB UStaticMeshComponent::GetLocalAABB() const { return Geometry::FAABB(); }

    REGISTER_CLASS(Engine::Component, UStaticMeshComponent)
} // namespace Engine::Component