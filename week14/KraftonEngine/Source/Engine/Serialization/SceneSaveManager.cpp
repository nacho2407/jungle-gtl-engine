#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include "SimpleJSON/json.hpp"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/SceneComponent.h"
#include "Component/ActorComponent.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Component/Primitive/DecalComponent.h"
#include "Component/Primitive/HeightFogComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/Camera/CameraComponent.h"
#include "Object/Object.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Core/Types/PropertyTypes.h"
#include "Object/FName.h"
#include "Serialization/JsonArchive.h"
#include "Profiling/Time/PlatformTime.h"
#include "Core/Logging/Log.h"

#include <algorithm>

// ---- JSON vector helpers ---------------------------------------------------

static void WriteVec3(json::JSON& Obj, const char* Key, const FVector& V)
{
	json::JSON arr = json::Array();
	arr.append(static_cast<double>(V.X));
	arr.append(static_cast<double>(V.Y));
	arr.append(static_cast<double>(V.Z));
	Obj[Key] = arr;
}

static FVector ReadVec3(json::JSON& Arr)
{
	FVector out(0, 0, 0);
	int i = 0;
	for (auto& e : Arr.ArrayRange()) {
		if (i == 0) out.X = static_cast<float>(e.ToFloat());
		else if (i == 1) out.Y = static_cast<float>(e.ToFloat());
		else if (i == 2) out.Z = static_cast<float>(e.ToFloat());
		++i;
	}
	return out;
}

// ---------------------------------------------------------------------------

namespace SceneKeys
{
	static constexpr const char* Version = "Version";
	static constexpr const char* Name = "Name";
	static constexpr const char* ClassName = "ClassName";
	static constexpr const char* WorldType = "WorldType";
	static constexpr const char* ContextName = "ContextName";
	static constexpr const char* ContextHandle = "ContextHandle";
	static constexpr const char* WorldSettings = "WorldSettings";
	static constexpr const char* GameMode = "GameMode";  // legacy / WorldSettings 내부 키
	static constexpr const char* Actors = "Actors";
	static constexpr const char* RootComponent = "RootComponent";
	static constexpr const char* NonSceneComponents = "NonSceneComponents";
	static constexpr const char* Properties = "Properties";
	static constexpr const char* Children = "Children";
	static constexpr const char* HiddenInComponentTree = "bHiddenInComponentTree";
	static constexpr const char* ObjectId = "ObjectId";
	static constexpr const char* RuntimeUUID = "RuntimeUUID";
	static constexpr const char* EditorUndoObjectReferences = "EditorUndoObjectReferences";
	static constexpr const char* EditorOutliner = "EditorOutliner";
	static constexpr const char* Groups = "Groups";
	static constexpr const char* GroupId = "GroupId";
	static constexpr const char* ParentGroupId = "ParentGroupId";
	static constexpr const char* ActorObjectIds = "ActorObjectIds";
	static constexpr const char* Expanded = "Expanded";
	static constexpr const char* Origin = "Origin";
}

class FSceneJsonSaveArchive : public FJsonArchive
{
public:
	FSceneJsonSaveArchive(json::JSON& Root, const FSceneSaveManager::FSceneSaveContext& InContext)
		: FJsonArchive(Root, /*bInIsSaving=*/true)
		, Context(InContext)
	{
	}

protected:
	uint32 ResolveJsonObjectId(const UObject* Object) const override
	{
		return Context.FindObjectId(Object);
	}

private:
	const FSceneSaveManager::FSceneSaveContext& Context;
};

class FSceneJsonLoadArchive : public FJsonArchive
{
public:
	FSceneJsonLoadArchive(json::JSON& Root, const FSceneSaveManager::FSceneLoadContext& InContext)
		: FJsonArchive(Root, /*bInIsSaving=*/false)
		, Context(InContext)
	{
	}

protected:
	UObject* ResolveJsonObjectReference(uint32 ObjectId) const override
	{
		return ObjectId != 0 ? Context.FindObjectById(ObjectId) : nullptr;
	}

private:
	const FSceneSaveManager::FSceneLoadContext& Context;
};

uint32 FSceneSaveManager::FSceneSaveContext::RegisterSceneObject(const UObject* Object)
{
	if (!Object)
	{
		return 0;
	}

	auto It = ObjectToId.find(Object);
	if (It != ObjectToId.end())
	{
		return It->second;
	}

	const uint32 ObjectId = NextObjectId++;
	ObjectToId.emplace(Object, ObjectId);
	return ObjectId;
}

uint32 FSceneSaveManager::FSceneSaveContext::FindObjectId(const UObject* Object) const
{
	if (!Object)
	{
		return 0;
	}

	auto It = ObjectToId.find(Object);
	return It != ObjectToId.end() ? It->second : 0;
}

void FSceneSaveManager::FSceneLoadContext::RegisterLoadedObject(json::JSON& Node, UObject* Object)
{
	if (!Object || !Node.hasKey(SceneKeys::ObjectId))
	{
		return;
	}

	const uint32 ObjectId = static_cast<uint32>(Node[SceneKeys::ObjectId].ToInt());
	if (ObjectId != 0)
	{
		ObjectById[ObjectId] = Object;
	}
}

UObject* FSceneSaveManager::FSceneLoadContext::FindObjectById(uint32 ObjectId) const
{
	auto It = ObjectById.find(ObjectId);
	return It != ObjectById.end() ? It->second : nullptr;
}

void FSceneSaveManager::FSceneLoadContext::QueueProperties(UObject* Object, json::JSON& Properties)
{
	if (!Object)
	{
		return;
	}

	PendingProperties.push_back({ Object, &Properties });
}

static void SerializeComponentEditorMetadata(json::JSON& Node, const UActorComponent* Comp)
{
	if (!Comp)
	{
		return;
	}

	if (Comp->IsHiddenInComponentTree())
	{
		Node[SceneKeys::HiddenInComponentTree] = true;
	}
}

static void DeserializeComponentEditorMetadata(UActorComponent* Comp, json::JSON& Node)
{
	if (!Comp)
	{
		return;
	}

	if (Node.hasKey(SceneKeys::HiddenInComponentTree))
	{
		Comp->SetHiddenInComponentTree(Node[SceneKeys::HiddenInComponentTree].ToBool());
	}
}

static void EnsureEditorBillboardMetadata(UActorComponent* Comp)
{
	if (ULightComponentBase* LightComponent = Cast<ULightComponentBase>(Comp))
	{
		LightComponent->EnsureEditorBillboard();
	}
	else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(Comp))
	{
		DecalComponent->EnsureEditorBillboard();
	}
	else if (UHeightFogComponent* HeightFogComponent = Cast<UHeightFogComponent>(Comp))
	{
		HeightFogComponent->EnsureEditorBillboard();
	}
	else if (UCameraComponent* CameraComponent = Cast<UCameraComponent>(Comp))
	{
		CameraComponent->EnsureEditorBillboard();
	}
}

/**
 * @brief 에디터 undo actor snapshot 적용 전에 기존 component 구조를 비웁니다.
 */
static void ClearActorComponentsForEditorUndo(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// AActor 소멸자와 같은 방식으로 component 목록이 비워질 때까지 뒤에서 제거합니다.
	// SceneComponent 제거는 자식 component까지 재귀적으로 OwnedComponents에서 제거하므로
	// 매 반복마다 최신 목록을 다시 조회해야 iterator invalidation을 피할 수 있습니다.
	while (!Actor->GetComponents().empty())
	{
		UActorComponent* Component = Actor->GetComponents().back();
		Actor->RemoveComponent(Component);
	}
}

/**
 * @brief 에디터 undo snapshot에 runtime UUID를 기록합니다.
 */
