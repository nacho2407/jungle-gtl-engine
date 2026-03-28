#include "Core/CoreMinimal.h"
#include "Asset/MaterialInstance.h"

namespace Engine::Asset
{
    UMaterialInstance::UMaterialInstance()
        : Parent(nullptr), bOverrideBaseColor(false), bOverrideMetallic(false),
          bOverrideRoughness(false)
    {
    }

    UMaterialInstance::~UMaterialInstance() {}

    REGISTER_CLASS(Engine::Asset, UMaterialInstance)
} // namespace Engine::Asset
