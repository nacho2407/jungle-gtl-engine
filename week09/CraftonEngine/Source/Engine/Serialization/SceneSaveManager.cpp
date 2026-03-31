#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>

#include "SimpleJSON/json.hpp"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/SceneComponent.h"
#include "Component/ActorComponent.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Object/FName.h"

namespace SceneKeys
{
	static constexpr const char* Version = "Version";
	static constexpr const char* Name = "Name";
	static constexpr const char* ClassName = "ClassName";
	static constexpr const char* WorldType = "WorldType";
	static constexpr const char* ContextName = "ContextName";
	static constexpr const char* ContextHandle = "ContextHandle";
	static constexpr const char* Actors = "Actors";
	static constexpr const char* Visible = "bVisible";
	static constexpr const char* RootComponent = "RootComponent";
	static constexpr const char* NonSceneComponents = "NonSceneComponents";
	static constexpr const char* Properties = "Properties";
	static constexpr const char* Children = "Children";
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

void FSceneSaveManager::SaveSceneAsJSON(const string& InSceneName, FWorldContext& WorldContext)
{
	using namespace json;

	if (!WorldContext.World) return;

	string FinalName = InSceneName.empty()
		? "Save_" + GetCurrentTimeStamp()
		: InSceneName;

	std::wstring SceneDir = GetSceneDirectory();
	std::filesystem::path FileDestination = std::filesystem::path(SceneDir) / (FPaths::ToWide(FinalName) + SceneExtension);
	std::filesystem::create_directories(SceneDir);

	JSON Root = SerializeWorld(WorldContext.World, WorldContext);
	Root[SceneKeys::Version] = 2;
	Root[SceneKeys::Name] = FinalName;

	std::ofstream File(FileDestination);
	if (File.is_open()) {
		File << Root.dump();
		File.flush();
		File.close();
	}
}

json::JSON FSceneSaveManager::SerializeWorld(UWorld* World, const FWorldContext& Ctx)
{
	using namespace json;
	JSON w = json::Object();
	w[SceneKeys::ClassName] = World->GetTypeInfo()->name;
	w[SceneKeys::WorldType] = WorldTypeToString(Ctx.WorldType);
	w[SceneKeys::ContextName] = Ctx.ContextName;
	w[SceneKeys::ContextHandle] = Ctx.ContextHandle.ToString();

	JSON Actors = json::Array();
	for (AActor* Actor : World->GetActors()) {
		if (!Actor) continue;
		Actors.append(SerializeActor(Actor));
	}
	w[SceneKeys::Actors] = Actors;
	return w;
}

json::JSON FSceneSaveManager::SerializeActor(AActor* Actor)
{
	using namespace json;
	JSON a = json::Object();
	a[SceneKeys::ClassName] = Actor->GetTypeInfo()->name;
	a[SceneKeys::Visible] = Actor->IsVisible();

	// RootComponent 트리 직렬화
	if (Actor->GetRootComponent()) {
		a[SceneKeys::RootComponent] = SerializeSceneComponentTree(Actor->GetRootComponent());
	}

	// Non-scene components
	JSON NonScene = json::Array();
	for (UActorComponent* Comp : Actor->GetComponents()) {
		if (!Comp) continue;
		if (Comp->IsA<USceneComponent>()) continue;

		JSON c = json::Object();
		c[SceneKeys::ClassName] = Comp->GetTypeInfo()->name;
		c[SceneKeys::Properties] = SerializeProperties(Comp);
		NonScene.append(c);
	}
	a[SceneKeys::NonSceneComponents] = NonScene;

	return a;
}

json::JSON FSceneSaveManager::SerializeSceneComponentTree(USceneComponent* Comp)
{
	using namespace json;
	JSON c = json::Object();
	c[SceneKeys::ClassName] = Comp->GetTypeInfo()->name;
	c[SceneKeys::Properties] = SerializeProperties(Comp);

	JSON Children = json::Array();
	for (USceneComponent* Child : Comp->GetChildren()) {
		if (!Child) continue;
		Children.append(SerializeSceneComponentTree(Child));
	}
	c[SceneKeys::Children] = Children;

	return c;
}

json::JSON FSceneSaveManager::SerializeProperties(UActorComponent* Comp)
{
	using namespace json;
	JSON props = json::Object();

	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	for (const auto& Prop : Descriptors) {
		props[Prop.Name] = SerializePropertyValue(Prop);
	}
	return props;
}

json::JSON FSceneSaveManager::SerializePropertyValue(const FPropertyDescriptor& Prop)
{
	using namespace json;

	switch (Prop.Type) {
	case EPropertyType::Bool:
		return JSON(*static_cast<bool*>(Prop.ValuePtr));

	case EPropertyType::Int:
		return JSON(*static_cast<int32*>(Prop.ValuePtr));

	case EPropertyType::Float:
		return JSON(static_cast<double>(*static_cast<float*>(Prop.ValuePtr)));

	case EPropertyType::Vec3: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		return arr;
	}
	case EPropertyType::Rotator: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		return arr;
	}
	case EPropertyType::Vec4: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		JSON arr = json::Array();
		arr.append(static_cast<double>(v[0]));
		arr.append(static_cast<double>(v[1]));
		arr.append(static_cast<double>(v[2]));
		arr.append(static_cast<double>(v[3]));
		return arr;
	}
	case EPropertyType::String:
	case EPropertyType::StaticMeshRef:
	case EPropertyType::Material:
		return JSON(*static_cast<FString*>(Prop.ValuePtr));

	case EPropertyType::Name:
		return JSON(static_cast<FName*>(Prop.ValuePtr)->ToString());

	default:
		return JSON();
	}
}