static void WriteEditorUndoRuntimeUUID(json::JSON& Node, const UObject* Object)
{
	if (!Object)
	{
		return;
	}

	Node[SceneKeys::RuntimeUUID] = static_cast<int>(Object->GetUUID());
}

/**
 * @brief 에디터 undo snapshot의 runtime UUID를 객체에 복원합니다.
 */
static void RestoreEditorUndoRuntimeUUID(json::JSON& Node, UObject* Object)
{
	if (!Object || !Node.hasKey(SceneKeys::RuntimeUUID))
	{
		return;
	}

	const uint32 RuntimeUUID = static_cast<uint32>(Node[SceneKeys::RuntimeUUID].ToInt());
	if (RuntimeUUID != 0)
	{
		Object->SetUUID(RuntimeUUID);
	}
}

/**
 * @brief scene component subtree JSON에 runtime UUID를 재귀적으로 기록합니다.
 */
static void AnnotateSceneComponentRuntimeUUIDs(USceneComponent* Comp, json::JSON& Node)
{
	if (!Comp)
	{
		return;
	}

	WriteEditorUndoRuntimeUUID(Node, Comp);
	if (!Node.hasKey(SceneKeys::Children))
	{
		return;
	}

	const TArray<USceneComponent*>& Children = Comp->GetChildren();
	int32 ChildIndex = 0;
	for (auto& ChildJSON : Node[SceneKeys::Children].ArrayRange())
	{
		if (ChildIndex < static_cast<int32>(Children.size()))
		{
			AnnotateSceneComponentRuntimeUUIDs(Children[ChildIndex], ChildJSON);
		}
		++ChildIndex;
	}
}

/**
 * @brief actor JSON에 actor/component runtime UUID를 기록합니다.
 */
static void AnnotateActorRuntimeUUIDs(AActor* Actor, json::JSON& ActorJSON)
{
	if (!Actor)
	{
		return;
	}

	WriteEditorUndoRuntimeUUID(ActorJSON, Actor);
	if (Actor->GetRootComponent() && ActorJSON.hasKey(SceneKeys::RootComponent))
	{
		AnnotateSceneComponentRuntimeUUIDs(Actor->GetRootComponent(), ActorJSON[SceneKeys::RootComponent]);
	}

	if (!ActorJSON.hasKey(SceneKeys::NonSceneComponents))
	{
		return;
	}

	int32 NonSceneIndex = 0;
	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!Comp || Comp->IsA<USceneComponent>())
		{
			continue;
		}

		if (NonSceneIndex < static_cast<int32>(ActorJSON[SceneKeys::NonSceneComponents].size()))
		{
			WriteEditorUndoRuntimeUUID(ActorJSON[SceneKeys::NonSceneComponents][static_cast<unsigned>(NonSceneIndex)], Comp);
		}
		++NonSceneIndex;
	}
}

/**
 * @brief undo 스냅샷 내 객체 참조 복원용 ObjectId/UUID 매핑 생성
 */
static json::JSON SerializeEditorUndoObjectReferenceMap(const FSceneSaveManager::FSceneSaveContext& Context)
{
	json::JSON References = json::Array();
	for (const auto& Entry : Context.ObjectToId)
	{
		const UObject* Object = Entry.first;
		const uint32 ObjectId = Entry.second;
		if (!Object || ObjectId == 0)
		{
			continue;
		}

		json::JSON Reference = json::Object();
		Reference[SceneKeys::ObjectId] = static_cast<int>(ObjectId);
		Reference[SceneKeys::RuntimeUUID] = static_cast<int>(Object->GetUUID());
		References.append(Reference);
	}
	return References;
}

/**
 * @brief undo snapshot의 ObjectId/UUID 매핑을 load context에 등록합니다.
 */
static void RegisterEditorUndoObjectReferenceMap(
	json::JSON& ActorJSON,
	FSceneSaveManager::FSceneLoadContext& Context)
{
	if (!ActorJSON.hasKey(SceneKeys::EditorUndoObjectReferences))
	{
		return;
	}

	for (auto& ReferenceJSON : ActorJSON[SceneKeys::EditorUndoObjectReferences].ArrayRange())
	{
		if (!ReferenceJSON.hasKey(SceneKeys::ObjectId) || !ReferenceJSON.hasKey(SceneKeys::RuntimeUUID))
		{
			continue;
		}

		const uint32 ObjectId = static_cast<uint32>(ReferenceJSON[SceneKeys::ObjectId].ToInt());
		const uint32 RuntimeUUID = static_cast<uint32>(ReferenceJSON[SceneKeys::RuntimeUUID].ToInt());
		UObject* Object = RuntimeUUID != 0 ? UObjectManager::Get().FindByUUID(RuntimeUUID) : nullptr;
		if (ObjectId != 0 && Object)
		{
			Context.ObjectById[ObjectId] = Object;
		}
	}
}

/**
 * @brief undo용 객체가 속한 world 검색
 */
static UWorld* GetEditorUndoObjectWorld(UObject* Object)
{
	if (!Object)
	{
		return nullptr;
	}

	if (AActor* Actor = Cast<AActor>(Object))
	{
		return Actor->GetWorld();
	}

	if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		if (AActor* Owner = Component->GetOwner())
		{
			return Owner->GetWorld();
		}
	}

	return Object->GetTypedOuter<UWorld>();
}

static const char* WorldTypeToString(EWorldType Type)
{
	switch (Type) {
	case EWorldType::Game: return "Game";
	case EWorldType::PIE:  return "PIE";
	default:               return "Editor";
	}
}

static EWorldType StringToWorldType(const string& Str)
{
	if (Str == "Game") return EWorldType::Game;
	if (Str == "PIE")  return EWorldType::PIE;
	return EWorldType::Editor;
}

// ============================================================
// Save
// ============================================================

void FSceneSaveManager::SaveSceneAsJSON(const string& InSceneName, FWorldContext& WorldContext, const FMinimalViewInfo* PerspectivePOV)
{
	using namespace json;

	if (!WorldContext.World) return;

	string FinalName = InSceneName.empty()
		? "Save_" + GetCurrentTimeStamp()
		: InSceneName;

	std::wstring SceneDir = GetSceneDirectory();
	std::filesystem::path FileDestination = std::filesystem::path(SceneDir) / (FPaths::ToWide(FinalName) + SceneExtension);
	std::filesystem::create_directories(SceneDir);

	FSceneSaveContext SaveContext;
	CollectWorldObjectIds(WorldContext.World, SaveContext);

	JSON Root = SerializeWorld(WorldContext.World, WorldContext, PerspectivePOV, SaveContext);
	Root[SceneKeys::Version] = 2;
	Root[SceneKeys::Name] = FinalName;

	std::ofstream File(FileDestination);
	if (File.is_open()) {
		File << Root.dump();
		File.flush();
		File.close();
	}
}

