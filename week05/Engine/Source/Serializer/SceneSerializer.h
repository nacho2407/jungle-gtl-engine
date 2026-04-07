#pragma once
#include "CoreMinimal.h"
#include "Types/String.h"
#include "EngineAPI.h"
#include <d3d11.h>

class ULevel;

struct ENGINE_API FCameraSerializeData
{
	FVector  Location  = FVector(0.f, 0.f, -5.f);
	FRotator Rotation  = FRotator::ZeroRotator;
	float    FOV       = 60.f;
	float    NearClip  = 0.1f;
	float    FarClip   = 1000.f;
	bool     bValid    = false;
};

class ENGINE_API FSceneSerializer
{
public:
	static void Save(ULevel* Scene, const FString& FilePath,
	                 const FCameraSerializeData& CameraData = {});
	static bool Load(ULevel* Scene, const FString& FilePath, ID3D11Device* Device,
	                 FCameraSerializeData* OutCameraData = nullptr);
};
