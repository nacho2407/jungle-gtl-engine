#include "FJsonConverter.h"

FJsonConverter::Json FJsonConverter::ToJson(const FPrimitiveRecord& Record)
{
    Json Json;
    Json["Type"] = Record.Type;
    Json["Location"] = VectorToJson(Record.Location);
    Json["Rotation"] = VectorToJson(Record.Rotation);
    Json["Scale"] = VectorToJson(Record.Scale);
    return Json;
}

bool FJsonConverter::FromJson(const Json& Json, FPrimitiveRecord& OutRecord)
{
    if (!Json.is_object())
    {
        return false;
    }

    if (!Json.contains("Type") || !Json["Type"].is_string())
    {
        return false;
    }

    if (!Json.contains("Location") || !Json.contains("Rotation") || !Json.contains("Scale"))
    {
        return false;
    }

    OutRecord.Type = Json["Type"].get<FString>();

    if (!JsonToVector(Json["Location"], OutRecord.Location))
    {
        return false;
    }

    if (!JsonToVector(Json["Rotation"], OutRecord.Rotation))
    {
        return false;
    }

    if (!JsonToVector(Json["Scale"], OutRecord.Scale))
    {
        return false;
    }

    return true;
}

FString FJsonConverter::ToString(const FPrimitiveRecord& Record, int32 Indent)
{
    return ToJson(Record).dump(Indent);
}

bool FJsonConverter::FromString(const FString& JsonString, FPrimitiveRecord& OutRecord)
{
    try
    {
        Json Json = Json::parse(JsonString);
        return FromJson(Json, OutRecord);
    }
    catch (const Json::exception&)
    {
        return false;
    }
}

FJsonConverter::Json FJsonConverter::VectorToJson(const FVector& Value)
{
    return Json::array({ Value.X, Value.Y, Value.Z });
}

bool FJsonConverter::JsonToVector(const Json& Json, FVector& OutValue)
{
    if (!Json.is_array() || Json.size() != 3)
    {
        return false;
    }

    if (!Json[0].is_number() || !Json[1].is_number() || !Json[2].is_number())
    {
        return false;
    }

    OutValue.X = Json[0].get<float>();
    OutValue.Y = Json[1].get<float>();
    OutValue.Z = Json[2].get<float>();

    return true;
}