bool FSceneSaveManager::SavePrefabAsJSON(
	const FString& FilePath,
	const TArray<AActor*>& Actors,
	const FSceneOutlinerState& OutlinerState)
{
	using namespace json;
	if (FilePath.empty() || Actors.empty())
	{
		return false;
	}

	TArray<AActor*> ValidActors;
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			ValidActors.push_back(Actor);
		}
	}

	if (ValidActors.empty())
	{
		return false;
	}

	FVector MinLocation = ValidActors.front()->GetActorLocation();
	FVector MaxLocation = MinLocation;
	for (AActor* Actor : ValidActors)
	{
		const FVector Location = Actor->GetActorLocation();
		MinLocation.X = (std::min)(MinLocation.X, Location.X);
		MinLocation.Y = (std::min)(MinLocation.Y, Location.Y);
		MinLocation.Z = (std::min)(MinLocation.Z, Location.Z);
		MaxLocation.X = (std::max)(MaxLocation.X, Location.X);
		MaxLocation.Y = (std::max)(MaxLocation.Y, Location.Y);
		MaxLocation.Z = (std::max)(MaxLocation.Z, Location.Z);
	}
	const FVector Origin = (MinLocation + MaxLocation) * 0.5f;

	FSceneSaveContext SaveContext;
	for (AActor* Actor : ValidActors)
	{
		CollectActorObjectIds(Actor, SaveContext);
	}

	JSON Root = json::Object();
	Root[SceneKeys::Version] = 1;
	Root[SceneKeys::ClassName] = "Prefab";
	WriteVec3(Root, SceneKeys::Origin, Origin);

	JSON ActorArray = json::Array();
	for (AActor* Actor : ValidActors)
	{
		ActorArray.append(SerializeActor(Actor, SaveContext));
	}
	Root[SceneKeys::Actors] = ActorArray;

	JSON OutlinerJSON = SerializePrefabOutliner(ValidActors, OutlinerState, SaveContext);
	if (OutlinerJSON.size() > 0)
	{
		Root[SceneKeys::EditorOutliner] = OutlinerJSON;
	}

	const std::filesystem::path Destination(FPaths::ToWide(FilePath));
	std::filesystem::create_directories(Destination.parent_path());

	std::ofstream File(Destination);
	if (!File.is_open())
	{
		return false;
	}

	File << Root.dump();
	File.flush();
	File.close();
	return true;
}

bool FSceneSaveManager::InstantiatePrefabFromJSON(
	const FString& FilePath,
	UWorld* World,
	const FVector& PlacementLocation,
	TArray<AActor*>& OutCreatedActors,
	FSceneOutlinerState& OutlinerState)
{
	using json::JSON;
	OutCreatedActors.clear();
	if (FilePath.empty() || !World)
	{
		return false;
	}

	std::ifstream File(std::filesystem::path(FPaths::ToWide(FilePath)));
	if (!File.is_open())
	{
		return false;
	}

	string FileContent((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	JSON Root = JSON::Load(FileContent);
	if (!Root.hasKey(SceneKeys::Actors))
	{
		return false;
	}

	FVector Origin = FVector::ZeroVector;
	if (Root.hasKey(SceneKeys::Origin))
	{
		Origin = ReadVec3(Root[SceneKeys::Origin]);
	}
	const FVector PlacementOffset = PlacementLocation - Origin;

	FSceneLoadContext LoadContextState;
	World->BeginDeferredPickingBVHUpdate();

	for (auto& ActorJSON : Root[SceneKeys::Actors].ArrayRange())
	{
		string ActorClass = ActorJSON[SceneKeys::ClassName].ToString();
		if (ActorClass.empty())
		{
			continue;
		}

		UObject* ActorObj = FObjectFactory::Get().Create(ActorClass, World);
		AActor* Actor = Cast<AActor>(ActorObj);
		if (!Actor)
		{
			if (ActorObj)
			{
				UObjectManager::Get().DestroyObject(ActorObj);
			}
			continue;
		}

		LoadContextState.RegisterLoadedObject(ActorJSON, Actor);
		OutCreatedActors.push_back(Actor);

		if (ActorJSON.hasKey(SceneKeys::Name))
		{
			Actor->SetFName(FName(ActorJSON[SceneKeys::Name].ToString()));
		}

		if (ActorJSON.hasKey(SceneKeys::RootComponent))
		{
			JSON& RootJSON = ActorJSON[SceneKeys::RootComponent];
			USceneComponent* RootComponent = DeserializeSceneComponentTree(RootJSON, Actor, LoadContextState);
			if (RootComponent)
			{
				Actor->SetRootComponent(RootComponent);
			}
		}

		if (ActorJSON.hasKey(SceneKeys::Properties))
		{
			LoadContextState.QueueProperties(Actor, ActorJSON[SceneKeys::Properties]);
		}

		if (ActorJSON.hasKey(SceneKeys::NonSceneComponents))
		{
			for (auto& CompJSON : ActorJSON[SceneKeys::NonSceneComponents].ArrayRange())
			{
				string CompClass = CompJSON[SceneKeys::ClassName].ToString();
				UObject* CompObj = FObjectFactory::Get().Create(CompClass, Actor);
				UActorComponent* Comp = Cast<UActorComponent>(CompObj);
				if (!Comp)
				{
					if (CompObj)
					{
						UObjectManager::Get().DestroyObject(CompObj);
					}
					continue;
				}

				LoadContextState.RegisterLoadedObject(CompJSON, Comp);
				if (CompJSON.hasKey(SceneKeys::Name))
				{
					Comp->SetFName(FName(CompJSON[SceneKeys::Name].ToString()));
				}
				Actor->RegisterComponent(Comp);

				if (CompJSON.hasKey(SceneKeys::Properties))
				{
					LoadContextState.QueueProperties(Comp, CompJSON[SceneKeys::Properties]);
				}
				DeserializeComponentEditorMetadata(Comp, CompJSON);
			}
		}
	}

	for (FPendingPropertyLoad& Pending : LoadContextState.PendingProperties)
	{
		if (Pending.Object && Pending.Properties)
		{
			DeserializeProperties(Pending.Object, *Pending.Properties, LoadContextState);
		}
	}

	for (AActor* Actor : OutCreatedActors)
	{
		if (!Actor)
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component)
			{
				Component->PostDuplicate();
			}
		}

		Actor->PostDuplicate();
		Actor->AddActorWorldOffset(PlacementOffset);
		World->AddActor(Actor, false);
	}

	World->EndDeferredPickingBVHUpdate();
	World->BuildWorldPrimitivePickingBVHNow();

	if (Root.hasKey(SceneKeys::EditorOutliner))
	{
		DeserializePrefabOutliner(Root[SceneKeys::EditorOutliner], LoadContextState, OutlinerState);
	}

	if (World->HasBegunPlay())
	{
		for (AActor* Actor : OutCreatedActors)
		{
			if (Actor && !Actor->HasActorBegunPlay())
			{
				Actor->BeginPlay();
			}
		}
	}

	return !OutCreatedActors.empty();
}

void FSceneSaveManager::CollectWorldObjectIds(UWorld* World, FSceneSaveContext& Context)
{
	if (!World)
	{
		return;
	}

	Context.RegisterSceneObject(World);
	for (AActor* Actor : World->GetActors())
	{
		CollectActorObjectIds(Actor, Context);
	}
}

void FSceneSaveManager::CollectActorObjectIds(AActor* Actor, FSceneSaveContext& Context)
{
	if (!Actor)
	{
		return;
	}

	Context.RegisterSceneObject(Actor);
	if (Actor->GetRootComponent())
	{
		CollectSceneComponentObjectIds(Actor->GetRootComponent(), Context);
	}

	for (UActorComponent* Comp : Actor->GetComponents())
	{
		if (!Comp)
		{
			continue;
		}

		Context.RegisterSceneObject(Comp);
	}
}

void FSceneSaveManager::CollectSceneComponentObjectIds(USceneComponent* Comp, FSceneSaveContext& Context)
{
	if (!Comp)
	{
		return;
	}

	Context.RegisterSceneObject(Comp);
	for (USceneComponent* Child : Comp->GetChildren())
	{
		CollectSceneComponentObjectIds(Child, Context);
	}
}

