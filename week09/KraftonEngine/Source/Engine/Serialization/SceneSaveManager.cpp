#include "SceneSaveManager.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <unordered_map>

#include "SimpleJSON/json.hpp"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/SceneComponent.h"
#include "Component/ActorComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/CameraComponent.h"
#include "GameFramework/StaticMeshActor.h"
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

	// ---- Primitives: gather static mesh components into a top-level block
	JSON Primitives = json::Object();
	std::unordered_map<AActor*, string> ActorPrimitiveKey;

	for (AActor* Actor : World->GetActors()) {
		if (!Actor) continue;

		for (UActorComponent* Comp : Actor->GetComponents()) {
			if (!Comp) continue;
			if (!Comp->IsA<UStaticMeshComponent>()) continue;

			UStaticMeshComponent* S = static_cast<UStaticMeshComponent*>(Comp);

			JSON p = json::Object();

			const FMatrix& M = S->GetWorldMatrix();
			FVector loc = M.GetLocation();
			FVector rot = M.GetEuler();
			FVector scale = M.GetScale();

			p["ObjStaticMeshAsset"] = S->GetStaticMeshPath();

			JSON locArr = json::Array();
			locArr.append(static_cast<double>(loc.X));
			locArr.append(static_cast<double>(loc.Y));
			locArr.append(static_cast<double>(loc.Z));
			p["Location"] = locArr;

			JSON rotArr = json::Array();
			rotArr.append(static_cast<double>(rot.X));
			rotArr.append(static_cast<double>(rot.Y));
			rotArr.append(static_cast<double>(rot.Z));
			p["Rotation"] = rotArr;

			JSON scaleArr = json::Array();
			scaleArr.append(static_cast<double>(scale.X));
			scaleArr.append(static_cast<double>(scale.Y));
			scaleArr.append(static_cast<double>(scale.Z));
			p["Scale"] = scaleArr;

			p["Type"] = "StaticMeshComp";

			// Note: per design, material/UV overrides are stored on the Actor->RootComponent Properties
			// and are NOT duplicated inside the Primitives block to avoid redundancy.

			// Use the Actor's UUID as the primitive key to avoid positional/index coupling
			string key = std::to_string(Actor->GetUUID());
			Primitives[key] = p;
			ActorPrimitiveKey[Actor] = key;

			// only first static mesh component per actor is exported as primitive
			break;
		}
	}

	if (Primitives.size() > 0) {
		w["Primitives"] = Primitives;
	}

	// ---- Actors: serialize and attach PrimitiveKey when present ----
	JSON Actors = json::Array();
	for (AActor* Actor : World->GetActors()) {
		if (!Actor) continue;
		JSON a = SerializeActor(Actor);
		auto it = ActorPrimitiveKey.find(Actor);
		if (it != ActorPrimitiveKey.end()) {
			a["PrimitiveKey"] = it->second;
		}
		Actors.append(a);
	}
	w[SceneKeys::Actors] = Actors;

	// ---- Camera: serialize active camera if present ----
	JSON cam = SerializeCamera(World);
	if (cam.size() > 0) {
		w["Camera"] = cam;
	}

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

	// Special-case UStaticMeshComponent: expose material overrides and UVScrolls as arrays
	if (Comp->IsA<UStaticMeshComponent>()) {
		JSON materials = json::Array();
		JSON uvscrolls = json::Array();

		for (const auto& Prop : Descriptors) {
			// Collect Element N and UVScroll N into arrays instead of numbered keys
			if (Prop.Name == "Static Mesh") {
				// Static Mesh is stored in the top-level Primitives block; avoid duplicating it here.
				continue;
			}
			if (Prop.Name.rfind("Element ", 0) == 0) {
				// Material path
				FString* Val = static_cast<FString*>(Prop.ValuePtr);
				materials.append(JSON(*Val));
				continue;
			}
			if (Prop.Name.rfind("UVScroll ", 0) == 0) {
				uint8_t* Val = static_cast<uint8_t*>(Prop.ValuePtr);
				uvscrolls.append(JSON(static_cast<bool>(*Val != 0)));
				continue;
			}
			props[Prop.Name] = SerializePropertyValue(Prop);
		}

		props["OverrideMaterials"] = materials;
		props["OverrideUVScrolls"] = uvscrolls;
		return props;
	}

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

	case EPropertyType::ByteBool:
		return JSON(static_cast<bool>(*static_cast<uint8_t*>(Prop.ValuePtr) != 0));

	case EPropertyType::Name:
		return JSON(static_cast<FName*>(Prop.ValuePtr)->ToString());

	default:
		return JSON();
	}
}

