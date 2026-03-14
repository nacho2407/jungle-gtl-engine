#include "Scene.h"
#include "World.h"
#include "json.h"
#include "USceneComponent.h"
#include "UCubeComp.h"
#include "USphereComp.h"
#include "UTriangleComp.h"
#include "Logger.h"

UScene::UScene()
{
}

UScene::~UScene()
{
}

void UScene::Update(float DeltaTime)
{
	for (USceneComponent* Prim : SceneComponents)
	{
		Prim->Update(DeltaTime);
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
			newSc = Cast<USceneComponent>(FObjectFactory::ConstructObject(UCubeComp::GetClass()));
		if (typeStr == "Sphere")
			newSc = Cast<USceneComponent>(FObjectFactory::ConstructObject(USphereComp::GetClass()));
		if (typeStr == "Triangle")
			newSc = Cast<USceneComponent>(FObjectFactory::ConstructObject(UTriangleComp::GetClass()));

		if (newSc == nullptr)
			continue;

		newSc->Deserialize(pair.second);
		newSc->UUID = std::stoi(pair.first);
		scene->SceneComponents.push_back(newSc);
	}

	return scene;
}

REGISTER_CLASS(UScene);