// ============================================================
// Load
// ============================================================

void FSceneSaveManager::LoadSceneFromJSON(const string& filepath, FWorldContext& OutWorldContext)
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
	UObject* WorldObj = FObjectFactory::Get().Create(ClassName);
	if (!WorldObj || !WorldObj->IsA<UWorld>()) return;

	UWorld* World = static_cast<UWorld*>(WorldObj);

	EWorldType WorldType = root.hasKey(SceneKeys::WorldType)
		? StringToWorldType(root[SceneKeys::WorldType].ToString())
		: EWorldType::Editor;
	FString ContextName = root.hasKey(SceneKeys::ContextName)
		? root[SceneKeys::ContextName].ToString()
		: "Loaded Scene";
	FString ContextHandle = root.hasKey(SceneKeys::ContextHandle)
		? root[SceneKeys::ContextHandle].ToString()
		: ContextName;

	// Deserialize Actors
	for (auto& ActorJSON : root[SceneKeys::Actors].ArrayRange()) {
		string ActorClass = ActorJSON[SceneKeys::ClassName].ToString();
		UObject* ActorObj = FObjectFactory::Get().Create(ActorClass);
		if (!ActorObj || !ActorObj->IsA<AActor>()) continue;

		AActor* Actor = static_cast<AActor*>(ActorObj);
		Actor->SetWorld(World);
		World->AddActor(Actor);

		if (ActorJSON.hasKey(SceneKeys::Visible)) {
			Actor->SetVisible(ActorJSON[SceneKeys::Visible].ToBool());
		}

		// RootComponent 트리 복원
		if (ActorJSON.hasKey(SceneKeys::RootComponent)) {
			auto RootJSON = ActorJSON[SceneKeys::RootComponent];
			USceneComponent* Root = DeserializeSceneComponentTree(RootJSON, Actor);
			if (Root) {
				Actor->SetRootComponent(Root);
			}
		}

		// Non-scene components 복원
		if (ActorJSON.hasKey(SceneKeys::NonSceneComponents)) {
			for (auto& CompJSON : ActorJSON[SceneKeys::NonSceneComponents].ArrayRange()) {
				string CompClass = CompJSON[SceneKeys::ClassName].ToString();
				UObject* CompObj = FObjectFactory::Get().Create(CompClass);
				if (!CompObj || !CompObj->IsA<UActorComponent>()) continue;

				UActorComponent* Comp = static_cast<UActorComponent*>(CompObj);
				Actor->RegisterComponent(Comp);

				if (CompJSON.hasKey(SceneKeys::Properties)) {
					auto PropsJSON = CompJSON[SceneKeys::Properties];
					DeserializeProperties(Comp, PropsJSON);
				}
			}
		}
	}

	OutWorldContext.WorldType = WorldType;
	OutWorldContext.World = World;
	OutWorldContext.ContextName = ContextName;
	OutWorldContext.ContextHandle = FName(ContextHandle);
}

