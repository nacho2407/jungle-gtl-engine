#pragma once

#include "Object/ObjectFactory.h"
#include "Math/Vector.h"

class UTexture2D;

// class UMaterialInterface
// {
// };

// TODO: 다른 파일에 분리하기
class UMaterial : public UObject // : public UMaterialInterface
{
public:
	DECLARE_CLASS(UMaterial, UObject)

	FString PathFileName;
	FString DiffuseTextureFilePath;
	FVector4 DiffuseColor;
	UTexture2D* DiffuseTexture = nullptr;	// UObjectManager 소유, 여기선 참조만

public:
	const FString& GetAssetPathFileName() const;
};