json::JSON FSceneSaveManager::SerializeWorld(UWorld* World, const FWorldContext& Ctx, const FMinimalViewInfo* PerspectivePOV, FSceneSaveContext& Context)
{
	using namespace json;
	JSON w = json::Object();
	w[SceneKeys::ClassName] = World->GetClass()->GetName();
	w[SceneKeys::ObjectId] = static_cast<int>(Context.RegisterSceneObject(World));
	w[SceneKeys::WorldType] = WorldTypeToString(Ctx.WorldType);
	w[SceneKeys::ContextName] = Ctx.ContextName;
	w[SceneKeys::ContextHandle] = Ctx.ContextHandle.ToString();

	// ---- Actors ----
	JSON Actors = json::Array();
	for (AActor* Actor : World->GetActors()) {
		if (!Actor) continue;
		Actors.append(SerializeActor(Actor, Context));
	}
	w[SceneKeys::Actors] = Actors;

	// ---- Editor-only outliner groups ----
	JSON EditorOutliner = SerializeEditorOutliner(World, Context);
	if (EditorOutliner.size() > 0)
	{
		w[SceneKeys::EditorOutliner] = EditorOutliner;
	}

	// ---- Perspective camera ----
	JSON cam = SerializeCamera(PerspectivePOV);
	if (cam.size() > 0) {
		w["PerspectiveCamera"] = cam;
	}

	return w;
}

json::JSON FSceneSaveManager::SerializeActor(AActor* Actor, FSceneSaveContext& Context)
{
	using namespace json;
	JSON a = json::Object();
	a[SceneKeys::ClassName] = Actor->GetClass()->GetName();
	a[SceneKeys::ObjectId] = static_cast<int>(Context.RegisterSceneObject(Actor));
	a[SceneKeys::Name] = Actor->GetFName().ToString();
	a[SceneKeys::Properties] = SerializeProperties(Actor, Context);

	// RootComponent 트리 직렬화
	if (Actor->GetRootComponent()) {
		a[SceneKeys::RootComponent] = SerializeSceneComponentTree(Actor->GetRootComponent(), Context);
	}

	// Non-scene components
	JSON NonScene = json::Array();
	for (UActorComponent* Comp : Actor->GetComponents()) {
		if (!Comp) continue;
		if (Comp->IsA<USceneComponent>()) continue;

		JSON c = json::Object();
		c[SceneKeys::ClassName] = Comp->GetClass()->GetName();
		c[SceneKeys::ObjectId] = static_cast<int>(Context.RegisterSceneObject(Comp));
		c[SceneKeys::Name] = Comp->GetFName().ToString();
		c[SceneKeys::Properties] = SerializeProperties(Comp, Context);
		SerializeComponentEditorMetadata(c, Comp);
		NonScene.append(c);
	}
	a[SceneKeys::NonSceneComponents] = NonScene;

	return a;
}

json::JSON FSceneSaveManager::SerializeSceneComponentTree(USceneComponent* Comp, FSceneSaveContext& Context)
{
	using namespace json;
	JSON c = json::Object();
	c[SceneKeys::ClassName] = Comp->GetClass()->GetName();
	c[SceneKeys::ObjectId] = static_cast<int>(Context.RegisterSceneObject(Comp));
	c[SceneKeys::Name] = Comp->GetFName().ToString();
	c[SceneKeys::Properties] = SerializeProperties(Comp, Context);
	SerializeComponentEditorMetadata(c, Comp);

	JSON Children = json::Array();
	for (USceneComponent* Child : Comp->GetChildren()) {
		if (!Child) continue;
		Children.append(SerializeSceneComponentTree(Child, Context));
	}
	c[SceneKeys::Children] = Children;

	return c;
}

json::JSON FSceneSaveManager::SerializeProperties(UObject* Obj, FSceneSaveContext& Context)
{
	using namespace json;
	JSON Props = json::Object();
	if (!Obj) return Props;

	FSceneJsonSaveArchive Ar(Props, Context);
	Obj->SerializeProperties(Ar, PF_Save);
	return Props;
}

json::JSON FSceneSaveManager::SerializeEditorOutliner(UWorld* World, FSceneSaveContext& Context)
{
	using namespace json;
	JSON Outliner = json::Object();
	if (!World)
	{
		return Outliner;
	}

	JSON Groups = json::Array();
	const FSceneOutlinerState& OutlinerState = World->GetEditorOutlinerState();
	for (const FSceneOutlinerGroup& Group : OutlinerState.Groups)
	{
		JSON ActorObjectIds = json::Array();
		for (uint32 ActorUUID : Group.ActorUUIDs)
		{
			AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(ActorUUID));
			if (!Actor || Actor->GetWorld() != World)
			{
				continue;
			}

			const uint32 ObjectId = Context.FindObjectId(Actor);
			if (ObjectId != 0)
			{
				ActorObjectIds.append(static_cast<int>(ObjectId));
			}
		}

		if (ActorObjectIds.size() == 0)
		{
			if (!OutlinerState.HasChildGroups(Group.GroupId))
			{
				continue;
			}
		}

		JSON GroupJSON = json::Object();
		GroupJSON[SceneKeys::GroupId] = static_cast<int>(Group.GroupId);
		if (OutlinerState.FindGroup(Group.ParentGroupId))
		{
			GroupJSON[SceneKeys::ParentGroupId] = static_cast<int>(Group.ParentGroupId);
		}
		GroupJSON[SceneKeys::Name] = Group.Name;
		GroupJSON[SceneKeys::Expanded] = Group.bExpanded;
		GroupJSON[SceneKeys::ActorObjectIds] = ActorObjectIds;
		Groups.append(GroupJSON);
	}

	if (Groups.size() > 0)
	{
		Outliner[SceneKeys::Groups] = Groups;
	}
	return Outliner;
}

json::JSON FSceneSaveManager::SerializePrefabOutliner(
	const TArray<AActor*>& Actors,
	const FSceneOutlinerState& OutlinerState,
	FSceneSaveContext& Context)
{
	using namespace json;
	JSON Outliner = json::Object();

	TArray<uint32> SelectedActorUUIDs;
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			SelectedActorUUIDs.push_back(Actor->GetUUID());
		}
	}

	JSON Groups = json::Array();
	TSet<uint32> IncludedGroupIds;
	for (const FSceneOutlinerGroup& SourceGroup : OutlinerState.Groups)
	{
		for (uint32 ActorUUID : SourceGroup.ActorUUIDs)
		{
			if (std::find(SelectedActorUUIDs.begin(), SelectedActorUUIDs.end(), ActorUUID) == SelectedActorUUIDs.end())
			{
				continue;
			}

			const FSceneOutlinerGroup* CurrentGroup = &SourceGroup;
			while (CurrentGroup)
			{
				IncludedGroupIds.insert(CurrentGroup->GroupId);
				CurrentGroup = OutlinerState.FindGroup(CurrentGroup->ParentGroupId);
			}
			break;
		}
	}

	for (const FSceneOutlinerGroup& SourceGroup : OutlinerState.Groups)
	{
		JSON ActorObjectIds = json::Array();
		if (IncludedGroupIds.find(SourceGroup.GroupId) == IncludedGroupIds.end())
		{
			continue;
		}

		for (uint32 ActorUUID : SourceGroup.ActorUUIDs)
		{
			if (std::find(SelectedActorUUIDs.begin(), SelectedActorUUIDs.end(), ActorUUID) == SelectedActorUUIDs.end())
			{
				continue;
			}

			AActor* Actor = Cast<AActor>(UObjectManager::Get().FindByUUID(ActorUUID));
			const uint32 ObjectId = Context.FindObjectId(Actor);
			if (ObjectId != 0)
			{
				ActorObjectIds.append(static_cast<int>(ObjectId));
			}
		}

		if (ActorObjectIds.size() == 0)
		{
			TArray<uint32> ChildGroupIds;
			OutlinerState.CollectChildGroupIds(SourceGroup.GroupId, ChildGroupIds);
			bool bHasIncludedChildGroup = false;
			for (uint32 ChildGroupId : ChildGroupIds)
			{
				if (IncludedGroupIds.find(ChildGroupId) != IncludedGroupIds.end())
				{
					bHasIncludedChildGroup = true;
					break;
				}
			}
			if (!bHasIncludedChildGroup)
			{
				continue;
			}
		}

		JSON GroupJSON = json::Object();
		GroupJSON[SceneKeys::GroupId] = static_cast<int>(SourceGroup.GroupId);
		if (IncludedGroupIds.find(SourceGroup.ParentGroupId) != IncludedGroupIds.end())
		{
			GroupJSON[SceneKeys::ParentGroupId] = static_cast<int>(SourceGroup.ParentGroupId);
		}
		GroupJSON[SceneKeys::Name] = SourceGroup.Name;
		GroupJSON[SceneKeys::Expanded] = SourceGroup.bExpanded;
		GroupJSON[SceneKeys::ActorObjectIds] = ActorObjectIds;
		Groups.append(GroupJSON);
	}

	if (Groups.size() > 0)
	{
		Outliner[SceneKeys::Groups] = Groups;
	}
	return Outliner;
}

