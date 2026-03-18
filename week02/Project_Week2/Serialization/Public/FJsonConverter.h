#pragma once

#include "Json/json.hpp"
#include "FWorldSaveData.h"

class FJsonConverter
{
public:
    using Json = nlohmann::ordered_json;

public:
    static Json ToJson(const FWorldSaveData& WorldSaveData);
    static bool FromJson(const Json& InJson, FWorldSaveData& OutWorldSaveData);

    static FString ToString(const FWorldSaveData& WorldSaveData, int32 Indent = 4);
    static bool FromString(const FString& JsonString, FWorldSaveData& OutWorldSaveData);
    
    static bool SaveToFile(const FString& FilePath, const FWorldSaveData& WorldSaveData, int32 Indent = 4);
    static bool LoadFromFile(const FString& FilePath, FWorldSaveData& OutWorldSaveData);

    static Json ToJson(const FPrimitiveRecord& Record);
    static bool FromJson(const Json& InJson, FPrimitiveRecord& OutRecord);

private:
    static Json VectorToJson(const FVector& Value);
    static bool JsonToVector(const Json& InJson, FVector& OutValue);
};
