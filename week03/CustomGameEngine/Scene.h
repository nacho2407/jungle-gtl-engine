#pragma once
#include <string>
#include "json.hpp"

class FScene
{
public:
	int Version;
	int NextUUID;
	// TArray<UObject> Primitives;
	
	std::string ToJson();
	static std::string ToJson(FScene);
	static FScene FromJson(std::string);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FScene, Version, NextUUID)