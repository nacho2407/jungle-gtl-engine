#include "FJsonConverter.h"

#include <string>

using Json = nlohmann::json;

Json FJsonConverter::ToJson(const FWorldSaveData& WorldSaveData)
{
    Json Root;
    Root["Version"] = WorldSaveData.Version;
    Root["NextUUID"] = WorldSaveData.NextUUID;

    Json PrimitivesJson = Json::object();

    for (const FPrimitiveRecord& Record: WorldSaveData.Primitives)
    {
        PrimitivesJson[std::to_string(Record.SaveID)] = ToJson(Record);
    }

    Root["Primitives"] = PrimitivesJson;

    return Root;
}

bool FJsonConverter::FromJson(const Json& InJson, FWorldSaveData& OutWorldSaveData)
{
    if (!InJson.is_object())
    {
        return false;
    }

    if (!InJson.contains("Version") || !InJson["Version"].is_number_unsigned())
    {
        return false;
    }

    if (!InJson.contains("NextUUID") || !InJson["NextUUID"].is_number_unsigned())
    {
        return false;
    }

    if (!InJson.contains("Primitives") || !InJson["Primitives"].is_object())
    {
        return false;
    }

    OutWorldSaveData.Version = InJson["Version"].get<uint32>();

    // 기존 데이터 초기화
    OutWorldSaveData.Primitives.empty();

    const Json& PrimitivesJson = InJson["Primitives"];

    for (Json::const_iterator It = PrimitivesJson.begin(); It != PrimitivesJson.end(); ++It)
    {
        const Json& PrimitiveJson = It.value();

        FPrimitiveRecord Record;
        if (!FromJson(PrimitiveJson, Record))
        {
            return false;
        }

        OutWorldSaveData.Primitives.push_back(Record);
    }

    return true;
}

FString FJsonConverter::ToString(const FWorldSaveData& WorldSaveData, int32 Indent)
{
    return ToJson(WorldSaveData).dump(Indent);
}

bool FJsonConverter::FromString(const FString& JsonString, FWorldSaveData& OutWorldSaveData)
{
    try
    {
        Json Parsed = Json::parse(JsonString);
        return FromJson(Parsed, OutWorldSaveData);
    }
    catch (const Json::parse_error&)
    {
        return false;
    }
    catch (const Json::type_error&)
    {
        return false;
    }
    catch (...)
    {
        return false;
    }
}

Json FJsonConverter::ToJson(const FPrimitiveRecord& Record)
{
    Json Obj;
    Obj["Type"] = Record.Type;
    Obj["Location"] = VectorToJson(Record.Location);
    Obj["Rotation"] = VectorToJson(Record.Rotation);
    Obj["Scale"] = VectorToJson(Record.Scale);
    return Obj;
}

bool FJsonConverter::FromJson(const Json& InJson, FPrimitiveRecord& OutRecord)
{
    if (!InJson.is_object())
    {
        return false;
    }

    if (!InJson.contains("Type") || !InJson["Type"].is_string())
    {
        return false;
    }

    if (!InJson.contains("Location") || !InJson.contains("Rotation") || !InJson.contains("Scale"))
    {
        return false;
    }

    FVector Location;
    FVector Rotation;
    FVector Scale;

    if (!JsonToVector(InJson["Location"], Location))
    {
        return false;
    }

    if (!JsonToVector(InJson["Rotation"], Rotation))
    {
        return false;
    }

    if (!JsonToVector(InJson["Scale"], Scale))
    {
        return false;
    }

    OutRecord.Type = InJson["Type"].get<FString>();
    OutRecord.Location = Location;
    OutRecord.Rotation = Rotation;
    OutRecord.Scale = Scale;

    return true;
}

FJsonConverter::Json FJsonConverter::VectorToJson(const FVector& Value)
{
    Json Array = Json::array();

    Array.push_back(Value.X);
    Array.push_back(Value.Y);
    Array.push_back(Value.Z);

    return Array;
}

bool FJsonConverter::JsonToVector(const Json& InJson, FVector& OutValue)
{
    if (!InJson.is_array() || InJson.size() != 3)
    {
        return false;
    }

    if (!InJson[0].is_number() || !InJson[1].is_number() || !InJson[2].is_number())
    {
        return false;
    }

    OutValue.X = InJson[0].get<float>();
    OutValue.Y = InJson[1].get<float>();
    OutValue.Z = InJson[2].get<float>();

    return true;
}