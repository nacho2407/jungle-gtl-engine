#pragma once

#include <string>
#include <filesystem>
#include "Core/CoreMinimal.h"
#include "Core/Paths.h"
#include "GameFramework/WorldContext.h"

// Forward declarations
class UObject;
class UWorld;
class AActor;
class UActorComponent;
class USceneComponent;

namespace json {
	class JSON;
}

struct FPropertyDescriptor;

using std::string;

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

	// CameraState 는 nullable — nullptr 이면 카메라 섹션을 무시합니다 (게임/PIE 호환)
	static void SaveSceneAsJSON(const string& SceneName, FWorldContext& WorldContext,
	                            const FEditorCameraState* CameraState = nullptr);
	static void LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext,
	                              FEditorCameraState* OutCameraState = nullptr);

	static TArray<FString> GetSceneFileList();

private:
	// ---- Serialization ----
	static json::JSON SerializeWorld(UWorld* World, const FWorldContext& Ctx);
	static json::JSON SerializeActor(AActor* Actor);
	static json::JSON SerializeSceneComponentTree(USceneComponent* Comp);
	static json::JSON SerializeProperties(UActorComponent* Comp);
	static json::JSON SerializePropertyValue(const FPropertyDescriptor& Prop);
	static json::JSON SerializeCameraState(const FEditorCameraState* CameraState = nullptr);

	// ---- Deserialization ----
	static USceneComponent* DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner);
	static void DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON);
	static void DeserializePropertyValue(FPropertyDescriptor& Prop, json::JSON& Value);
	static void DeserializeCameraState(json::JSON& root, FEditorCameraState* OutCameraState = nullptr);

	static string GetCurrentTimeStamp();
};
