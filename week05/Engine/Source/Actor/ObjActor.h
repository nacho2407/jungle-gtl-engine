#pragma once

#include "Actor.h"

class UPrimitiveComponent;
class URandomColorComponent;
class UObjComponent;
struct ID3D11Device;

class ENGINE_API AObjActor : public AActor
{
public:
	DECLARE_RTTI(AObjActor, AActor)

	void LoadObj(ID3D11Device* Device, const FString& FilePath);
	void PostSpawnInitialize() override;

	UPrimitiveComponent* GetPrimitiveComponent() const { return PrimitiveComponent; }
	URandomColorComponent* GetRandomColorComponent() const { return RandomColorComponent; }

	void SetUseRandomColor(bool bEnable) { bUseRandomColor = bEnable; }
	bool GetUseRandomColor() const { return bUseRandomColor; }

private:
	UObjComponent* ObjComponent = nullptr;
	UPrimitiveComponent* PrimitiveComponent = nullptr;
	URandomColorComponent* RandomColorComponent = nullptr;
	bool bUseRandomColor = true;
};
