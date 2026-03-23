#pragma once

#include "Core/CoreMinimal.h"

#include <variant>

struct ENGINE_API FSceneJsonValue
{
    using Array = TArray<FSceneJsonValue>;
    using Object = TMap<FString, FSceneJsonValue>;

    enum class EType
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    FSceneJsonValue() = default;
    FSceneJsonValue(std::nullptr_t);
    FSceneJsonValue(bool InValue);
    FSceneJsonValue(double InValue);
    FSceneJsonValue(const FString& InValue);
    FSceneJsonValue(FString&& InValue);
    FSceneJsonValue(const char* InValue);
    FSceneJsonValue(const Array& InValue);
    FSceneJsonValue(Array&& InValue);
    FSceneJsonValue(const Object& InValue);
    FSceneJsonValue(Object&& InValue);

    EType GetType() const;

    bool IsNull() const;
    bool IsBool() const;
    bool IsNumber() const;
    bool IsString() const;
    bool IsArray() const;
    bool IsObject() const;

    bool TryGetBool(bool& OutValue) const;
    bool TryGetNumber(double& OutValue) const;
    bool TryGetString(FString& OutValue) const;

    const Array* TryGetArray() const;
    Array* TryGetArray();
    const Object* TryGetObject() const;
    Object* TryGetObject();

    const FSceneJsonValue* FindField(const FString& Key) const;
    FSceneJsonValue* FindField(const FString& Key);

  private:
    using Storage =
        std::variant<std::monostate, bool, double, FString, Array, Object>;

    Storage Value;
};

class ENGINE_API FSceneJsonParser
{
  public:
    static bool Parse(const FString& Source, FSceneJsonValue& OutValue,
                      FString* OutErrorMessage = nullptr);
};

class ENGINE_API FSceneJsonWriter
{
  public:
    static FString Write(const FSceneJsonValue& Value, bool bPretty = true);
};
