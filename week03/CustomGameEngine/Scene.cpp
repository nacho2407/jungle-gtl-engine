#include "Scene.h"

using json = nlohmann::ordered_json;
constexpr int INDENT = 4;

std::string FScene::ToJson()
{
    // Serialize: 객체 -> JSON
    json j = *this;
    return j.dump(INDENT);
}

std::string FScene::ToJson(FScene scene)
{
    json j = scene;
    return j.dump(INDENT);
}

FScene FScene::FromJson(std::string jsonStr)
{
    nlohmann::json j = nlohmann::json::parse(jsonStr);
    auto result = j.get<FScene>();
    return result;
}
