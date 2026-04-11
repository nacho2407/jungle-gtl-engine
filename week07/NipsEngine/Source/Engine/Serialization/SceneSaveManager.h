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

	static json::JSON SerializeWorldToPrimitives(UWorld* World, const FWorldContext& Ctx);
	// SceneComponent 트리를 OutPrimitives에 재귀적으로 평탄화 직렬화
	static void CollectComponentsFlat(USceneComponent* Comp, uint32 ParentID, json::JSON& OutPrimitives);
	// Actor가 소유한 비씬 ActorComponent(MovementComponent 등)를 직렬화
	static void CollectNonSceneComponents(AActor* Actor, json::JSON& OutPrimitives);
	static json::JSON SerializeWorld(UWorld* World, const FWorldContext& Ctx);
	static json::JSON SerializeActor(AActor* Actor);
	static json::JSON SerializeSceneComponentTree(USceneComponent* Comp);
	static json::JSON SerializeProperties(UActorComponent* Comp);
	static json::JSON SerializePropertyValue(const FPropertyDescriptor& Prop);
	static json::JSON SerializeCameraState(const FEditorCameraState* CameraState = nullptr);

	// ---- Deserialization ----
	static void DeserializePrimitivesToWorld(json::JSON& PrimitivesNode, UWorld* World);
	static USceneComponent* DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner);
	// UUIDToSceneComp: SceneComponentRef 타입 역직렬화에 사용. nullptr이면 SceneComponentRef 무시.
	static void DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON,
	                                  const std::unordered_map<uint32, USceneComponent*>* UUIDToSceneComp = nullptr);
	static void DeserializePropertyValue(FPropertyDescriptor& Prop, json::JSON& Value,
	                                     const std::unordered_map<uint32, USceneComponent*>* UUIDToSceneComp = nullptr);
	static void DeserializeCameraState(json::JSON& root, FEditorCameraState* OutCameraState = nullptr);

	static string GetCurrentTimeStamp();
};
