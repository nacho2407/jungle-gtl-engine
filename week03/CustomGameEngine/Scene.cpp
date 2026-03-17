#include "Scene.h"
#include "World.h"
#include "json.h"
#include "Component/SceneComponent.h"
#include "Component/CubeComponent.h"
#include "Component/SphereComponent.h"
#include "Component/TriangleComponent.h"
#include "Component/PlaneComponent.h"
#include "Logger.h"

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

std::string UScene::ToJson()
{
	return ToJson(this);
}

std::string UScene::ToJson(UScene* scene)
{
	json::JSON json;
	json["Version"] = scene->Version;
	json["NextUUID"] = scene->NextUUID;

	json::JSON arr;
	for (auto s : scene->SceneComponents)
	{
		arr[std::to_string(s->UUID)] = s->Serialize();
	}
	json["Primitives"] = arr;

	return json.dump();
}

UScene* UScene::FromJson(std::string jsonStr)
{
	//TODO : UScene = new UScene
	UObject* Object = FObjectFactory::ConstructObject(UScene::GetClass());
	UScene* scene = Cast<UScene>(Object);

	json::JSON obj;
	obj = json::JSON::Load(jsonStr);

	scene->Version = obj["Version"].ToInt();
	scene->NextUUID = obj["NextUUID"].ToInt();

	for (auto pair : obj["Primitives"].ObjectRange())
	{
		USceneComponent* newSc = nullptr;
		FString typeStr = pair.second["Type"].ToString();
		if (typeStr == "Cube")
			newSc = GetWorld().AddSceneComponent<UCubeComp>(scene);
		if (typeStr == "Sphere")
			newSc = GetWorld().AddSceneComponent<USphereComp>(scene);
		if (typeStr == "Triangle")
			newSc = GetWorld().AddSceneComponent<UTriangleComp>(scene);
		if (typeStr == "Plane")
			newSc = GetWorld().AddSceneComponent<UPlaneComp>(scene);

		if (newSc == nullptr)
			continue;

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