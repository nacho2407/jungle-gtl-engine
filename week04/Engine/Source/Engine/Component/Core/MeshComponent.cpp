#include "Core/CoreMinimal.h"
#include "Engine/Component/Core/MeshComponent.h"
/*
 * #include "Asset/Material.h"
 * 추가 예정 클래스
 */

namespace Engine::Component
{
    UMeshComponent::UMeshComponent() {}

    UMeshComponent::~UMeshComponent() {}

    void UMeshComponent::Serialize(bool bIsLoading, void* JsonHandle)
    {
        // 공통 메시 직렬화 로직
    }

    void UMeshComponent::InitializeMaterialSlots(uint32 NumSections)
    {
        OverrideMaterials.resize(NumSections, nullptr);
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
