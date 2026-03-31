#pragma once

#include "Object/ObjectFactory.h"
#include "Math/Vector.h"

class UTexture2D;
class FArchive;

// class UMaterialInterface
// {
// };

class UMaterial : public UObject // : public UMaterialInterface
{
public:
	DECLARE_CLASS(UMaterial, UObject)

	FString PathFileName;					// 어떤 Material인지 판별하는 고유 이름
	FString DiffuseTextureFilePath;
	FVector4 DiffuseColor;
	UTexture2D* DiffuseTexture = nullptr;	// UObjectManager 소유, 여기선 참조만

public:
	const FString& GetAssetPathFileName() const;
	void Serialize(FArchive& Ar);
};
