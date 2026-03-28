#include "Core/CoreMinimal.h"
#include "Engine/Component/Core/MeshComponent.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include "Asset/MaterialInterface.h"
#include "Asset/Asset.h"

#include <string>

namespace Engine::Component
{
    UMeshComponent::UMeshComponent()
    {
    }

    UMeshComponent::~UMeshComponent()
    {
    }

    void UMeshComponent::Serialize(bool bIsLoading, void* JsonHandle)
    {
    }

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
} // namespace Engine::Component
