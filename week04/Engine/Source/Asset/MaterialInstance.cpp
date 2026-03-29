#include "Core/CoreMinimal.h"
#include "MaterialInstance.h"
#include "Renderer/RenderAsset/MaterialResource.h"

namespace Engine::Asset
{
    void UMaterialInstance::SetParent(UMaterialInterface* InParent) { Parent = InParent; }

    const FMaterialData* UMaterialInstance::GetMaterialData(const FString& SubMaterialName) const
    {
        // 1. 이 인스턴스에서 오버라이드(수정)한 데이터가 있는지 맵에서 검사합니다.
        auto It = OverriddenData.find(SubMaterialName);
        if (It != OverriddenData.end())
        {
            // 내가 수정한 적이 있다면 내 데이터를 반환
            return &It->second;
        }

        // 2. 수정한 적이 없다면 부모(원본 Asset 또는 상위 Instance)에게 물어봅니다.
        if (Parent)
        {
            return Parent->GetMaterialData(SubMaterialName);
        }

        // 3. 부모도 없고 오버라이드 데이터도 없으면 nullptr 반환
        return nullptr;
    }

    void UMaterialInstance::SetMaterialDataOverride(const FString&       SubMaterialName,
                                                    const FMaterialData& NewData)
    {
        // 맵에 데이터를 추가하거나 기존 데이터를 덮어씌웁니다.
        OverriddenData[SubMaterialName] = NewData;
    }

    void UMaterialInstance::ClearMaterialDataOverride(const FString& SubMaterialName)
    {
        // 맵에서 데이터를 지우면, 다음 GetMaterialData 호출 시 자연스럽게 부모의 값을 참조하게
        // 됩니다.
        OverriddenData.erase(SubMaterialName);
    }

    void UMaterialInstance::ClearAllOverrides() { OverriddenData.clear(); }

    REGISTER_CLASS(Engine::Asset, UMaterialInstance)
} // namespace Engine::Asset