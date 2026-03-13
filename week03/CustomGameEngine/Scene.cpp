#include "Scene.h"
#include "json.hpp"

constexpr int INDENT = 4;

std::string FScene::ToJson()
{
    return ToJson(*this);
}

json::JSON SerializeUObject(UObject& s)
{
    json::JSON obj;
    // TODO: Primitive 구현되면 serialize 코드에 반영
    obj["Data"] = "This is test data";
    return obj;
}

std::string FScene::ToJson(FScene scene)
{
    json::JSON json;
    json["Version"] = scene.Version;
    json["NextUUID"] = scene.NextUUID;

    json::JSON arr;
    for (auto s : scene.Primitives) 
    {    
        arr[std::to_string(s.UUID)] = SerializeUObject(s);
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
    //for (auto& s : obj["Primitives"]) {
    //    scene->Primitives.push_back(s);
    //}

    return scene;
}
