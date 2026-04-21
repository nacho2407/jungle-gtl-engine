#include "EditorMaterialInspector.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"

void FEditorMaterialInspector::Render()
{
	bool bIsValid = ImGui::Begin("MaterialInspector");
	bIsValid &= std::filesystem::exists(MaterialPath);
	bIsValid &= MaterialPath.extension() == ".mat";

	if (!bIsValid)
	{
		ImGui::End();
		return;
	}

	if (CachedJson.IsNull())
	{
		std::ifstream File(MaterialPath.c_str());

		std::stringstream Buffer;
		Buffer << File.rdbuf();
		CachedJson =  json::JSON::Load(Buffer.str());
	}


	json::JSON JsonData = CachedJson;

	TMap<const char*, FString> MatMap;

	MatMap[MatKeys::PathFileName] = JsonData.hasKey(MatKeys::PathFileName) ? JsonData[MatKeys::PathFileName].ToString().c_str() : "";
	ImGui::Selectable(MatMap[MatKeys::PathFileName].c_str());

	RenderTextureSection(JsonData);


	ImGui::End();
}

void FEditorMaterialInspector::RenderTextureSection(json::JSON JsonData)
{
	if (!JsonData.hasKey(MatKeys::Textures)) return;

	ImGui::Text("Textures");
	for (auto& Pair : JsonData[MatKeys::Textures].ObjectRange())
	{
		FString SlotName = Pair.first.c_str();
		FString TexturePath = Pair.second.ToString().c_str();

		if (!CachedSRVs.contains(TexturePath))
			CachedSRVs[TexturePath] = FResourceManager::Get().FindLoadedTexture(TexturePath);

		if (CachedSRVs[TexturePath])
			ImGui::Image(CachedSRVs[TexturePath].Get(), ImVec2(50, 50));
	}
}
