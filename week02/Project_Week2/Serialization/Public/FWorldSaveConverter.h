#pragma once

#include "World/UWorld.h"

#include "FWorldSaveData.h"

class FWorldSaveConverter
{
public:
    static FWorldSaveData FromWorld(const UWorld* World);
    static bool ToWorld(const FWorldSaveData& SaveData, UWorld* World);

private:
    static FPrimitiveRecord MakePrimitiveRecord(const AActor* Actor, uint32 SaveID);
    static AActor* MakeActorFromRecord(const FPrimitiveRecord& Record);
    static FString FindSavedTypeByClassName(const FString& ClassName);

public:
    static const TMap<FString, FString> ClassNameMap;
};