USceneComponent* FSceneSaveManager::DeserializeSceneComponentTree(json::JSON& Node, AActor* Owner)
{
	string ClassName = Node[SceneKeys::ClassName].ToString();
	UObject* Obj = FObjectFactory::Get().Create(ClassName);
	if (!Obj || !Obj->IsA<USceneComponent>()) return nullptr;

	USceneComponent* Comp = static_cast<USceneComponent*>(Obj);
	Owner->RegisterComponent(Comp);

	// Restore properties
	if (Node.hasKey(SceneKeys::Properties)) {
		auto PropsJSON = Node[SceneKeys::Properties];
		DeserializeProperties(Comp, PropsJSON);
	}
	Comp->MarkTransformDirty();

	// Restore children recursively
	if (Node.hasKey(SceneKeys::Children)) {
		for (auto& ChildJSON : Node[SceneKeys::Children].ArrayRange()) {
			USceneComponent* Child = DeserializeSceneComponentTree(ChildJSON, Owner);
			if (Child) {
				Child->AttachToComponent(Comp);
			}
		}
	}

	return Comp;
}

void FSceneSaveManager::DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON)
{
	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	for (auto& Prop : Descriptors) {
		if (!PropsJSON.hasKey(Prop.Name.c_str())) continue;
		auto Value = PropsJSON[Prop.Name.c_str()];
		DeserializePropertyValue(Prop, Value);
		Comp->PostEditProperty(Prop.Name.c_str());
	}

	// 2nd pass: PostEditProperty가 새 프로퍼티를 추가할 수 있음
	// (예: SetStaticMesh → OverrideMaterialPaths 생성 → "Element N" 디스크립터 추가)
	TArray<FPropertyDescriptor> Descriptors2;
	Comp->GetEditableProperties(Descriptors2);

	for (size_t i = Descriptors.size(); i < Descriptors2.size(); ++i) {
		auto& Prop = Descriptors2[i];
		if (!PropsJSON.hasKey(Prop.Name.c_str())) continue;
		auto Value = PropsJSON[Prop.Name.c_str()];
		DeserializePropertyValue(Prop, Value);
		Comp->PostEditProperty(Prop.Name.c_str());
	}
}

void FSceneSaveManager::DeserializePropertyValue(FPropertyDescriptor& Prop, json::JSON& Value)
{
	switch (Prop.Type) {
	case EPropertyType::Bool:
		*static_cast<bool*>(Prop.ValuePtr) = Value.ToBool();
		break;

	case EPropertyType::Int:
		*static_cast<int32*>(Prop.ValuePtr) = Value.ToInt();
		break;

	case EPropertyType::Float:
		*static_cast<float*>(Prop.ValuePtr) = static_cast<float>(Value.ToFloat());
		break;

	case EPropertyType::Vec3: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 3) v[i] = static_cast<float>(elem.ToFloat());
			i++;
		}
		break;
	}
	case EPropertyType::Rotator: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 3) v[i] = static_cast<float>(elem.ToFloat());
			i++;
		}
		break;
	}
	case EPropertyType::Vec4: {
		float* v = static_cast<float*>(Prop.ValuePtr);
		int i = 0;
		for (auto& elem : Value.ArrayRange()) {
			if (i < 4) v[i] = static_cast<float>(elem.ToFloat());
			i++;
		}
		break;
	}
	case EPropertyType::String:
	case EPropertyType::StaticMeshRef:
	case EPropertyType::Material:
		*static_cast<FString*>(Prop.ValuePtr) = Value.ToString();
		break;

	case EPropertyType::Name:
		*static_cast<FName*>(Prop.ValuePtr) = FName(Value.ToString());
		break;

	default:
		break;
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
