#include "Core/CoreMinimal.h"
#include "Asset/MaterialInstance.h"

UMaterialInstance::UMaterialInstance()
    : Parent(nullptr), bOverrideBaseColor(false), bOverrideMetallic(false),
      bOverrideRoughness(false)
{
}

UMaterialInstance::~UMaterialInstance() {}

REGISTER_CLASS(, UMaterialInstance)
