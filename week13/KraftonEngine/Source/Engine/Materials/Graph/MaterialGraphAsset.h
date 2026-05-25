#pragma once

#include "Materials/Graph/MaterialGraphTypes.h"
#include "SimpleJSON/json.hpp"

namespace MaterialGraphAsset
{
	bool LoadFromJson(const json::JSON& JsonData, FMaterialGraph& OutGraph);
	void SaveToJson(const FMaterialGraph& Graph, json::JSON& OutJson);

	json::JSON MakeDefaultMaterialJson(const FString& ProjectRelativePath, const FString& MaterialGuid);
	FString ComputeGraphHashString(const json::JSON& GraphJson);
}
