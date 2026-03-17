#pragma once

#include "CoreTypes.h"
#include "FWorldSaveData.h"
#include "Actor/AActor.h"
#include "Component/USceneComponent.h"

class FPrimitiveRecordConverter
{
public:

	static FPrimitiveRecord FromActor(const AActor* Actor);
	static AActor* ToActor(const FPrimitiveRecord& Record);

private:

	static const TMap<FString, FString> ClassNameMap;
};