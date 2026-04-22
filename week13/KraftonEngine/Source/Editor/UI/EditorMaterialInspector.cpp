#include "EditorMaterialInspector.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"
#include "Editor/UI/ContentBrowser/ContentItem.h"
#include "SimpleJSON/json.hpp"
#include "Engine/Materials/Material.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Texture/Texture2D.h"

FEditorMaterialInspector::FEditorMaterialInspector(std::filesystem::path InPath)
{
	MaterialPath = InPath;
	CachedMaterial = FMaterialManager::Get().GetOrCreateMaterial(
		FPaths::ToUtf8(InPath.lexically_relative(FPaths::RootDir()).generic_wstring())
	);
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
		std::ifstream File(MaterialPath);

		std::stringstream Buffer;
		Buffer << File.rdbuf();
		CachedJson = json::JSON::Load(Buffer.str());
	}


	json::JSON JsonData = CachedJson;

	TMap<const char*, FString> MatMap;
	MatMap[MatKeys::PathFileName] = JsonData.hasKey(MatKeys::PathFileName) ? JsonData[MatKeys::PathFileName].ToString().c_str() : "";
	ImGui::Selectable(MatMap[MatKeys::PathFileName].c_str());

	RenderShaderParameter();
	RenderTextureSection();

	ImGui::End();
}

void FEditorMaterialInspector::RenderShaderParameter()
{
	const auto& Layout = CachedMaterial->GetParameterInfo();

	for (const auto& [ParamName, Info] : Layout)
	{
		ImGui::Text(ParamName.c_str());
		
		switch (Info->Size)
		{
			case sizeof(float) : // 4바이트 - Scalar
			{
				float Param;
				bool bIsValid = CachedMaterial->GetScalarParameter(ParamName, Param);
				ImGui::DragFloat("##floatParam", &Param);
				CachedMaterial->SetScalarParameter(ParamName, Param);
				break;
			}
			case sizeof(float) * 3: // 12바이트 - Vector3
			{
				FVector Param;
				bool bIsValid = CachedMaterial->GetVector3Parameter(ParamName, Param);
				ImGui::DragFloat3("##float3Param", &Param.X);
				CachedMaterial->SetVector3Parameter(ParamName, Param);
				break;
			}
			case sizeof(float) * 4: // 16바이트 - Vector4
			{
				FVector4 Param;
				bool bIsValid = CachedMaterial->GetVector4Parameter(ParamName, Param);
				ImGui::DragFloat4("##float4Param", &Param.X);
				CachedMaterial->SetVector4Parameter(ParamName, Param);
				break;
			}
			case sizeof(float) * 16: // 64바이트 - Matrix
			{
				FMatrix Param;
				bool bIsValid = CachedMaterial->GetMatrixParameter(ParamName, Param);
				ImGui::DragFloat4("##matrix1Param", Param.Data);
				ImGui::DragFloat4("##matrix2Param", Param.Data + 4);
				ImGui::DragFloat4("##matrix3Param", Param.Data + 4);
				ImGui::DragFloat4("##matrix4Param", Param.Data + 4);
				CachedMaterial->SetMatrixParameter(ParamName, Param);
				break;
			}
			default:
				break; // uint, bool 등 특수 케이스는 별도 처리 필요
		}
	}

}

void FEditorMaterialInspector::RenderTextureSection()
{
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
				FString NewTexturePath = FPaths::ToUtf8(
					ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring()
				);
				ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
				UTexture2D* NewTexture = UTexture2D::LoadFromFile(NewTexturePath, Device);
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
