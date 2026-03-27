#pragma once
#include "PrimitiveComponent.h"

class FMaterial;

class UMeshComponent : public UPrimitiveComponent
{
private:
	// TODO : 머티리얼 교체지원 FMaterial -> UMaterial
	TArray<FMaterial*> OverrideMaterial;

public:
	DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)

};
