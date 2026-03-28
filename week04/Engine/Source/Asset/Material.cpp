#include "Core/CoreMinimal.h"
#include "Asset/Material.h"

UMaterial::UMaterial() {}

UMaterial::~UMaterial() {}

void UMaterial::Serialize(class FArchive& Ar)
{
    UMaterialInterface::Serialize(Ar);
    // Serialize PBR properties here
}

REGISTER_CLASS(, UMaterial)
