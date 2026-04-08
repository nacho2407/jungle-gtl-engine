#pragma once

#include "CoreMinimal.h"

class FMaterial;
struct FRenderMesh;

/**
 * Scene/UI text primitive를 mesh/material로 변환하는 공통 계약.
 * frontend/renderer 코어 결합을 줄이기 위한 얇은 인터페이스다.
 */
class ENGINE_API ISceneTextFeature
{
public:
	virtual ~ISceneTextFeature() = default;
	virtual FMaterial* GetBaseMaterial() const = 0;
	virtual bool BuildMesh(const FString& Text, FRenderMesh& OutMesh, float LetterSpacing) const = 0;
};

/**
 * SubUV primitive를 mesh/material로 변환하는 공통 계약.
 */
class ENGINE_API ISceneSubUVFeature
{
public:
	virtual ~ISceneSubUVFeature() = default;
	virtual FMaterial* GetBaseMaterial() const = 0;
	virtual bool BuildMesh(const FVector2& Size, FRenderMesh& OutMesh) const = 0;
};

