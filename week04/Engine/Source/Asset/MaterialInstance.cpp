#include "Asset/MaterialInstance.h"
#include "CoreUObject/Object.h"

namespace Engine::Asset
{
    const FMaterialData* UMaterialInstance::GetMaterialData(const FString& SubMaterialName) const
    {
        // 1. 자신의 오버라이드 맵에서 먼저 찾습니다.
        auto It = OverriddenData.find(SubMaterialName);
        if (It != OverriddenData.end())
        {
            return &It->second;
        }

        // 2. 없으면 부모에게 위임합니다.
        if (Parent)
        {
            return Parent->GetMaterialData(SubMaterialName);
        }

        return nullptr;
    }

    void UMaterialInstance::SetMaterialDataOverride(const FString& SubMaterialName, const FMaterialData& NewData)
    {
        OverriddenData[SubMaterialName] = NewData;
    }

    void UMaterialInstance::ClearMaterialDataOverride(const FString& SubMaterialName)
    {
        OverriddenData.erase(SubMaterialName);
    }

    void UMaterialInstance::ClearAllOverrides()
    {
        OverriddenData.clear();
    }

    REGISTER_CLASS(Engine::Asset, UMaterialInstance)
} // namespace Engine::Asset
