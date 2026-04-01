#include "Core/CoreMinimal.h"
#include "Engine/Component/Mesh/StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "Asset/AssetManager.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "SceneIO/SceneAssetPath.h"
#include "Asset/MaterialInterface.h"
#include <algorithm>
#include <filesystem>

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
        if (StaticMesh == nullptr)
        {
            return;
        }

        // Sub-material selection removed. Material overrides are managed via MeshComponent.
        /*
        const TArray<Engine::Asset::FMaterialSlot>& SlotRef = StaticMesh->GetMaterialSlots();

        for (int32 idx = 0; idx < static_cast<int32>(SlotRef.size()); ++idx)
        {
            const FString  PropertyName = "StaticMeshMaterialSlot_" + std::to_string(idx);
            const FWString DisplayName = L"Material " + std::to_wstring(idx);

            Builder.AddMaterialSlot(
                PropertyName, DisplayName,
                [this, idx]() -> FString
                {
                    if (this->StaticMesh == nullptr)
                    {
                        return "";
                    }

                    return this->GetSubMaterialName(idx);
                },
                [this, idx](const FString& InSubMaterial)
                {
                    this->SetSubMaterialName(idx, InSubMaterial);
                },
                [this]() -> TArray<FString>
                {
                    return this->GetAvailableSubMaterialNames();
                },
                FComponentPropertyOptions{});
        }
        */
    }

    void UStaticMeshComponent::Serialize(bool bIsLoading, void* JsonHandle)
    {
        UMeshComponent::Serialize(bIsLoading, JsonHandle);
    }

    void UStaticMeshComponent::ResolveAssetReferences(UAssetManager* InAssetManager)
    {
        if (InAssetManager == nullptr || PendingMeshPath.empty())
        {
            return;
        }

        const std::filesystem::path AbsolutePath = Engine::SceneIO::ResolveSceneAssetPathToAbsolute(PendingMeshPath);
        if (AbsolutePath.empty())
        {
            return;
        }

        FAssetLoadParams Params;
        Params.ExplicitType = EAssetType::StaticMesh;

        UAsset* LoadedAsset = InAssetManager->Load(AbsolutePath.wstring(), Params);
        if (Asset::UStaticMesh* NewMesh = Cast<Asset::UStaticMesh>(LoadedAsset))
        {
            SetStaticMesh(NewMesh);
            // PendingMeshPath = ""; // 로드 완료 후 비움
        }
    }

    void UStaticMeshComponent::SetStaticMesh(Asset::UStaticMesh* InStaticMesh)
    {
        StaticMesh = InStaticMesh;
        if (StaticMesh && StaticMesh->GetRenderResource())
        {
            uint32 NumSubMeshes = static_cast<uint32>(StaticMesh->GetRenderResource()->SubMeshes.size());
            InitializeMaterialSlots(NumSubMeshes);
        }
        else if (StaticMesh)
        {
            InitializeMaterialSlots(StaticMesh->GetNumSections());
        }
        else
        {
            InitializeMaterialSlots(0);
        }

        OnTransformChanged();
    }

    FString UStaticMeshComponent::GetMeshPath() const
    {
        if (!PendingMeshPath.empty()) return PendingMeshPath;
        return StaticMesh ? WidePathToUtf8(StaticMesh->GetPath()) : "";
    }

    void UStaticMeshComponent::SetMeshPath(const FString& InPath)
    {
        PendingMeshPath = InPath;
        StaticMesh = nullptr;
    }

    bool UStaticMeshComponent::GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const
    {
        OutTriangles.clear();
        if (StaticMesh == nullptr || StaticMesh->GetRenderResource() == nullptr)
        {
            return false;
        }

        const FStaticMesh* Resource = StaticMesh->GetRenderResource();
        if (Resource->CPU_Positions.empty() || Resource->CPU_Indices.size() < 3)
        {
            return false;
        }

        OutTriangles.reserve(Resource->CPU_Indices.size() / 3);

        for (size_t Index = 0; Index + 2 < Resource->CPU_Indices.size(); Index += 3)
        {
            const uint32 I0 = Resource->CPU_Indices[Index];
            const uint32 I1 = Resource->CPU_Indices[Index + 1];
            const uint32 I2 = Resource->CPU_Indices[Index + 2];

            if (I0 >= Resource->CPU_Positions.size() || I1 >= Resource->CPU_Positions.size() ||
                I2 >= Resource->CPU_Positions.size())
            {
                continue;
            }

            OutTriangles.emplace_back(Resource->CPU_Positions[I0], Resource->CPU_Positions[I1],
                                      Resource->CPU_Positions[I2]);
        }

        return !OutTriangles.empty();
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
        // 1. 컴포넌트 레벨에서 오버라이드된 재질이 있는지 확인
        Asset::UMaterialInterface* OverrideMat = UMeshComponent::GetMaterial(Index);
        if (OverrideMat)
        {
            return OverrideMat;
        }

        // 2. 없으면 에셋 레벨의 재질 반환
        if (StaticMesh)
        {
            return StaticMesh->GetMaterial(Index);
        }

        return nullptr;
    }

    uint32 UStaticMeshComponent::GetNumMaterials() const
    {
        if (StaticMesh)
        {
            return StaticMesh->GetNumSections();
        }
        return UMeshComponent::GetNumMaterials();
    }

    REGISTER_CLASS(Engine::Component, UStaticMeshComponent)
} // namespace Engine::Component
