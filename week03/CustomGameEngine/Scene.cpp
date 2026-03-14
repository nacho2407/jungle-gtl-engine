#include "Scene.h"
#include "json.h"
#include "URender.h"
#include "UPrimitiveComponent.h"
constexpr int INDENT = 4;

UScene::UScene()
{
}

UScene::~UScene()
{
}

void UScene::Update(float DeltaTime)
{
	for (UPrimitiveComponent* Prim : PrimitiveContainer)
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
	for (auto s : scene->Primitives)
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
		USceneComponent* s = Cast<USceneComponent>(
			FObjectFactory::ConstructObject(USceneComponent::GetClass())
			);
		s->Deserialize(pair.second);
		s->UUID = std::stoi(pair.first);
		scene->Primitives.push_back(s);
	}

	return scene;
}

void UScene::Render() const
{
	for (UPrimitiveComponent* PrimComponent : PrimitiveContainer)
	{
		URenderObject* Obj = PrimComponent->Render();
		Renderer->PushRenderObject(Obj);
	}
}

REGISTER_CLASS(UScene);