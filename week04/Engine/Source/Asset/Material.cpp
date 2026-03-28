#include "Core/CoreMinimal.h"
#include "Asset/Material.h"

namespace Engine::Asset
{
    UMaterial::UMaterial() = default;
    UMaterial::~UMaterial() = default;

    const FMaterialData* UMaterial::GetMaterialData() const
    {
        if (SharedResource == nullptr || MaterialName.empty())
        {
            return nullptr;
        }
        return SharedResource->GetMaterial(MaterialName);
    }

    void UMaterial::Serialize(class FArchive& Ar)
    {
        UMaterialInterface::Serialize(Ar);
        // 추가 속성 직렬화 (나중에 구현)
    }

    REGISTER_CLASS(Engine::Asset, UMaterial)
} // namespace Engine::Asset
