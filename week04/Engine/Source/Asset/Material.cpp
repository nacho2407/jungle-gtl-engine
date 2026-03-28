#include "Core/CoreMinimal.h"
#include "Asset/Material.h"

namespace Engine::Asset
{
    UMaterial::UMaterial() {}

    UMaterial::~UMaterial() {}

    void UMaterial::Serialize(class FArchive& Ar) { UMaterialInterface::Serialize(Ar); }

    REGISTER_CLASS(Engine::Asset, UMaterial)
} // namespace Engine::Asset
