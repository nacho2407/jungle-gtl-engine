#pragma once
#include <string>
#include "UEngineTypes.h"
#include "UObject.h"

class FScene
{
public:
	int Version;
	int NextUUID;
	TArray<UObject> Primitives;
	
	std::string ToJson();
	static std::string ToJson(FScene);
	static FScene FromJson(std::string);
};