// ---- Camera + Primitives helpers ----

json::JSON FSceneSaveManager::SerializeCamera(UWorld* World)
{
	using namespace json;
	JSON cam = json::Object();

	for (AActor* Actor : World->GetActors()) {
		if (!Actor) continue;
		for (UActorComponent* Comp : Actor->GetComponents()) {
			if (!Comp) continue;
			if (!Comp->IsA<UCameraComponent>()) continue;

			UCameraComponent* C = static_cast<UCameraComponent*>(Comp);
			const FMatrix& M = C->GetWorldMatrix();
			FVector loc = M.GetLocation();
			FVector rot = M.GetEuler();
			FVector scale = M.GetScale();

			JSON locArr = json::Array();
			locArr.append(static_cast<double>(loc.X));
			locArr.append(static_cast<double>(loc.Y));
			locArr.append(static_cast<double>(loc.Z));
			cam["Location"] = locArr;

			JSON rotArr = json::Array();
			rotArr.append(static_cast<double>(rot.X));
			rotArr.append(static_cast<double>(rot.Y));
			rotArr.append(static_cast<double>(rot.Z));
			cam["Rotation"] = rotArr;

			JSON scaleArr = json::Array();
			scaleArr.append(static_cast<double>(scale.X));
			scaleArr.append(static_cast<double>(scale.Y));
			scaleArr.append(static_cast<double>(scale.Z));
			cam["Scale"] = scaleArr;

			const FCameraState& S = C->GetCameraState();
			JSON state = json::Object();
			state["FOV"] = static_cast<double>(S.FOV);
			state["AspectRatio"] = static_cast<double>(S.AspectRatio);
			state["NearZ"] = static_cast<double>(S.NearZ);
			state["FarZ"] = static_cast<double>(S.FarZ);
			state["OrthoWidth"] = static_cast<double>(S.OrthoWidth);
			state["bIsOrthogonal"] = S.bIsOrthogonal;
			cam["State"] = state;

			return cam; // first found camera
		}
	}
	return cam;
}

void FSceneSaveManager::DeserializePrimitives(json::JSON& Primitives, UWorld* World, std::unordered_map<string, AActor*>& OutCreatedActors)
{
	using namespace json;
	for (auto it = Primitives.ObjectRange().begin(); it != Primitives.ObjectRange().end(); ++it) {
		const auto& kv = *it;
		const string& Key = kv.first;
		JSON Entry = kv.second;

		if (!Entry.hasKey("Type")) continue;
		if (Entry["Type"].ToString() != "StaticMeshComp") continue;

		string MeshPath = Entry.hasKey("ObjStaticMeshAsset") ? Entry["ObjStaticMeshAsset"].ToString() : string("None");

		// Spawn a static mesh actor and initialize
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
		if (!Actor) continue;
		Actor->InitDefaultComponents(FString(MeshPath));
		OutCreatedActors[Key] = Actor;

		// Location / Rotation / Scale
		if (Entry.hasKey("Location")) {
			auto locjson = Entry["Location"];
			float lx = 0, ly = 0, lz = 0;
			int i = 0;
			for (auto& e : locjson.ArrayRange()) { if (i==0) lx = static_cast<float>(e.ToFloat()); else if (i==1) ly = static_cast<float>(e.ToFloat()); else if (i==2) lz = static_cast<float>(e.ToFloat()); i++; }
			Actor->SetActorLocation(FVector(lx, ly, lz));
		}
		if (Entry.hasKey("Rotation")) {
			auto rotjson = Entry["Rotation"];
			float rx = 0, ry = 0, rz = 0;
			int i = 0;
			for (auto& e : rotjson.ArrayRange()) { if (i==0) rx = static_cast<float>(e.ToFloat()); else if (i==1) ry = static_cast<float>(e.ToFloat()); else if (i==2) rz = static_cast<float>(e.ToFloat()); i++; }
			Actor->SetActorRotation(FVector(rx, ry, rz));
		}
		if (Entry.hasKey("Scale")) {
			auto sjson = Entry["Scale"];
			float sx = 1, sy = 1, sz = 1;
			int i = 0;
			for (auto& e : sjson.ArrayRange()) { if (i==0) sx = static_cast<float>(e.ToFloat()); else if (i==1) sy = static_cast<float>(e.ToFloat()); else if (i==2) sz = static_cast<float>(e.ToFloat()); i++; }
			Actor->SetActorScale(FVector(sx, sy, sz));
		}

		// Material/UV overrides are applied later when deserializing the Actor's RootComponent properties
	}
}

