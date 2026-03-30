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

        Builder.AddAssetPath("StaticMesh_path", L"Static Mesh", [this]() { return GetMeshPath(); },
                             [this](const FString& InPath) { SetMeshPath(InPath); },
                             StaticMeshPathOptions);
    }

    void UStaticMeshComponent::Serialize(bool bIsLoading, void* JsonHandle)
    {
        UMeshComponent::Serialize(bIsLoading, JsonHandle);
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
                   "Failed to load Static Mesh asset for StaticMeshComponent: %s",
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
        Asset::UMaterialInterface* OverrideMat = UMeshComponent::GetMaterial(Index);
        if (OverrideMat)
        {
            return OverrideMat;
        }

        if (StaticMesh)
        {
            return StaticMesh->GetMaterial(Index);
        }

        return nullptr;
    }

    REGISTER_CLASS(Engine::Component, UStaticMeshComponent)
} // namespace Engine::Component
