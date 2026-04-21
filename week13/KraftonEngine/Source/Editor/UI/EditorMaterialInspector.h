#pragma once
#include "imgui.h" 
#include "Platform/Paths.h"
#include "Core/CoreTypes.h"
#include <fstream>
#include <filesystem>
#include "SimpleJSON/json.hpp"
#include <wrl/client.h>
#include <Engine/Materials/MaterialManager.h>

struct ID3D11ShaderResourceView;

class FEditorMaterialInspector final
{
public:
	FEditorMaterialInspector() = default;
	FEditorMaterialInspector(std::filesystem::path InPath);
	void Render();

private:
	void RenderTextureSection();

private:
	std::filesystem::path MaterialPath;
	json::JSON CachedJson;
	UMaterial* CachedMaterial;

	TMap<FString, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> CachedSRVs;
};

