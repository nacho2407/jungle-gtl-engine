#pragma once
#include "PrimitiveComponent.h"

class FMaterial;
struct ID3D11Device;

class ENGINE_API UObjComponent : public UPrimitiveComponent
{
public:
	DECLARE_RTTI(UObjComponent, UPrimitiveComponent)

	void Initialize();

	void LoadPrimitive(const FString& FilePath);
	void LoadTexture(ID3D11Device* Device, const FString& FilePath);

private:
	std::unique_ptr<FMaterial> DynamicMaterialOwner;

};
