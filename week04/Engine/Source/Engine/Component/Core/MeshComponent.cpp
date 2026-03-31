#include "Engine/Component/Core/PrimitiveComponent.h"
#include "Engine/Component/Core/MeshComponent.h"
#include "Engine/Component/Core/ComponentProperty.h"
#include <algorithm>
#include <filesystem>

namespace Engine::Component
{
    UMeshComponent::UMeshComponent() {}

    UMeshComponent::~UMeshComponent() {}

    void UMeshComponent::Serialize(bool bIsLoading, void* JsonHandle)
    {
        UPrimitiveComponent::Serialize(bIsLoading, JsonHandle);
    }

    void UMeshComponent::InitializeMaterialSlots(uint32 NumSections)
    {
        OverrideMaterials.resize(NumSections, nullptr);
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
        return (Index < OverrideMaterials.size()) ? OverrideMaterials[Index] : nullptr;
    }

    uint32 UMeshComponent::GetNumMaterials() const
    {
        return static_cast<uint32>(OverrideMaterials.size());
    }

    FString UMeshComponent::GetSubMaterialName(uint32 Index) const
    {
        return "";
    }

    void UMeshComponent::DescribeProperties(FComponentPropertyBuilder& Builder)
    {
        UPrimitiveComponent::DescribeProperties(Builder);
    }

    FString UMeshComponent::WidePathToUtf8(const FWString& Path)
    {
        std::filesystem::path FilePath(Path);
        const std::u8string   Utf8Path = FilePath.u8string();
        return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
    }

    // REGISTER_CLASS 제거 (추상 클래스이므로 인스턴스화 불가)
} // namespace Engine::Component
