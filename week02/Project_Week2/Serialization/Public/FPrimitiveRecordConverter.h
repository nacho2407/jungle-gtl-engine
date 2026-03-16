#pragma once

#include "FPrimitiveRecord.h"
#include "Actor/AActor.h"
#include "Component/USceneComponent.h"

class FPrimitiveRecordConverter
{
public:
	static FPrimitiveRecord FromActor(const AActor* Actor);
	//static AActor* ToActor(const FPrimitiveRecord& Record);
};