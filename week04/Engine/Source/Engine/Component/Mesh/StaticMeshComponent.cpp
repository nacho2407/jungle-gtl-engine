#include "Core/CoreMinimal.h"
#include "Engine/Component/Mesh/StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "Asset/AssetManager.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "SceneIO/SceneAssetPath.h"
#include <filesystem>

namespace Engine::Component
{
    EBasicMeshType UStaticMeshComponent::GetBasicMeshType() const
    {
        return EBasicMeshType::None;
    }

    void UStaticMeshComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UMeshComponent::DescribeProperties(Builder);

        FComponentPropertyOptions StaticMeshPathOptions;
        StaticMeshPathOptions.ExpectedAssetPathKind = EComponentAssetPathKind::StaticMeshFile;

        Builder.AddAssetPath(
            "StaticMesh_path", L"Static Mesh", 
            [this]() { return GetMeshPath(); },
            [this](const FString& InPath) { SetMeshPath(InPath); }, 
            StaticMeshPathOptions);
    }

    void UStaticMeshComponent::Serialize(bool bIsLoading, void* JsonHandle)
    {
        UMeshComponent::Serialize(bIsLoading, JsonHandle);
        // JSON 직렬화 로직 추가 지점
    }

    void UStaticMeshComponent::ResolveAssetReferences(UAssetManager* InAssetManager)
    {
        if (InAssetManager == nullptr || StaticMeshPath.empty())
        {
            return;
        }

        const std::filesystem::path AbsolutePath =
            Engine::SceneIO::ResolveSceneAssetPathToAbsolute(StaticMeshPath);

        if (AbsolutePath.empty())
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "[StaticMeshComponent] Failed to resolve mesh path: %s", StaticMeshPath.c_str());
            return;
        }

        FAssetLoadParams LoadParams;
        LoadParams.ExplicitType = EAssetType::StaticMesh;

        UAsset* LoadedAsset = InAssetManager->Load(AbsolutePath, LoadParams);
        Asset::UStaticMesh* NewMesh = Cast<Asset::UStaticMesh>(LoadedAsset);
        if (NewMesh == nullptr)
        {
            UE_LOG(Asset, ELogVerbosity::Warning,
                   "Failed to load sprite atlas asset for SubUVComponent: %s",
                   StaticMeshPath.c_str());
            return;
        }
        SetStaticMesh(NewMesh);
    }

    void UStaticMeshComponent::SetStaticMesh(Asset::UStaticMesh* InStaticMesh)
    {
        StaticMesh = InStaticMesh;
        if (StaticMesh && StaticMesh->GetRenderResource())
        {
            uint32 NumSubMeshes =
                static_cast<uint32>(StaticMesh->GetRenderResource()->SubMeshes.size());
            InitializeMaterialSlots(NumSubMeshes);
        }
        else
        {
            InitializeMaterialSlots(0);
        }
    }

    FString UStaticMeshComponent::GetMeshPath() const
    {
        if (!StaticMeshPath.empty())
        {
            return StaticMeshPath;
        }
        return StaticMesh ? WidePathToUtf8(StaticMesh->GetPath()) : "";
    }

    void UStaticMeshComponent::SetMeshPath(const FString& InPath)
    {
        StaticMeshPath = InPath;
        StaticMesh = nullptr;
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


    Asset::UMaterialInterface* UStaticMeshComponent::GetMaterial(uint32 Index) const
    {
        // 1. 컴포넌트 수준에서 오버라이드 한 것이 있는지 먼저 확인
        Asset::UMaterialInterface* OverrideMat = UMeshComponent::GetMaterial(Index);
        if (OverrideMat)
        {
            return OverrideMat;
        }

        // 2. 오버라이드 된 게 없으면 원본 스태틱 메시 에셋에서 가져옴
        if (StaticMesh)
        {
            // (StaticMesh 클래스 내부에 Index를 받아 UMaterialAsset을 반환하는 함수가 있다고 가정)
            // return StaticMesh->GetDefaultMaterial(Index);
        }

        return nullptr; // 아무것도 없으면 nullptr
    }

    REGISTER_CLASS(Engine::Component, UStaticMeshComponent)
} // namespace Engine::Component