void FSceneSaveManager::DeserializeCamera(json::JSON& CameraJSON, UWorld* World)
{
	using namespace json;
	if (CameraJSON.JSONType() == JSON::Class::Null) return;

	UCameraComponent* Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
	if (!Camera) return;

	if (CameraJSON.hasKey("Location")) {
		auto l = CameraJSON["Location"];
		int i = 0; float x=0,y=0,z=0;
		for (auto& e : l.ArrayRange()) { if (i==0) x = static_cast<float>(e.ToFloat()); else if (i==1) y = static_cast<float>(e.ToFloat()); else if (i==2) z = static_cast<float>(e.ToFloat()); i++; }
		Camera->SetWorldLocation(FVector(x,y,z));
	}
	if (CameraJSON.hasKey("Rotation")) {
		auto r = CameraJSON["Rotation"];
		int i=0; float rx=0,ry=0,rz=0;
		for (auto& e : r.ArrayRange()) { if (i==0) rx = static_cast<float>(e.ToFloat()); else if (i==1) ry = static_cast<float>(e.ToFloat()); else if (i==2) rz = static_cast<float>(e.ToFloat()); i++; }
		Camera->SetRelativeRotation(FVector(rx,ry,rz));
	}
	if (CameraJSON.hasKey("State")) {
		auto s = CameraJSON["State"];
		FCameraState cs;
		if (s.hasKey("FOV")) cs.FOV = static_cast<float>(s["FOV"].ToFloat());
		if (s.hasKey("AspectRatio")) cs.AspectRatio = static_cast<float>(s["AspectRatio"].ToFloat());
		if (s.hasKey("NearZ")) cs.NearZ = static_cast<float>(s["NearZ"].ToFloat());
		if (s.hasKey("FarZ")) cs.FarZ = static_cast<float>(s["FarZ"].ToFloat());
		if (s.hasKey("OrthoWidth")) cs.OrthoWidth = static_cast<float>(s["OrthoWidth"].ToFloat());
		if (s.hasKey("bIsOrthogonal")) cs.bIsOrthogonal = s["bIsOrthogonal"].ToBool();
		Camera->SetCameraState(cs);
	}

	World->SetActiveCamera(Camera);
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

	// Deserialize Primitives (top-level) and Camera first
	std::unordered_map<string, AActor*> CreatedFromPrimitives;
	if (root.hasKey("Primitives")) {
		auto Prims = root["Primitives"];
		DeserializePrimitives(Prims, World, CreatedFromPrimitives);
	}

	if (root.hasKey("Camera")) {
		auto Cam = root["Camera"];
		DeserializeCamera(Cam, World);
	}

	// Deserialize Actors
	for (auto& ActorJSON : root[SceneKeys::Actors].ArrayRange()) {
		string ActorClass = ActorJSON[SceneKeys::ClassName].ToString();
		// If this actor references a PrimitiveKey and that primitive already created an actor,
		// prefer the primitive-created actor and update it instead of creating a duplicate.
		AActor* Actor = nullptr;
		if (ActorJSON.hasKey("PrimitiveKey")) {
			string pk = ActorJSON["PrimitiveKey"].ToString();
			auto it = CreatedFromPrimitives.find(pk);
			if (it != CreatedFromPrimitives.end()) {
				Actor = it->second;
			}
		}

		if (!Actor) {
			UObject* ActorObj = FObjectFactory::Get().Create(ActorClass);
			if (!ActorObj || !ActorObj->IsA<AActor>()) continue;
			Actor = static_cast<AActor*>(ActorObj);
			Actor->SetWorld(World);
			World->AddActor(Actor);
		}

		if (ActorJSON.hasKey(SceneKeys::Visible)) {
			Actor->SetVisible(ActorJSON[SceneKeys::Visible].ToBool());
		}

		// RootComponent 트리 복원
		if (ActorJSON.hasKey(SceneKeys::RootComponent)) {
			auto RootJSON = ActorJSON[SceneKeys::RootComponent];
			if (Actor->GetRootComponent()) {
				// Merge properties into existing root component created by primitives
				DeserializeSceneComponentIntoExisting(Actor->GetRootComponent(), RootJSON, Actor);
			} else {
				USceneComponent* Root = DeserializeSceneComponentTree(RootJSON, Actor);
				if (Root) Actor->SetRootComponent(Root);
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

void FSceneSaveManager::DeserializeSceneComponentIntoExisting(USceneComponent* Existing, json::JSON& Node, AActor* Owner)
{
	using namespace json;
	if (!Existing) return;

	if (Node.hasKey(SceneKeys::Properties)) {
		auto PropsJSON = Node[SceneKeys::Properties];
		DeserializeProperties(Existing, PropsJSON);
	}

	// Children: merge into existing children by order; create new children if missing
	if (Node.hasKey(SceneKeys::Children)) {
		auto& ChildrenJSON = Node[SceneKeys::Children];
		auto ExistingChildren = Existing->GetChildren();

		size_t idx = 0;
		for (auto& ChildJSON : ChildrenJSON.ArrayRange()) {
			if (idx < ExistingChildren.size()) {
				DeserializeSceneComponentIntoExisting(ExistingChildren[idx], const_cast<json::JSON&>(ChildJSON), Owner);
			} else {
				USceneComponent* NewChild = DeserializeSceneComponentTree(const_cast<json::JSON&>(ChildJSON), Owner);
				if (NewChild) NewChild->AttachToComponent(Existing);
			}
			idx++;
		}
	}
}

void FSceneSaveManager::DeserializeProperties(UActorComponent* Comp, json::JSON& PropsJSON)
{
	TArray<FPropertyDescriptor> Descriptors;
	Comp->GetEditableProperties(Descriptors);

	// If this is a UStaticMeshComponent and arrays are present, map them into Element N / UVScroll N descriptors
	if (Comp->IsA<UStaticMeshComponent>()) {
		auto mapArrayToDescriptors = [&](const char* ArrayKey, const char* Prefix) {
			if (!PropsJSON.hasKey(ArrayKey)) return;
			auto arr = PropsJSON[ArrayKey];
			int idx = 0;
			for (auto& e : arr.ArrayRange()) {
				string dname = string(Prefix) + std::to_string(idx);
				for (auto& Prop : Descriptors) {
					if (Prop.Name == dname) {
						DeserializePropertyValue(Prop, const_cast<json::JSON&>(e));
						Comp->PostEditProperty(Prop.Name.c_str());
						break;
					}
				}
				idx++;
			}
		};

		mapArrayToDescriptors("OverrideMaterials", "Element ");
		mapArrayToDescriptors("OverrideUVScrolls", "UVScroll ");
	}

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

	case EPropertyType::ByteBool:
		*static_cast<uint8_t*>(Prop.ValuePtr) = Value.ToBool() ? 1 : 0;
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