json::JSON FSceneSaveManager::SerializeActorForEditorUndo(AActor* Actor)
{
	if (!Actor)
	{
		return json::JSON();
	}

	FSceneSaveContext Context;
	if (UWorld* World = Actor->GetWorld())
	{
		// 외부 actor/component 참조도 ObjectId로 직렬화될 수 있도록 world 전체 object id를 준비합니다.
		CollectWorldObjectIds(World, Context);
	}
	else
	{
		CollectActorObjectIds(Actor, Context);
	}

	json::JSON ActorJSON = SerializeActor(Actor, Context);
	AnnotateActorRuntimeUUIDs(Actor, ActorJSON);
	ActorJSON[SceneKeys::EditorUndoObjectReferences] = SerializeEditorUndoObjectReferenceMap(Context);
	return ActorJSON;
}

json::JSON FSceneSaveManager::SerializeObjectPropertiesForEditorUndo(UObject* Object)
{
	if (!Object)
	{
		return json::JSON();
	}

	FSceneSaveContext Context;

	// Details 속성은 actor/component 참조를 자주 담으므로 객체가 속한 world 전체를
	// 같은 ObjectId context에 먼저 등록해 JSON archive의 객체 참조를 보존합니다.
	if (UWorld* World = GetEditorUndoObjectWorld(Object))
	{
		CollectWorldObjectIds(World, Context);
	}
	else if (AActor* Actor = Cast<AActor>(Object))
	{
		CollectActorObjectIds(Actor, Context);
	}
	else if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		if (AActor* Owner = Component->GetOwner())
		{
			CollectActorObjectIds(Owner, Context);
		}
	}

	json::JSON ObjectJSON = json::Object();
	ObjectJSON[SceneKeys::ClassName] = Object->GetClass()->GetName();
	ObjectJSON[SceneKeys::ObjectId] = static_cast<int>(Context.RegisterSceneObject(Object));
	WriteEditorUndoRuntimeUUID(ObjectJSON, Object);
	ObjectJSON[SceneKeys::Properties] = SerializeProperties(Object, Context);
	ObjectJSON[SceneKeys::EditorUndoObjectReferences] = SerializeEditorUndoObjectReferenceMap(Context);
	return ObjectJSON;
}

void FSceneSaveManager::DeserializeObjectPropertiesForEditorUndo(UObject* Object, json::JSON ObjectJSON)
{
	using json::JSON;
	if (!Object || ObjectJSON.JSONType() == JSON::Class::Null)
	{
		return;
	}

	if (!ObjectJSON.hasKey(SceneKeys::Properties))
	{
		return;
	}

	FSceneLoadContext LoadContextState;
	RegisterEditorUndoObjectReferenceMap(ObjectJSON, LoadContextState);
	LoadContextState.RegisterLoadedObject(ObjectJSON, Object);

	// Runtime UUID는 command의 대상 검색 기준이므로 보통 이미 같지만,
	// 스냅샷에 명시된 값이 있으면 동일성을 유지하도록 한 번 더 보정합니다.
	RestoreEditorUndoRuntimeUUID(ObjectJSON, Object);
	DeserializeProperties(Object, ObjectJSON[SceneKeys::Properties], LoadContextState);
}

// ---- Camera helpers ----

json::JSON FSceneSaveManager::SerializeCamera(const FMinimalViewInfo* POV)
{
	using namespace json;
	JSON cam = json::Object();
	if (!POV) return cam;

	WriteVec3(cam, "Location", POV->Location);
	// FRotator(Pitch, Yaw, Roll) → 직렬화 컨벤션 FVector(Roll, Pitch, Yaw)
	WriteVec3(cam, "Rotation", FVector(POV->Rotation.Roll, POV->Rotation.Pitch, POV->Rotation.Yaw));

	cam["FOV"] = static_cast<double>(POV->FOV);
	cam["NearClip"] = static_cast<double>(POV->NearClip);
	cam["FarClip"] = static_cast<double>(POV->FarClip);

	return cam;
}

void FSceneSaveManager::DeserializeCamera(json::JSON& CameraJSON, FPerspectiveCameraData& OutCam)
{
	using namespace json;
	if (CameraJSON.JSONType() == JSON::Class::Null) return;

	if (CameraJSON.hasKey("Location")) OutCam.Location = ReadVec3(CameraJSON["Location"]);
	if (CameraJSON.hasKey("Rotation")) OutCam.Rotation = ReadVec3(CameraJSON["Rotation"]);
	if (CameraJSON.hasKey("FOV")) {
		auto& Val = CameraJSON["FOV"];
		float fov = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
		// 엔진 내부는 라디안 — π(~3.14)를 넘으면 degree로 간주하고 변환
		if (fov > 3.14159265f) fov *= (3.14159265f / 180.0f);
		OutCam.FOV = fov;
	}
	if (CameraJSON.hasKey("NearClip")) {
		auto& Val = CameraJSON["NearClip"];
		OutCam.NearClip = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
	}
	if (CameraJSON.hasKey("FarClip")) {
		auto& Val = CameraJSON["FarClip"];
		OutCam.FarClip = static_cast<float>(Val.JSONType() == JSON::Class::Array ? Val[0].ToFloat() : Val.ToFloat());
	}
	OutCam.bValid = true;
}

// ============================================================
// Load
// ============================================================

