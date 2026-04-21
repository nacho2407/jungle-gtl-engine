#include "EditorMaterialInspector.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"
#include "Editor/UI/ContentBrowser/ContentItem.h"
#include "SimpleJSON/json.hpp"
#include "Engine/Materials/Material.h"
#include "Engine/Texture/Texture2D.h"

FEditorMaterialInspector::FEditorMaterialInspector(std::filesystem::path InPath)
{
	MaterialPath = InPath;
	CachedMaterial = FMaterialManager::Get().GetOrCreateMaterial(FPaths::ToUtf8(InPath.lexically_relative(FPaths::RootDir())));
}

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
		CachedJson = json::JSON::Load(Buffer.str());
	}


	json::JSON JsonData = CachedJson;

	TMap<const char*, FString> MatMap;

	MatMap[MatKeys::PathFileName] = JsonData.hasKey(MatKeys::PathFileName) ? JsonData[MatKeys::PathFileName].ToString().c_str() : "";
	ImGui::Selectable(MatMap[MatKeys::PathFileName].c_str());

	RenderTextureSection();

	if (CachedMaterial)
	{
		ImGui::Text("NiceNiceNiceNiceNiceNiceNiceNiceNiceNice");
		ImGui::Text(CachedMaterial->GetAssetPathFileName().c_str());
	}

	ImGui::End();
}

void FEditorMaterialInspector::RenderTextureSection()
{
	/*if (!CachedJson.hasKey(MatKeys::Textures))
		return;

	ImGui::Text("Textures");
	for (auto& Pair : CachedJson[MatKeys::Textures].ObjectRange())
	{
		FString SlotName = Pair.first.c_str();
		FString TexturePath = Pair.second.ToString().c_str();

		if (!CachedSRVs.contains(TexturePath))
			CachedSRVs[TexturePath] = FResourceManager::Get().FindLoadedTexture(TexturePath);

		if (CachedSRVs[TexturePath])
		{
			ImGui::Text(SlotName.c_str());
			ImGui::Image(CachedSRVs[TexturePath].Get(), ImVec2(100, 100));
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PNGElement"))
				{
					FContentItem ContentItem = *reinterpret_cast<const FContentItem*>(payload->Data);
					FString NewTexturePath = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()));
					Pair.second = NewTexturePath.c_str();

					std::ofstream File(MaterialPath);
					File << CachedJson.dump();
					CachedJson = json::JSON();
					break;
				}
				ImGui::EndDragDropTarget();
			}
		}
	}*/


	ImGui::Text("NiceNiceNiceNiceNiceNiceNiceNiceNiceNiceTextures");
	TMap<FString, UTexture2D*>* Textures = CachedMaterial->GetTexture();

	for (auto& Pair : *Textures)
	{
		FString SlotName = Pair.first.c_str();
		UTexture2D* Texture = Pair.second;

		if (!Texture)
			continue;


		ImGui::Text(SlotName.c_str());
		ImGui::Image(Texture->GetSRV(), ImVec2(100, 100));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PNGElement"))
			{
				FContentItem ContentItem = *reinterpret_cast<const FContentItem*>(payload->Data);
				FString NewTexturePath = FPaths::ToUtf8(ContentItem.Path);

				UTexture2D* NewTexture = UTexture2D::LoadFromCached(NewTexturePath);
				if (NewTexture)
				{
					CachedMaterial->SetTextureParameter(SlotName, NewTexture);
					CachedMaterial->RebuildCachedSRVs();
				}

			}
			ImGui::EndDragDropTarget();

		}
	}
}
