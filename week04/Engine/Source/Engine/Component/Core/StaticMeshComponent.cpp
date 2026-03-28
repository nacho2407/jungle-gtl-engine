#include "Core/CoreMinimal.h"
#include "StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "Asset/AssetManager.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include <filesystem>

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
        // JSON 직렬화 로직 추가 지점
    }

    void UStaticMeshComponent::ResolveAssetReferences(UAssetManager* InAssetManager)
    {
        if (InAssetManager == nullptr || PendingMeshPath.empty())
        {
            return;
        }

        // FString(std::string)을 FWString(std::wstring)으로 안전하게 변환
        std::filesystem::path FilePath(PendingMeshPath.c_str());
        FWString WidePath = FilePath.wstring();

        FAssetLoadParams Params;
        Params.ExplicitType = EAssetType::Mesh;

        UAsset* LoadedAsset = InAssetManager->Load(WidePath, Params);
        if (UStaticMesh* NewMesh = Cast<UStaticMesh>(LoadedAsset))
        {
            SetStaticMesh(NewMesh);
            PendingMeshPath = ""; // 로드 완료 후 비움
        }
    }

    void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
    {
        StaticMesh = InStaticMesh;
        if (StaticMesh)
        {
            InitializeMaterialSlots(StaticMesh->GetNumSections());
        }
        else
        {
            InitializeMaterialSlots(0);
        }
    }

    FString UStaticMeshComponent::GetMeshPath() const
    {
        if (!PendingMeshPath.empty()) return PendingMeshPath;
        return StaticMesh ? StaticMesh->GetAssetPathFileName() : "";
    }

    void UStaticMeshComponent::SetMeshPath(const FString& InPath)
    {
        PendingMeshPath = InPath;
        // 엔진 시스템이 ResolveAssetReferences를 호출할 때까지 대기하거나, 
        // 에디터에서 즉시 갱신이 필요한 경우 직접 호출될 수 있습니다.
    }

    bool UStaticMeshComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        OutTriangles.clear();
        return false;
    }

    Geometry::FAABB UStaticMeshComponent::GetLocalAABB() const
    {
        if (StaticMesh && StaticMesh->GetRenderResource())
        {
            return StaticMesh->GetRenderResource()->BoundingBox;
        }
        return Geometry::FAABB(FVector::ZeroVector, FVector::ZeroVector);
    }

    REGISTER_CLASS(Engine::Component, UStaticMeshComponent)
} // namespace Engine::Component