void FSceneSaveManager::LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext, FPerspectiveCameraData& OutCam, const EWorldType* OverrideWorldType)
{
	using json::JSON;
	std::ifstream File(std::filesystem::path(FPaths::ToWide(filepath)));
	if (!File.is_open()) {
		std::cerr << "Failed to open file at target destination" << std::endl;
		return;
	}

	string FileContent((std::istreambuf_iterator<char>(File)),
		std::istreambuf_iterator<char>());

	JSON root = JSON::Load(FileContent);

	string ClassName = root[SceneKeys::ClassName].ToString();
	ClassName = ClassName.empty() ? "UWorld" : ClassName; // Default to "World" if ClassName is missing
	UObject* WorldObj = FObjectFactory::Get().Create(ClassName);
	if (!WorldObj || !WorldObj->IsA<UWorld>()) return;

	UWorld* World = static_cast<UWorld*>(WorldObj);
	FSceneLoadContext LoadContextState;
	LoadContextState.RegisterLoadedObject(root, World);

	EWorldType WorldType = OverrideWorldType
		? *OverrideWorldType
		: (root.hasKey(SceneKeys::WorldType)
			? StringToWorldType(root[SceneKeys::WorldType].ToString())
			: EWorldType::Editor);

	// World 의 WorldType 을 actor deserialize 전에 적용. Default 가 Editor 라 actor 추가
	// 시 CreateRenderState 의 "EditorOnly && WorldType != Editor" 체크가 잘못 통과돼 Game
	// 빌드에서도 editor billboard SceneProxy 가 만들어지는 버그를 막기 위해.
	World->SetWorldType(WorldType);
	FString ContextName = root.hasKey(SceneKeys::ContextName)
		? root[SceneKeys::ContextName].ToString()
		: "Loaded Scene";
	FString ContextHandle = root.hasKey(SceneKeys::ContextHandle)
		? root[SceneKeys::ContextHandle].ToString()
		: ContextName;

	// WorldSettings legacy — scene 단위 GameMode override 는 더 이상 사용하지 않는다.
	// 기존 씬 파일에 남은 값은 로그만 남기고 ProjectSettings.GameplayPreset 을 따른다.
	FWorldSettings WorldSettings;
	if (root.hasKey(SceneKeys::WorldSettings))
	{
		JSON& WSObj = root[SceneKeys::WorldSettings];
		if (WSObj.hasKey(SceneKeys::GameMode))
		{
			UE_LOG("[SceneLoad] Legacy WorldSettings.GameMode '%s' ignored. GameplayPreset is used instead.",
				WSObj[SceneKeys::GameMode].ToString().c_str());
		}
	}
	else if (root.hasKey(SceneKeys::GameMode))
	{
		UE_LOG("[SceneLoad] Legacy root GameMode '%s' ignored. GameplayPreset is used instead.",
			root[SceneKeys::GameMode].ToString().c_str());
	}
	World->GetWorldSettings() = WorldSettings;

	World->InitWorld();

	// "PerspectiveCamera" 우선, 구버전 "Camera" 키도 지원
	const char* CamKey = root.hasKey("PerspectiveCamera") ? "PerspectiveCamera"
		: root.hasKey("Camera") ? "Camera"
		: nullptr;
	if (CamKey) {
		JSON& Cam = root[CamKey];
		DeserializeCamera(Cam, OutCam);
	}

	// Deserialize Actors
	if (root.hasKey(SceneKeys::Actors))
	{
		for (auto& ActorJSON : root[SceneKeys::Actors].ArrayRange()) {
			string ActorClass = ActorJSON[SceneKeys::ClassName].ToString();

			UObject* ActorObj = FObjectFactory::Get().Create(ActorClass, World);
			if (!ActorObj || !ActorObj->IsA<AActor>()) continue;
			AActor* Actor = static_cast<AActor*>(ActorObj);
			LoadContextState.RegisterLoadedObject(ActorJSON, Actor);
			RestoreEditorUndoRuntimeUUID(ActorJSON, Actor);
			World->AddActor(Actor);

			if (ActorJSON.hasKey(SceneKeys::Name)) {
				Actor->SetFName(FName(ActorJSON[SceneKeys::Name].ToString()));
			}

			// RootComponent 트리 복원
			if (ActorJSON.hasKey(SceneKeys::RootComponent)) {
				JSON& RootJSON = ActorJSON[SceneKeys::RootComponent];
				USceneComponent* Root = DeserializeSceneComponentTree(RootJSON, Actor, LoadContextState);
				if (Root) Actor->SetRootComponent(Root);
			}

			// Actor 프로퍼티(Location/Rotation/Scale/Visible 및 서브클래스 추가 항목)
			// 복원 — RootComponent 복원 뒤여야 SetActorLocation 등이 적용됨.
			if (ActorJSON.hasKey(SceneKeys::Properties)) {
				LoadContextState.QueueProperties(Actor, ActorJSON[SceneKeys::Properties]);
			}

			// Non-scene components 복원
			if (ActorJSON.hasKey(SceneKeys::NonSceneComponents)) {
				for (auto& CompJSON : ActorJSON[SceneKeys::NonSceneComponents].ArrayRange()) {
					string CompClass = CompJSON[SceneKeys::ClassName].ToString();
					UObject* CompObj = FObjectFactory::Get().Create(CompClass, Actor);
					if (!CompObj || !CompObj->IsA<UActorComponent>()) continue;

					UActorComponent* Comp = static_cast<UActorComponent*>(CompObj);
					LoadContextState.RegisterLoadedObject(CompJSON, Comp);
					RestoreEditorUndoRuntimeUUID(CompJSON, Comp);
					if (CompJSON.hasKey(SceneKeys::Name)) {
						Comp->SetFName(FName(CompJSON[SceneKeys::Name].ToString()));
					}
					Actor->RegisterComponent(Comp);

					if (CompJSON.hasKey(SceneKeys::Properties)) {
						JSON& PropsJSON = CompJSON[SceneKeys::Properties];
						LoadContextState.QueueProperties(Comp, PropsJSON);
					}
					DeserializeComponentEditorMetadata(Comp, CompJSON);
				}
			}
		}
	}

	for (FPendingPropertyLoad& Pending : LoadContextState.PendingProperties)
	{
		if (Pending.Object && Pending.Properties)
		{
			DeserializeProperties(Pending.Object, *Pending.Properties, LoadContextState);
		}
	}

	TArray<AActor*> LoadedActors = World->GetActors();
	for (AActor* Actor : LoadedActors)
	{
		if (Actor)
		{
			World->RemoveActorToOctree(Actor);
		}
	}

	// JSON scene loading recreates actors/components from serialized data, but it is not
	// a UObject duplicate path. Run the same post-load fix-up phase so actor subclasses
	// rebuild cached component pointers before gameplay tick/input starts.
	for (AActor* Actor : LoadedActors)
	{
		if (!Actor)
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component)
			{
				Component->PostDuplicate();
			}
		}
	}

	for (AActor* Actor : LoadedActors)
	{
		if (Actor)
		{
			Actor->PostDuplicate();
		}
	}

	if (root.hasKey(SceneKeys::EditorOutliner))
	{
		DeserializeEditorOutliner(root[SceneKeys::EditorOutliner], World, LoadContextState);
	}

	for (AActor* Actor : LoadedActors)
	{
		if (!Actor)
		{
			continue;
		}

		World->InsertActorToOctree(Actor);
	}

	OutWorldContext.WorldType = WorldType;
	OutWorldContext.World = World;
	OutWorldContext.ContextName = ContextName;
	OutWorldContext.ContextHandle = FName(ContextHandle);
}

void FSceneSaveManager::DeserializeEditorOutliner(json::JSON& Node, UWorld* World, FSceneLoadContext& Context)
{
	using json::JSON;
	if (!World || Node.JSONType() == JSON::Class::Null)
	{
		return;
	}

	FSceneOutlinerState& OutlinerState = World->GetEditorOutlinerState();
	OutlinerState.Clear();

	if (!Node.hasKey(SceneKeys::Groups))
	{
		return;
	}

	uint32 MaxGroupId = 0;
	for (auto& GroupJSON : Node[SceneKeys::Groups].ArrayRange())
	{
		FSceneOutlinerGroup Group;
		Group.GroupId = GroupJSON.hasKey(SceneKeys::GroupId)
			? static_cast<uint32>(GroupJSON[SceneKeys::GroupId].ToInt())
			: OutlinerState.NextGroupId++;
		Group.ParentGroupId = GroupJSON.hasKey(SceneKeys::ParentGroupId)
			? static_cast<uint32>(GroupJSON[SceneKeys::ParentGroupId].ToInt())
			: 0;
		Group.Name = GroupJSON.hasKey(SceneKeys::Name)
			? GroupJSON[SceneKeys::Name].ToString()
			: FSceneOutlinerState::MakeDefaultGroupName(Group.GroupId);
		Group.bExpanded = GroupJSON.hasKey(SceneKeys::Expanded)
			? GroupJSON[SceneKeys::Expanded].ToBool()
			: true;

		if (GroupJSON.hasKey(SceneKeys::ActorObjectIds))
		{
			for (auto& ActorIdJSON : GroupJSON[SceneKeys::ActorObjectIds].ArrayRange())
			{
				const uint32 ObjectId = static_cast<uint32>(ActorIdJSON.ToInt());
				AActor* Actor = Cast<AActor>(Context.FindObjectById(ObjectId));
				if (Actor && Actor->GetWorld() == World)
				{
					Group.ActorUUIDs.push_back(Actor->GetUUID());
				}
			}
		}

		MaxGroupId = (std::max)(MaxGroupId, Group.GroupId);
		OutlinerState.Groups.push_back(std::move(Group));
	}

	for (FSceneOutlinerGroup& Group : OutlinerState.Groups)
	{
		if (Group.ParentGroupId == Group.GroupId || !OutlinerState.FindGroup(Group.ParentGroupId))
		{
			Group.ParentGroupId = 0;
		}
	}
	OutlinerState.RemoveEmptyGroups();
	OutlinerState.NextGroupId = (std::max)(OutlinerState.NextGroupId, MaxGroupId + 1);
}

