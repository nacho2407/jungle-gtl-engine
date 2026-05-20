#pragma once

#include <string>
#include <filesystem>
#include "Core/CoreTypes.h"
#include "Platform/Paths.h"
#include "GameFramework/WorldContext.h"
#include "Math/Vector.h"
#include "Core/PropertyTypes.h"

// Forward declarations
class UObject;
class UWorld;
class AActor;
class UActorComponent;
class USceneComponent;
struct FMinimalViewInfo;

namespace json
{
	class JSON;
}


using std::string;

// Perspective 뷰포트 카메라의 씬 스냅샷 — 씬 저장/로드 시 주고받는 순수 데이터
struct FPerspectiveCameraData
{
	FVector Location = FVector(0, 0, 0);
	FVector Rotation = FVector(0, 0, 0); // Euler (Roll, Pitch, Yaw) in degrees
	float   FOV      = 3.14159265f / 3.0f;
	float   NearClip = 0.1f;
	float   FarClip  = 1000.0f;
	bool    bValid   = false;
};

class FSceneSaveManager
{
public:
	static constexpr const wchar_t* SceneExtension = L".Scene";

	static std::wstring GetSceneDirectory() { return FPaths::SceneDir(); }

	static void SaveSceneAsJSON(const string& SceneName, FWorldContext& WorldContext, const struct FMinimalViewInfo* PerspectivePOV = nullptr);
	// OverrideWorldType: 호출자가 World 의 WorldType 을 명시 — Game 빌드처럼 scene 파일에
	// 기록된 EWorldType (보통 Editor) 을 무시하고 강제로 다른 타입으로 시작하고 싶을 때 사용.
	// nullptr 이면 scene 파일의 값을 따른다 (없으면 Editor). UWorld 의 default WorldType
	// 이 Editor 라 actor deserialize 시점에 EditorOnly 컴포넌트의 SceneProxy 가 만들어지는
	// 사고를 막기 위해, 이 값은 Actor 생성 전에 World 에 적용된다.
	static void LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext, FPerspectiveCameraData& OutCam, const EWorldType* OverrideWorldType = nullptr);

	static TArray<FString> GetSceneFileList();

private:
	struct FSceneSaveContext : FJsonObjectReferenceContext
	{
		TMap<const UObject*, uint32> ObjectToId;
		uint32 NextObjectId = 1;

		uint32 RegisterSceneObject(const UObject* Object);
		uint32 FindObjectId(const UObject* Object) const;
		bool SerializeObjectReference(const UObject* Object, json::JSON& OutValue) const override;
	};

	struct FPendingPropertyLoad
	{
		UObject* Object = nullptr;
		json::JSON* Properties = nullptr;
	};

	struct FSceneLoadContext : FJsonObjectReferenceContext
	{
		TMap<uint32, UObject*> ObjectById;
		TArray<FPendingPropertyLoad> PendingProperties;

		void RegisterLoadedObject(json::JSON& Node, UObject* Object);
		UObject* FindObjectById(uint32 ObjectId) const;
		void QueueProperties(UObject* Object, json::JSON& Properties);
		bool DeserializeObjectReference(json::JSON& Value, UObject*& OutObject) const override;
	};

	// ---- Serialization ----
	static void CollectWorldObjectIds(UWorld* World, FSceneSaveContext& Context);
	static void CollectActorObjectIds(AActor* Actor, FSceneSaveContext& Context);
	static void CollectSceneComponentObjectIds(USceneComponent* Comp, FSceneSaveContext& Context);
	static json::JSON SerializeWorld(UWorld* World, const FWorldContext& Ctx, const FMinimalViewInfo* PerspectivePOV, FSceneSaveContext& Context);
	static json::JSON SerializeActor(AActor* Actor, FSceneSaveContext& Context);
	static json::JSON SerializeSceneComponentTree(USceneComponent* Comp, FSceneSaveContext& Context);
	static json::JSON SerializeProperties(UObject* Obj, FSceneSaveContext& Context);

	// ---- Camera ----
	static json::JSON SerializeCamera(const FMinimalViewInfo* POV);
	static void DeserializeCamera(json::JSON& CamJSON, FPerspectiveCameraData& OutCam);

	// ---- Deserialization helpers ----
	static USceneComponent* DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner, FSceneLoadContext& Context);
	static void DeserializeProperties(UObject* Obj, json::JSON& PropsJSON, FSceneLoadContext& Context);

	static string GetCurrentTimeStamp();
};
