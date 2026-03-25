#include "Scene.h"

#include "World.h"
#include "ThirdParty/json.h"
#include "Component/SceneComponent.h"
#include "Component/CubeComponent.h"
#include "Component/SphereComponent.h"
#include "Component/TriangleComponent.h"
#include "Component/PlaneComponent.h"
#include "Component/BillboardSubUVComponent.h"
#include "Component/TextMeshComponent.h"
#include "EngineCore/Logger.h"

UScene::UScene()
{
}

UScene::~UScene()
{
}

void UScene::Update(float DeltaTime)
{
	for (USceneComponent* sc : SceneComponents)
	{
		sc->Update(DeltaTime);
	}
}

void UScene::DrawDebugLines()
{
	for (USceneComponent* sc : SceneComponents)
	{
		sc->DrawDebugLines();
	}
}

std::string UScene::ToJson()
{
	return ToJson(this);
}

std::string UScene::ToJson(UScene* scene)
{
	json::JSON json;
	json["Version"] = scene->Version;
	json["NextUUID"] = scene->NextUUID;

	auto compare = [](USceneComponent* a, USceneComponent* b) { return a->UUID < b->UUID; };
	if (!std::is_sorted(scene->SceneComponents.begin(), scene->SceneComponents.end(), compare))
	{
		std::sort(scene->SceneComponents.begin(), scene->SceneComponents.end(), compare);
	}

	json::JSON arr;
	for (USceneComponent* s : scene->SceneComponents)
	{
		arr[std::to_string(s->UUID)] = s->Serialize();
	}
	json["Primitives"] = arr;

	return json.dump();
}

namespace
{
	static USceneComponent* CreateSceneComponentFromType(const FString& typeStr, UScene* scene)
	{
		if (scene == nullptr)
		{
			return nullptr;
		}

		UObject* object = nullptr;
		if (typeStr == "Cube")
			object = FObjectFactory::ConstructObject(UCubeComp::GetClass());
		else if (typeStr == "Sphere")
			object = FObjectFactory::ConstructObject(USphereComp::GetClass());
		else if (typeStr == "Triangle")
			object = FObjectFactory::ConstructObject(UTriangleComp::GetClass());
		else if (typeStr == "Plane")
			object = FObjectFactory::ConstructObject(UPlaneComp::GetClass());
		else if (typeStr == "BillboardSubUV")
			object = FObjectFactory::ConstructObject(UBillboardSubUVComp::GetClass());
		else if (typeStr == "TextMesh")
			object = FObjectFactory::ConstructObject(UTextMeshComponent::GetClass());

		USceneComponent* newSc = Cast<USceneComponent>(object);
		if (newSc == nullptr)
		{
			return nullptr;
		}

		newSc->Scene = scene;
		scene->SceneComponents.push_back(newSc);
		newSc->OnComponentAdded();
		return newSc;
	}
}

UScene* UScene::FromJson(std::string jsonStr)
{
	UObject* Object = FObjectFactory::ConstructObject(UScene::GetClass());
	UScene* scene = Cast<UScene>(Object);
	if (scene == nullptr)
	{
		return nullptr;
	}

	json::JSON obj;
	obj = json::JSON::Load(jsonStr);

	scene->Version = obj["Version"].ToInt();
	scene->NextUUID = obj["NextUUID"].ToInt();

	json::JSON primitives = obj["Primitives"];
	if (primitives.JSONType() != json::JSON::Class::Object)
	{
		return scene;
	}

	for (auto pair : primitives.ObjectRange())
	{
		FString typeStr = pair.second["Type"].ToString();
		USceneComponent* newSc = CreateSceneComponentFromType(typeStr, scene);

		if (newSc == nullptr)
			continue;

		newSc->Scene = scene;
		UPrimitiveComponent* prim = Cast<UPrimitiveComponent>(newSc);
		if (prim != nullptr)
		{
			prim->MarkRenderStateDirty();
		}

		newSc->Deserialize(pair.second);
		newSc->UUID = std::stoi(pair.first);
	}

	return scene;
}

REGISTER_CLASS(UScene);