void FSceneSaveManager::DeserializePrefabOutliner(
	json::JSON& Node,
	FSceneLoadContext& Context,
	FSceneOutlinerState& OutlinerState)
{
	using json::JSON;
	if (Node.JSONType() == JSON::Class::Null || !Node.hasKey(SceneKeys::Groups))
	{
		return;
	}

	TMap<uint32, uint32> SourceGroupIdToNewGroupId;
	TArray<std::pair<uint32, uint32>> PendingParentLinks;

	for (auto& GroupJSON : Node[SceneKeys::Groups].ArrayRange())
	{
		TArray<uint32> ActorUUIDs;
		if (GroupJSON.hasKey(SceneKeys::ActorObjectIds))
		{
			for (auto& ActorIdJSON : GroupJSON[SceneKeys::ActorObjectIds].ArrayRange())
			{
				const uint32 ObjectId = static_cast<uint32>(ActorIdJSON.ToInt());
				if (AActor* Actor = Cast<AActor>(Context.FindObjectById(ObjectId)))
				{
					ActorUUIDs.push_back(Actor->GetUUID());
				}
			}
		}

		const FString GroupName = GroupJSON.hasKey(SceneKeys::Name)
			? GroupJSON[SceneKeys::Name].ToString()
			: FString();
		const uint32 NewGroupId = OutlinerState.CreateEmptyGroup(GroupName);
		if (FSceneOutlinerGroup* NewGroup = OutlinerState.FindGroup(NewGroupId))
		{
			NewGroup->ActorUUIDs = std::move(ActorUUIDs);
			NewGroup->bExpanded = GroupJSON.hasKey(SceneKeys::Expanded)
				? GroupJSON[SceneKeys::Expanded].ToBool()
				: true;
		}

		if (GroupJSON.hasKey(SceneKeys::GroupId))
		{
			const uint32 SourceGroupId = static_cast<uint32>(GroupJSON[SceneKeys::GroupId].ToInt());
			SourceGroupIdToNewGroupId[SourceGroupId] = NewGroupId;

			const uint32 SourceParentGroupId = GroupJSON.hasKey(SceneKeys::ParentGroupId)
				? static_cast<uint32>(GroupJSON[SceneKeys::ParentGroupId].ToInt())
				: 0;
			if (SourceParentGroupId != 0)
			{
				PendingParentLinks.push_back({ SourceGroupId, SourceParentGroupId });
			}
		}
	}

	for (const auto& Link : PendingParentLinks)
	{
		auto ChildIt = SourceGroupIdToNewGroupId.find(Link.first);
		auto ParentIt = SourceGroupIdToNewGroupId.find(Link.second);
		if (ChildIt == SourceGroupIdToNewGroupId.end() || ParentIt == SourceGroupIdToNewGroupId.end())
		{
			continue;
		}

		if (FSceneOutlinerGroup* ChildGroup = OutlinerState.FindGroup(ChildIt->second))
		{
			ChildGroup->ParentGroupId = ParentIt->second;
		}
	}
	OutlinerState.RemoveEmptyGroups();
}

AActor* FSceneSaveManager::DeserializeActorForEditorUndo(UWorld* World, json::JSON ActorJSON)
{
	using json::JSON;
	if (!World || ActorJSON.JSONType() == JSON::Class::Null)
	{
		return nullptr;
	}

	if (ActorJSON.hasKey(SceneKeys::RuntimeUUID))
	{
		const uint32 RuntimeUUID = static_cast<uint32>(ActorJSON[SceneKeys::RuntimeUUID].ToInt());
		if (RuntimeUUID != 0)
		{
			if (AActor* ExistingActor = Cast<AActor>(UObjectManager::Get().FindByUUID(RuntimeUUID)))
			{
				if (ExistingActor->GetWorld() == World)
				{
					return ExistingActor;
				}
			}
		}
	}

	string ActorClass = ActorJSON[SceneKeys::ClassName].ToString();
	if (ActorClass.empty())
	{
		return nullptr;
	}

	UObject* ActorObj = FObjectFactory::Get().Create(ActorClass, World);
	AActor* Actor = Cast<AActor>(ActorObj);
	if (!Actor)
	{
		if (ActorObj)
		{
			UObjectManager::Get().DestroyObject(ActorObj);
		}
		return nullptr;
	}

	FSceneLoadContext LoadContextState;
	RegisterEditorUndoObjectReferenceMap(ActorJSON, LoadContextState);
	LoadContextState.RegisterLoadedObject(ActorJSON, Actor);
	RestoreEditorUndoRuntimeUUID(ActorJSON, Actor);
	World->AddActor(Actor);

	if (ActorJSON.hasKey(SceneKeys::Name))
	{
		Actor->SetFName(FName(ActorJSON[SceneKeys::Name].ToString()));
	}

	if (ActorJSON.hasKey(SceneKeys::RootComponent))
	{
		JSON& RootJSON = ActorJSON[SceneKeys::RootComponent];
		USceneComponent* Root = DeserializeSceneComponentTree(RootJSON, Actor, LoadContextState);
		if (Root)
		{
			Actor->SetRootComponent(Root);
		}
	}

	if (ActorJSON.hasKey(SceneKeys::Properties))
	{
		LoadContextState.QueueProperties(Actor, ActorJSON[SceneKeys::Properties]);
	}

	if (ActorJSON.hasKey(SceneKeys::NonSceneComponents))
	{
		for (auto& CompJSON : ActorJSON[SceneKeys::NonSceneComponents].ArrayRange())
		{
			string CompClass = CompJSON[SceneKeys::ClassName].ToString();
			UObject* CompObj = FObjectFactory::Get().Create(CompClass, Actor);
			UActorComponent* Comp = Cast<UActorComponent>(CompObj);
			if (!Comp)
			{
				if (CompObj)
				{
					UObjectManager::Get().DestroyObject(CompObj);
				}
				continue;
			}

			LoadContextState.RegisterLoadedObject(CompJSON, Comp);
			RestoreEditorUndoRuntimeUUID(CompJSON, Comp);
			if (CompJSON.hasKey(SceneKeys::Name))
			{
				Comp->SetFName(FName(CompJSON[SceneKeys::Name].ToString()));
			}
			Actor->RegisterComponent(Comp);

			if (CompJSON.hasKey(SceneKeys::Properties))
			{
				LoadContextState.QueueProperties(Comp, CompJSON[SceneKeys::Properties]);
			}
			DeserializeComponentEditorMetadata(Comp, CompJSON);
		}
	}

	for (FPendingPropertyLoad& Pending : LoadContextState.PendingProperties)
	{
		if (Pending.Object && Pending.Properties)
		{
			DeserializeProperties(Pending.Object, *Pending.Properties, LoadContextState);
		}
	}

	World->RemoveActorToOctree(Actor);
	World->InsertActorToOctree(Actor);
	World->BuildWorldPrimitivePickingBVHNow();
	return Actor;
}

