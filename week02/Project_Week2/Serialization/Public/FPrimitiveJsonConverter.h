#pragma once

#include "Json/json.hpp"

#include "FWorldSaveData.h"

class FPrimitiveJsonConverter
{
public:
    using Json = nlohmann::json;

public:
    static Json ToJson(const FPrimitiveRecord& Record);
    static bool FromJson(const Json& Json, FPrimitiveRecord& OutRecord);

    static FString ToString(const FPrimitiveRecord& Record, int32 Indent = 4);
    static bool FromString(const FString& JsonString, FPrimitiveRecord& OutRecord);

private:
    static Json VectorToJson(const FVector& Value);
    static bool JsonToVector(const Json& Json, FVector& OutValue);
};