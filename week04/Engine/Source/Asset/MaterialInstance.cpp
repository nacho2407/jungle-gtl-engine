#include "Asset/MaterialInstance.h"
#include "CoreUObject/Object.h"

namespace Engine::Asset
{
    const FMaterial* UMaterialInstance::GetMaterialData() const
    {
        // 1. 자신의 오버라이드 맵에서 먼저 찾습니다.
        if (bHasOverride)
        {
            return &OverriddenData;
        }

        // 2. 없으면 부모에게 위임합니다.
        if (Parent)
        {
            return Parent->GetMaterialData();
        }

        return nullptr;
    }

    void UMaterialInstance::SetMaterialDataOverride(const FMaterial& NewData)
    {
        OverriddenData = NewData;
        bHasOverride = true;
    }

    void UMaterialInstance::ClearMaterialDataOverride()
    {
        bHasOverride = false;
    }

    REGISTER_CLASS(Engine::Asset, UMaterialInstance)
} // namespace Engine::Asset
