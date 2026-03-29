#include "Core/CoreMinimal.h"
#include "Engine/Component/Core/MeshComponent.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "Asset/MaterialInterface.h"
#include "Asset/Asset.h"

#include <string>
#include <filesystem>

namespace Engine::Component
{
    UMeshComponent::UMeshComponent()
    {
    }

    UMeshComponent::~UMeshComponent()
    {
    }

    //void UMeshComponent::Serialize(bool bIsLoading, void* JsonHandle)
    //{
    //}

    void UMeshComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UPrimitiveComponent::DescribeProperties(Builder);

        // Redundant manual registration removed. 
        // Material slots are now handled via a dedicated UI section in PropertiesPanel.cpp.
    }

    void UMeshComponent::InitializeMaterialSlots(uint32 NumSections)
    {
        if (NumSections != OverrideMaterials.size())
        {
            OverrideMaterials.resize(NumSections, nullptr);
        }
    }

    void UMeshComponent::SetMaterial(uint32 Index, Asset::UMaterialInterface* InMaterial)
    {
        if (Index < OverrideMaterials.size())
        {
            OverrideMaterials[Index] = InMaterial;
        }
    }

    Asset::UMaterialInterface* UMeshComponent::GetMaterial(uint32 Index) const
    {
        if (Index < OverrideMaterials.size())
        {
            return OverrideMaterials[Index];
        }
        return nullptr;
    }

    uint32 UMeshComponent::GetNumMaterials() const
    {
        return static_cast<uint32>(OverrideMaterials.size());
    }

    FString UMeshComponent::WidePathToUtf8(const FWString& Path)
    {
        const std::filesystem::path      FilePath(Path);
        const std::u8string Utf8Path = FilePath.u8string();
        return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
    }
} // namespace Engine::Component
