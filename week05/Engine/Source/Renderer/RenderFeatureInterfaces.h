#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderType.h"

class FMaterial;
struct FRenderMesh;
class UBillboardComponent;

class ENGINE_API ISceneTextFeature
{
public:
	virtual ~ISceneTextFeature() = default;
	virtual FMaterial* GetBaseMaterial() const = 0;
	virtual bool BuildMesh(
		const FString& Text,
		FRenderMesh& OutMesh,
		float LetterSpacing,
		EHorizTextAligment HorizAlignment = EHorizTextAligment::EHTA_Center,
		EVerticalTextAligment VertAlignment = EVerticalTextAligment::EVRTA_TextBottom) const = 0;
};

class ENGINE_API ISceneSubUVFeature
{
public:
	virtual ~ISceneSubUVFeature() = default;
	virtual FMaterial* GetBaseMaterial() const = 0;
	virtual bool BuildMesh(const FVector2& Size, FRenderMesh& OutMesh) const = 0;
};

class ENGINE_API ISceneBillboardFeature
{
public:
	virtual ~ISceneBillboardFeature() = default;
	virtual FMaterial* GetBaseMaterial() const = 0;
	virtual bool BuildMesh(const FVector2& Size, FRenderMesh& OutMesh) const = 0;
	virtual FMaterial* GetOrCreateMaterial(const UBillboardComponent& Component) = 0;
	virtual void PruneMaterials(const TArray<const UBillboardComponent*>& ActiveComponents) = 0;
};
