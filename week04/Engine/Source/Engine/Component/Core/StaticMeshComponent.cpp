#include "Core/CoreMinimal.h"
#include "StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "Engine/Component/Core/ComponentProperty.h"

namespace Engine::Component
{
    UStaticMeshComponent::UStaticMeshComponent() : StaticMesh(nullptr) {}

    UStaticMeshComponent::~UStaticMeshComponent() {}

    EBasicMeshType UStaticMeshComponent::GetBasicMeshType() const { return EBasicMeshType::None; }

    void UStaticMeshComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UMeshComponent::DescribeProperties(Builder);

        FComponentPropertyOptions Options;
        Options.ExpectedAssetPathKind = EComponentAssetPathKind::StaticMeshFile;

        Builder.AddAssetPath(
            "ObjStaticMeshAsset", L"Static Mesh", [this]() { return GetMeshPath(); },
            [this](const FString& InPath) { SetMeshPath(InPath); }, Options);
    }

    void UStaticMeshComponent::Serialize(bool bIsLoading, void* JsonHandle)
    {
        UMeshComponent::Serialize(bIsLoading, JsonHandle);
    }

    void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
    {
        StaticMesh = InStaticMesh;
        if (StaticMesh)
        {
            // 에셋이 변경되면 머티리얼 슬롯 수를 동기화합니다.
        }
    }

    FString UStaticMeshComponent::GetMeshPath() const
    {
        return StaticMesh ? StaticMesh->GetAssetPathFileName() : "";
    }

    void UStaticMeshComponent::SetMeshPath(const FString& InPath)
    {
        // FObjManager 연동 예정
    }

    bool UStaticMeshComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        OutTriangles.clear();
        return false;
    }

    Geometry::FAABB UStaticMeshComponent::GetLocalAABB() const
    {
        return Geometry::FAABB(FVector::ZeroVector, FVector::ZeroVector);
    }

    REGISTER_CLASS(Engine::Component, UStaticMeshComponent)
} // namespace Engine::Component
