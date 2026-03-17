#pragma once

#include "World/UWorld.h"

#include "FWorldSaveData.h"

class FWorldSaveConverter
{
public:
    static FWorldSaveData FromWorld(const UWorld* World);
    static bool ToWorld(const FWorldSaveData& SaveData, UWorld* World);

private:
    static FPrimitiveRecord MakePrimitiveRecord(const AActor* Actor);
    static AActor* MakeActorFromRecord(const FPrimitiveRecord& Record);

    static bool IsSavablePrimitiveActor(const AActor* Actor);
    static void RemoveSavablePrimitiveActors(UWorld* World);

private:
    static const TMap<FString, FString> ClassNameMap;
};