void FSceneSaveManager::ApplyActorSnapshotForEditorUndo(AActor* Actor, json::JSON ActorJSON)
{
	using json::JSON;
	if (!Actor || ActorJSON.JSONType() == JSON::Class::Null)
	{
		return;
	}

	UWorld* World = Actor->GetWorld();
	if (World)
	{
		// 기존 component proxy가 octree에 남아 있지 않도록 actor 단위 partition 항목을 먼저 제거합니다.
		World->BeginDeferredPickingBVHUpdate();
		World->RemoveActorToOctree(Actor);
	}

	// 기존 actor 객체는 유지하고 component 구조만 snapshot 기준으로 다시 구성합니다.
	ClearActorComponentsForEditorUndo(Actor);

	FSceneLoadContext LoadContextState;
	RegisterEditorUndoObjectReferenceMap(ActorJSON, LoadContextState);
	LoadContextState.RegisterLoadedObject(ActorJSON, Actor);
	RestoreEditorUndoRuntimeUUID(ActorJSON, Actor);

	if (ActorJSON.hasKey(SceneKeys::Name))
	{
		Actor->SetFName(FName(ActorJSON[SceneKeys::Name].ToString()));
	}

	if (ActorJSON.hasKey(SceneKeys::RootComponent))
	{
		JSON& RootJSON = ActorJSON[SceneKeys::RootComponent];
		USceneComponent* Root = DeserializeSceneComponentTree(RootJSON, Actor, LoadContextState);
		if (Root)
		{
			Actor->SetRootComponent(Root);
		}
	}

	if (ActorJSON.hasKey(SceneKeys::Properties))
	{
		LoadContextState.QueueProperties(Actor, ActorJSON[SceneKeys::Properties]);
	}

	if (ActorJSON.hasKey(SceneKeys::NonSceneComponents))
	{
		for (auto& CompJSON : ActorJSON[SceneKeys::NonSceneComponents].ArrayRange())
		{
			string CompClass = CompJSON[SceneKeys::ClassName].ToString();
			UObject* CompObj = FObjectFactory::Get().Create(CompClass, Actor);
			UActorComponent* Comp = Cast<UActorComponent>(CompObj);
			if (!Comp)
			{
				if (CompObj)
				{
					UObjectManager::Get().DestroyObject(CompObj);
				}
				continue;
			}

			LoadContextState.RegisterLoadedObject(CompJSON, Comp);
			RestoreEditorUndoRuntimeUUID(CompJSON, Comp);
			if (CompJSON.hasKey(SceneKeys::Name))
			{
				Comp->SetFName(FName(CompJSON[SceneKeys::Name].ToString()));
			}
			Actor->RegisterComponent(Comp);

			if (CompJSON.hasKey(SceneKeys::Properties))
			{
				LoadContextState.QueueProperties(Comp, CompJSON[SceneKeys::Properties]);
			}
			DeserializeComponentEditorMetadata(Comp, CompJSON);
		}
	}

	for (FPendingPropertyLoad& Pending : LoadContextState.PendingProperties)
	{
		if (Pending.Object && Pending.Properties)
		{
			DeserializeProperties(Pending.Object, *Pending.Properties, LoadContextState);
		}
	}

	// actor subclass가 들고 있는 component raw pointer를 새 component 인스턴스로 다시 연결합니다.
	Actor->PostDuplicate();

	if (World)
	{
		World->InsertActorToOctree(Actor);
		World->MarkWorldPrimitivePickingBVHDirty();
		World->EndDeferredPickingBVHUpdate();
	}
}

USceneComponent* FSceneSaveManager::DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner, FSceneLoadContext& Context)
{
	string ClassName = Node[SceneKeys::ClassName].ToString();
	UObject* Obj = FObjectFactory::Get().Create(ClassName, Owner);
	if (!Obj || !Obj->IsA<USceneComponent>()) return nullptr;

	USceneComponent* Comp = static_cast<USceneComponent*>(Obj);
	Context.RegisterLoadedObject(Node, Comp);
	RestoreEditorUndoRuntimeUUID(Node, Comp);
	if (Node.hasKey(SceneKeys::Name)) {
		Comp->SetFName(FName(Node[SceneKeys::Name].ToString()));
	}
	Owner->RegisterComponent(Comp);

	// Restore properties
	if (Node.hasKey(SceneKeys::Properties)) {
		json::JSON& PropsJSON = Node[SceneKeys::Properties];
		Context.QueueProperties(Comp, PropsJSON);
	}
	DeserializeComponentEditorMetadata(Comp, Node);
	Comp->MarkTransformDirty();

	// Restore children recursively
	if (Node.hasKey(SceneKeys::Children)) {
		for (auto& ChildJSON : Node[SceneKeys::Children].ArrayRange()) {
			USceneComponent* Child = DeserializeSceneComponentTree(ChildJSON, Owner, Context);
			if (Child) {
				Child->AttachToComponent(Comp);
			}
		}
	}

	EnsureEditorBillboardMetadata(Comp);

	return Comp;
}

void FSceneSaveManager::DeserializeProperties(UObject* Obj, json::JSON& PropsJSON, FSceneLoadContext& Context)
{
	if (!Obj) return;

	TArray<const FProperty*> Properties;
	Obj->GetClass()->GetPropertyRefs(Properties);
	for (const FProperty* Property : Properties)
	{
		if(!Property || (Property->Flags & PF_Save) == 0)
		{
			continue;
		}

		const char* PropertyKey = Property->Name;
		if (!PropsJSON.hasKey(PropertyKey) && Property->DisplayName && PropsJSON.hasKey(Property->DisplayName))
		{
			PropertyKey = Property->DisplayName;
		}

		if (!PropsJSON.hasKey(PropertyKey))
		{
			continue;
		}

		if (PropertyKey != Property->Name)
		{
			PropsJSON[Property->Name] = PropsJSON[PropertyKey];
		}
	}

	for (const FProperty* Property : Properties)
	{
		if(!Property || (Property->Flags & PF_Save) == 0)
		{
			continue;
		}

		const char* PropertyKey = Property->Name;
		if (!PropsJSON.hasKey(PropertyKey))
		{
			continue;
		}

		if(!Property->GetValuePtrFor(Obj))
		{
			continue;
		}

		FSceneJsonLoadArchive Ar(PropsJSON[PropertyKey], Context);
		Property->Serialize(Obj, Ar);

		FPropertyChangedEvent Event;
		Event.Object = Obj;
		Event.Property = Property;
		Event.PropertyName = Property->Name;
		Event.DisplayName = Property->DisplayName ? Property->DisplayName : Property->Name;
		Event.PropertyPath = Property->Name;
		Event.Type = Property->GetType();
		Event.ChangeType = EPropertyChangeType::Load;
		Obj->PostEditChangeProperty(Event);
	}

}

// ============================================================
// Utility
// ============================================================

string FSceneSaveManager::GetCurrentTimeStamp()
{
	std::time_t t = std::time(nullptr);
	std::tm tm{};
	localtime_s(&tm, &t);

	char buf[20];
	std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
	return buf;
}

TArray<FString> FSceneSaveManager::GetSceneFileList()
{
	TArray<FString> Result;
	std::wstring SceneDir = GetSceneDirectory();
	if (!std::filesystem::exists(SceneDir))
	{
		return Result;
	}

	for (auto& Entry : std::filesystem::directory_iterator(SceneDir))
	{
		if (Entry.is_regular_file() && Entry.path().extension() == SceneExtension)
		{
			Result.push_back(FPaths::ToUtf8(Entry.path().stem().wstring()));
		}
	}
	return Result;
}
