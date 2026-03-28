#include "Core/CoreMinimal.h"
#include "Engine/Component/Core/MeshComponent.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "Asset/Asset.h"

#include <string>

namespace Engine::Component
{
    UMeshComponent::UMeshComponent() {}

    UMeshComponent::~UMeshComponent() {}

    void UMeshComponent::Serialize(bool bIsLoading, void* JsonHandle) {}

    void UMeshComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UPrimitiveComponent::DescribeProperties(Builder);

        // Automate Material Slot exposure using standard string conversion
        for (uint32 i = 0; i < GetNumMaterials(); ++i)
        {
            std::string IndexStr = std::to_string(i);
            FString     Key = "MaterialSlot_" + IndexStr;

            std::wstring WIndexStr = std::to_wstring(i);
            FWString     Label = L"Material Slot " + WIndexStr;

            FComponentPropertyOptions Options;
            Options.ExpectedAssetPathKind = EComponentAssetPathKind::TextureImage;

            Builder.AddAssetPath(
                Key, Label,
                [this, i]() -> FString
                {
                    // Safely access UAsset functionality even if UMaterial header is missing
                    UObject* MatObj = (UObject*)GetMaterial(i);
                    if (MatObj)
                    {
                        if (UAsset* Asset = Cast<UAsset>(MatObj))
                        {
                            return Asset->GetAssetName();
                        }
                    }
                    return "";
                },
                [this, i](const FString& NewPath)
                { 
                    // To be implemented with Material Loader
                },
                Options);
        }
    }

    void UMeshComponent::InitializeMaterialSlots(uint32 NumSections)
    {
        if (NumSections != OverrideMaterials.size())
        {
            OverrideMaterials.resize(NumSections, nullptr);
        }
    }

    void UMeshComponent::SetMaterial(uint32 Index, Asset::UMaterial* InMaterial)
    {
        if (Index < OverrideMaterials.size())
        {
            OverrideMaterials[Index] = InMaterial;
        }
    }

    Asset::UMaterial* UMeshComponent::GetMaterial(uint32 Index) const
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
} // namespace Engine::Component
