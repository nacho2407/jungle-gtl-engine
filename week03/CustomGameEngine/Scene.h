#pragma once
#include <string>
#include "UEngineTypes.h"
#include "USceneComponent.h"

class FScene
{
public:
	int Version;
	int NextUUID;
	TArray<USceneComponent> Primitives;
	
	std::string ToJson();
	static std::string ToJson(FScene);
	static FScene FromJson(std::string);
};