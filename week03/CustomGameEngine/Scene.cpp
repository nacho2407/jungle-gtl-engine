#include "Scene.h"
#include "json.hpp"

#include "UPrimitiveComponent.h"
constexpr int INDENT = 4;

json::JSON SerializeFVector(FVector v)
{
	json::JSON j;
	j[0] = v.x;
	j[1] = v.y;
	j[2] = v.z;
	return j;
}

FVector DeserializeFVector(json::JSON j)
{
	FVector v;
	v.x = j[0].ToFloat();
	v.y = j[0].ToFloat();
	v.z = j[0].ToFloat();
	return v;
}

json::JSON SerializeSceneComponent(USceneComponent& s)
{
	json::JSON obj;
	obj["Location"] = SerializeFVector(s.RelativeLocation);
	obj["Rotation"] = SerializeFVector(s.RelativeRotation);
	obj["Scale"] = SerializeFVector(s.RelativeScale3D);
	// TODO: RTTI 구현되면 반영
	obj["Type"] = "TYPE SERIALIZATION NOT IMPLEMETED";
	return obj;
}

USceneComponent DeserializeSceneComponent(json::JSON j)
{
	USceneComponent s;
	s.RelativeLocation = DeserializeFVector(j["Location"]);
	s.RelativeLocation = DeserializeFVector(j["Location"]);
	s.RelativeLocation = DeserializeFVector(j["Location"]);
	// TODO: RTTI 구현되면 반영
	return s;
}

void FScene::Update(float DeltaTime)
{
	for (UPrimitiveComponent* Prim : PrimitiveContainer)
	{
		Prim->Update(DeltaTime);
	}
}

std::string FScene::ToJson()
{
	return ToJson(*this);
}

std::string FScene::ToJson(FScene scene)
{
	json::JSON json;
	json["Version"] = scene.Version;
	json["NextUUID"] = scene.NextUUID;

	json::JSON arr;
	for (auto s : scene.Primitives)
	{
		arr[std::to_string(s.UUID)] = SerializeSceneComponent(s);
	}
	json["Primitives"] = arr;

	return json.dump();
}

FScene FScene::FromJson(std::string jsonStr)
{
	FScene scene;

	json::JSON obj;
	obj = json::JSON::Load(jsonStr);

	scene.Version = obj["Version"].ToInt();
	scene.NextUUID = obj["NextUUID"].ToInt();

	for (auto pair : obj["Primitives"].ObjectRange())
	{
		USceneComponent s = DeserializeSceneComponent(pair.second);
		s.UUID = std::stoi(pair.first);
		scene.Primitives.push_back(s);
	}

	return scene;
}

void FScene::Render() const
{
	for (UPrimitiveComponent* PrimComponent : PrimitiveContainer)
	{
		PrimComponent->Render(Renderer);
	}
}
