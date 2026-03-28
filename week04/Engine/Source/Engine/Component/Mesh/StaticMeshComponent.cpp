#include "Core/CoreMinimal.h"
#include "Engine/Component/Mesh/StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "Engine/Component/Core/ComponentProperty.h"

namespace Engine::Component
{
    UStaticMeshComponent::UStaticMeshComponent() : StaticMesh(nullptr) {}

    UStaticMeshComponent::~UStaticMeshComponent() {}

    EBasicMeshType UStaticMeshComponent::GetBasicMeshType() const
    {
        return EBasicMeshType::None;
    }

    void UStaticMeshComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UMeshComponent::DescribeProperties(Builder);

        FComponentPropertyOptions Options;
        Options.ExpectedAssetPathKind = EComponentAssetPathKind::StaticMeshFile;

        Builder.AddAssetPath(
            "ObjStaticMeshAsset", L"Static Mesh", 
            [this]() { return GetMeshPath(); },
            [this](const FString& InPath) { SetMeshPath(InPath); }, 
            Options);
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
            // 메시의 섹션 개수에 맞춰 머티리얼 슬롯 자동 조절!
            InitializeMaterialSlots(StaticMesh->GetNumSections());
        }
        else
        {
            // 메시가 없으면 머티리얼 슬롯도 존재할 수 없습니다.
            InitializeMaterialSlots(0);
        }
    }

    FString UStaticMeshComponent::GetMeshPath() const
    {
        return StaticMesh ? StaticMesh->GetAssetPathFileName() : "";
    }

    void UStaticMeshComponent::SetMeshPath(const FString& InPath)
    {
        if (InPath.empty()) return;
        // FObjManager linkage needed
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
