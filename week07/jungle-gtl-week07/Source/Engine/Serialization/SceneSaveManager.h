#pragma once

#include <string>
#include <filesystem>
#include <unordered_map>
#include "Core/CoreMinimal.h"
#include "Core/Paths.h"
#include "GameFramework/WorldContext.h"

// Forward declarations
class UObject;
class UWorld;
class AActor;
class UActorComponent;
class USceneComponent;

struct FPropertyDescriptor;

// Perspective 카메라 상태 — 씬 파일에 저장/복원되는 에디터 전용 데이터
struct FEditorCameraState
{
	FVector  Location = FVector::ZeroVector;
	FRotator Rotation = FRotator(0.f, 0.f, 0.f);  // Pitch, Yaw, Roll (degrees)
	float    FOV      = 60.0f;                     // degrees
	float    NearClip = 0.1f;
	float    FarClip  = 1000.0f;
	bool     bValid   = false;
};

class FSceneSaveManager {
public:
	static constexpr const wchar_t* SceneExtension = L".Scene";

	static std::wstring GetSceneDirectory() { return FPaths::SceneDir(); }

	static void Save(const FString& FilePath, FWorldContext& WorldContext,
					 const FEditorCameraState* CameraState = nullptr);
	static void Load(const FString& FilePath, FWorldContext& OutWorldContext,
					 FEditorCameraState* OutCameraState = nullptr);

private:
	static FString GetCurrentTimeStamp();
};
