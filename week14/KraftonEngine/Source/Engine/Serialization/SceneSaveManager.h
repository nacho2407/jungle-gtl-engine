#pragma once

#include <string>
#include <filesystem>
#include "Core/Types/CoreTypes.h"
#include "Platform/Paths.h"
#include "GameFramework/WorldContext.h"
#include "Math/Vector.h"
#include "Core/Types/PropertyTypes.h"
#include "Engine/Editor/SceneOutlinerState.h"

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
	float   FarClip  = 10000.0f;
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

	/**
	 * @brief 선택된 actor 목록을 prefab JSON 파일로 저장합니다
	 *
	 * @param FilePath prefab을 저장할 전체 파일 경로
	 *
	 * @param Actors prefab에 포함할 actor 목록
	 *
	 * @param OutlinerState actor 그룹 정보를 읽을 Scene Manager 상태
	 *
	 * @return 저장에 성공하면 true
	 *
	 * @details prefab은 actor 참조형 asset이 아니라 scene actor JSON을 복사해 저장하는 가벼운 템플릿입니다
	 */
	static bool SavePrefabAsJSON(const FString& FilePath, const TArray<AActor*>& Actors, const FSceneOutlinerState& OutlinerState);

	/**
	 * @brief prefab JSON 파일을 현재 world에 복사본 actor들로 배치합니다
	 *
	 * @param FilePath 로드할 prefab 전체 파일 경로
	 *
	 * @param World actor를 생성할 world
	 *
	 * @param PlacementLocation prefab 원점을 맞출 배치 위치
	 *
	 * @param OutCreatedActors 생성된 actor 목록
	 *
	 * @param OutlinerState 복원할 그룹을 추가할 Scene Manager 상태
	 *
	 * @return 하나 이상의 actor 생성에 성공하면 true
	 *
	 * @details prefab 내부 actor/component 참조는 같은 prefab에서 생성된 객체끼리만 복원됩니다
	 */
	static bool InstantiatePrefabFromJSON(
		const FString& FilePath,
		UWorld* World,
		const FVector& PlacementLocation,
		TArray<AActor*>& OutCreatedActors,
		FSceneOutlinerState& OutlinerState);

	/**
	 * @brief 에디터 undo용 actor 스냅샷 JSON 생성
	 *
	 * @param Actor 스냅샷을 생성할 actor
	 *
	 * @return actor와 하위 component 상태를 담은 JSON 객체
	 *
	 * @details 일반 scene save와 달리 undo/redo에서 같은 대상을 다시 찾을 수 있도록 runtime UUID를 함께 기록합니다.
	 */
	static json::JSON SerializeActorForEditorUndo(AActor* Actor);

	/**
	 * @brief 에디터 undo용 actor 스냅샷 JSON 복원
	 *
	 * @param World actor를 복원할 world
	 *
	 * @param ActorJSON SerializeActorForEditorUndo가 생성한 actor JSON 객체
	 *
	 * @return 복원된 actor. 실패하면 nullptr 반환
	 *
	 * @details Runtime UUID가 기록된 snapshot이면 actor와 component의 UUID를 기존 값으로 되살립니다.
	 */
	static AActor* DeserializeActorForEditorUndo(UWorld* World, json::JSON ActorJSON);

	/**
	 * @brief 에디터 undo용 actor 스냅샷을 기존 actor에 적용합니다.
	 *
	 * @param Actor 스냅샷을 적용할 기존 actor
	 *
	 * @param ActorJSON SerializeActorForEditorUndo가 생성한 actor JSON 객체
	 *
	 * @details Actor 객체 자체는 유지하고 하위 component 구조와 저장 대상 속성만 snapshot 상태로 재구성합니다.
	 */
	static void ApplyActorSnapshotForEditorUndo(AActor* Actor, json::JSON ActorJSON);

	/**
	 * @brief 에디터 undo용 객체 속성 스냅샷 JSON 생성
	 *
	 * @param Object 스냅샷을 생성할 객체
	 *
	 * @return 객체의 저장 대상 속성 상태를 담은 JSON 객체
	 *
	 * @details Details 패널 속성 undo/redo에서 같은 객체의 속성만 되돌릴 수 있도록 runtime UUID와 객체 참조 map을 함께 기록합니다.
	 */
	static json::JSON SerializeObjectPropertiesForEditorUndo(UObject* Object);

	/**
	 * @brief 에디터 undo용 객체 속성 스냅샷 JSON 복원
	 *
	 * @param Object 속성을 복원할 객체
	 *
	 * @param ObjectJSON SerializeObjectPropertiesForEditorUndo가 생성한 object property JSON 객체
	 *
	 * @details 스냅샷에 들어 있는 PF_Save 속성을 복원하고 각 속성의 PostEditChangeProperty를 호출합니다.
	 */
	static void DeserializeObjectPropertiesForEditorUndo(UObject* Object, json::JSON ObjectJSON);

	struct FSceneSaveContext
	{
		TMap<const UObject*, uint32> ObjectToId;
		uint32 NextObjectId = 1;

		uint32 RegisterSceneObject(const UObject* Object);
		uint32 FindObjectId(const UObject* Object) const;
	};

	struct FPendingPropertyLoad
	{
		UObject* Object = nullptr;
		json::JSON* Properties = nullptr;
	};

	struct FSceneLoadContext
	{
		TMap<uint32, UObject*> ObjectById;
		TArray<FPendingPropertyLoad> PendingProperties;

		void RegisterLoadedObject(json::JSON& Node, UObject* Object);
		UObject* FindObjectById(uint32 ObjectId) const;
		void QueueProperties(UObject* Object, json::JSON& Properties);
	};

private:
	// ---- Serialization ----
	static void CollectWorldObjectIds(UWorld* World, FSceneSaveContext& Context);
	static void CollectActorObjectIds(AActor* Actor, FSceneSaveContext& Context);
	static void CollectSceneComponentObjectIds(USceneComponent* Comp, FSceneSaveContext& Context);
	static json::JSON SerializeWorld(UWorld* World, const FWorldContext& Ctx, const FMinimalViewInfo* PerspectivePOV, FSceneSaveContext& Context);
	static json::JSON SerializeActor(AActor* Actor, FSceneSaveContext& Context);
	static json::JSON SerializeSceneComponentTree(USceneComponent* Comp, FSceneSaveContext& Context);
	static json::JSON SerializeProperties(UObject* Obj, FSceneSaveContext& Context);
	static json::JSON SerializeEditorOutliner(UWorld* World, FSceneSaveContext& Context);
	static json::JSON SerializePrefabOutliner(const TArray<AActor*>& Actors, const FSceneOutlinerState& OutlinerState, FSceneSaveContext& Context);

	// ---- Camera ----
	static json::JSON SerializeCamera(const FMinimalViewInfo* POV);
	static void DeserializeCamera(json::JSON& CamJSON, FPerspectiveCameraData& OutCam);

	// ---- Deserialization helpers ----
	static USceneComponent* DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner, FSceneLoadContext& Context);
	static void DeserializeProperties(UObject* Obj, json::JSON& PropsJSON, FSceneLoadContext& Context);
	static void DeserializeEditorOutliner(json::JSON& Node, UWorld* World, FSceneLoadContext& Context);
	static void DeserializePrefabOutliner(json::JSON& Node, FSceneLoadContext& Context, FSceneOutlinerState& OutlinerState);

	static string GetCurrentTimeStamp();
};
