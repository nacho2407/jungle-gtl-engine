#pragma once
#include "imgui.h" 
#include "Platform/Paths.h"
#include "Core/CoreTypes.h"
#include <fstream>
#include <filesystem>
#include "SimpleJSON/json.hpp"
#include <wrl/client.h>

struct ID3D11ShaderResourceView;

class FEditorMaterialInspector final
{
public:
	FEditorMaterialInspector() = default;
	FEditorMaterialInspector(std::filesystem::path InPath) { MaterialPath = InPath; }
	void Render();

private:
	void RenderTextureSection(json::JSON JsonData);

private:
	std::filesystem::path MaterialPath;
	json::JSON CachedJson;

	TMap<FString, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> CachedSRVs;
};

