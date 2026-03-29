#include "Materials/Material.h"

IMPLEMENT_CLASS(UMaterial, UObject)

const FString& UMaterial::GetAssetPathFileName() const
{
	return PathFileName;
}