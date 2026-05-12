#include "Editor/UI/EditorMaterialWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/EditorRenderPipeline.h"

#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Asset/StaticMesh.h"
#include "Core/ResourceManager.h"
#include "Object/ObjectIterator.h"
#include "Math/Utils.h"
#include <algorithm>
#include <filesystem>

#include "ImGui/imgui.h"

namespace
{
ImU32 ToColorU32(const FVector& Color, float Alpha = 1.0f)
{
	return ImGui::GetColorU32(ImVec4(
		MathUtil::Clamp(Color.X, 0.0f, 1.0f),
		MathUtil::Clamp(Color.Y, 0.0f, 1.0f),
		MathUtil::Clamp(Color.Z, 0.0f, 1.0f),
		MathUtil::Clamp(Alpha, 0.0f, 1.0f)));
}

const UMaterial* ResolveBaseMaterial(UMaterialInterface* Material)
{
	if (const UMaterial* BaseMaterial = Cast<UMaterial>(Material))
	{
		return BaseMaterial;
	}
	if (const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material))
	{
		return Instance->Parent;
	}
	return nullptr;
}

const char* GetMaterialParamTypeName(EMaterialParamType Type)
{
	switch (Type)
	{
	case EMaterialParamType::Bool: return "Bool";
	case EMaterialParamType::Int: return "Int";
	case EMaterialParamType::UInt: return "UInt";
	case EMaterialParamType::Float: return "Float";
	case EMaterialParamType::Vector2: return "Vector2";
	case EMaterialParamType::Vector3: return "Vector3";
	case EMaterialParamType::Vector4: return "Vector4";
	case EMaterialParamType::Matrix4: return "Matrix4";
	case EMaterialParamType::Texture: return "Texture";
	default: return "Unknown";
	}
}

FString SanitizeAssetStem(FString Name)
{
	for (char& Ch : Name)
	{
		if (Ch == '\\' || Ch == '/' || Ch == ':' || Ch == '*' || Ch == '?' || Ch == '"' || Ch == '<' || Ch == '>' || Ch == '|')
		{
			Ch = '_';
		}
	}
	return Name.empty() ? "Material" : Name;
}

std::filesystem::path ResolveMaterialInstanceDirectory(const UMaterial* BaseMaterial)
{
	std::filesystem::path MatPath = std::filesystem::path(BaseMaterial ? FPaths::ToWide(BaseMaterial->GetFilePath()) : L"");
	if (!MatPath.empty() && MatPath.has_parent_path() && MatPath.extension() != L".mtl")
	{
		return MatPath.parent_path();
	}
	return std::filesystem::path("Asset") / "Material" / "Instances";
}

FString ResolveMaterialInstanceStem(const UMaterial* BaseMaterial)
{
	if (!BaseMaterial)
	{
		return "Material";
	}

	std::filesystem::path MatPath = std::filesystem::path(FPaths::ToWide(BaseMaterial->GetFilePath()));
	if (!MatPath.empty() && MatPath.has_stem() && MatPath.extension() != L".mtl")
	{
		return SanitizeAssetStem(FPaths::ToString(MatPath.stem().wstring()));
	}

	return SanitizeAssetStem(BaseMaterial->GetName());
}
}

#define MAT_SEPARATOR() ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

void FEditorMaterialWidget::ResetSelection()
{
	SelectedMaterialPtr = nullptr;
	EditingSlotOwner = nullptr;
	EditingSlotIndex = -1;
	AssetEditingMaterialPtr = nullptr;
}

void FEditorMaterialWidget::OpenMaterialAsset(UMaterialInterface* Material)
{
	if (!Material)
	{
		return;
	}

	AssetEditingMaterialPtr = Material;
	EditingSlotOwner = nullptr;
	EditingSlotIndex = -1;
	SelectedMaterialPtr = Material;
	bFocusWindowNextFrame = true;
}

void FEditorMaterialWidget::OpenMaterialSlot(UPrimitiveComponent* PrimitiveComp, int32 SlotIndex)
{
	if (!PrimitiveComp || SlotIndex < 0 || SlotIndex >= PrimitiveComp->GetNumMaterials())
	{
		return;
	}

	AssetEditingMaterialPtr = nullptr;
	EditingSlotOwner = PrimitiveComp;
	EditingSlotIndex = SlotIndex;
	SelectedMaterialPtr = PrimitiveComp->GetMaterial(SlotIndex);
	bFocusWindowNextFrame = true;
}

void FEditorMaterialWidget::OnActorDestroyed(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	if (EditingSlotOwner && EditingSlotOwner->GetOwner() == Actor)
	{
		ResetSelection();
	}
}

void FEditorMaterialWidget::Render(float DeltaTime)
{
	ImGui::SetNextWindowSize(ImVec2(500.0f, 400.0f), ImGuiCond_Once);
	if (bFocusWindowNextFrame)
	{
		ImGui::SetNextWindowFocus();
		bFocusWindowNextFrame = false;
	}
    ImGui::Begin("Material Editor");

	if (EditingSlotOwner && EditingSlotIndex >= 0)
	{
		RefreshEditingMaterialFromSlot();
		RenderSingleMaterialEditor(EditingSlotOwner);
		ImGui::End();
		return;
	}

	if (AssetEditingMaterialPtr)
	{
		RenderAssetMaterialEditor();
		ImGui::End();
		return;
	}

	ImGui::TextDisabled("Open a material asset, or press Edit on a StaticMesh material slot.");
	
	ImGui::End();
}

void FEditorMaterialWidget::RenderAssetMaterialEditor()
{
	SelectedMaterialPtr = AssetEditingMaterialPtr;
	if (!SelectedMaterialPtr)
	{
		ImGui::TextDisabled("No material asset selected.");
		return;
	}

	ImGui::TextDisabled("Asset Material");
	ImGui::TextWrapped("%s", SelectedMaterialPtr->GetName().c_str());

	MAT_SEPARATOR();
	RenderMaterialPreviewSummary();
	RenderMaterialPreview(nullptr);
	MAT_SEPARATOR();
	RenderMaterialDetails(nullptr);
}

void FEditorMaterialWidget::RenderSingleMaterialEditor(UPrimitiveComponent* SlotOwnerComp)
{
	if (!SlotOwnerComp || EditingSlotIndex < 0 || EditingSlotIndex >= SlotOwnerComp->GetNumMaterials())
	{
		ImGui::TextDisabled("The edited material slot is no longer valid.");
		return;
	}

	ImGui::TextDisabled("StaticMesh Material Slot");
	ImGui::Text("Slot [%d]", EditingSlotIndex);
	ImGui::TextWrapped("%s", SelectedMaterialPtr ? SelectedMaterialPtr->GetName().c_str() : "(None)");

	MAT_SEPARATOR();
	RenderMaterialPreviewSummary();
	RenderMaterialPreview(SlotOwnerComp);
	MAT_SEPARATOR();
	RenderMaterialDetails(SlotOwnerComp);
}

void FEditorMaterialWidget::RenderMaterialDetails(UPrimitiveComponent* SlotOwnerComp)
{
	if (!SelectedMaterialPtr)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No material assigned. Assign one from the component Material Slot.");
		return;
	}

	if (ImGui::Button("Create Instance"))
	{
		CreateInstanceForCurrentMaterial();
	}

	RenderMaterialProperties();
}

void FEditorMaterialWidget::RefreshEditingMaterialFromSlot()
{
	if (!EditingSlotOwner || EditingSlotIndex < 0 || EditingSlotIndex >= EditingSlotOwner->GetNumMaterials())
	{
		SelectedMaterialPtr = nullptr;
		return;
	}

	SelectedMaterialPtr = EditingSlotOwner->GetMaterial(EditingSlotIndex);
}

bool FEditorMaterialWidget::CreateInstanceForCurrentMaterial()
{
	UMaterial* BaseMat = Cast<UMaterial>(SelectedMaterialPtr);
	if (!BaseMat)
	{
		if (SelectedMaterialPtr && SelectedMaterialPtr->IsA<UMaterialInstance>())
		{
			EditorEngine->GetNotificationService().Info("Material is already an instance");
		}
		return false;
	}

	const std::filesystem::path InstanceDir = ResolveMaterialInstanceDirectory(BaseMat);
	std::error_code Ec;
	std::filesystem::create_directories(InstanceDir, Ec);

	const FString PureName = ResolveMaterialInstanceStem(BaseMat);
	int32 Index = 0;
	std::filesystem::path FinalPath;
	do
	{
		const FString NewName = PureName + "_Inst_" + std::to_string(Index) + ".matinst";
		FinalPath = InstanceDir / NewName;
		Index++;
	} while (std::filesystem::exists(FinalPath));

	const FString InstancePath = FPaths::Normalize(FPaths::ToString(FinalPath.generic_wstring()));
	UMaterialInstance* NewInstance = EditorEngine->GetAssetService().CreateMaterialInstance(InstancePath, BaseMat);
	if (!NewInstance)
	{
		return false;
	}

	if (!EditorEngine->GetAssetService().SaveMaterialInstance(InstancePath, NewInstance))
	{
		return false;
	}

	SelectedMaterialPtr = NewInstance;
	if (EditingSlotOwner && EditingSlotIndex >= 0 && EditingSlotIndex < EditingSlotOwner->GetNumMaterials())
	{
		EditingSlotOwner->SetMaterial(EditingSlotIndex, NewInstance);
		EditorEngine->GetSceneService().MarkDirty();
	}
	else
	{
		AssetEditingMaterialPtr = NewInstance;
	}

	EditorEngine->GetNotificationService().Info("Material instance created");
	return true;
}

UStaticMesh* FEditorMaterialWidget::ResolvePreviewMesh(UPrimitiveComponent* PrimitiveComp)
{
	if (PreviewMesh == nullptr)
	{
		PreviewMesh = FResourceManager::Get().LoadStaticMesh("Asset\\Mesh\\PreviewSphere.obj");
	}

	if (PreviewMesh && PreviewMesh->HasValidMeshData())
	{
		return PreviewMesh;
	}

	UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(PrimitiveComp);
	UStaticMesh* SelectedMesh = MeshComp ? MeshComp->GetStaticMesh() : nullptr;
	return (SelectedMesh && SelectedMesh->HasValidMeshData()) ? SelectedMesh : nullptr;
}

void FEditorMaterialWidget::RenderMaterialPreviewSummary()
{
	if (!SelectedMaterialPtr)
	{
		ImGui::TextDisabled("No material preview.");
		return;
	}

	const bool bInstance = SelectedMaterialPtr->IsA<UMaterialInstance>();
	ImGui::TextDisabled(bInstance ? "Editable Material Instance" : "Read-only Material");
	if (!SelectedMaterialPtr->GetFilePath().empty())
	{
		ImGui::TextWrapped("%s", FPaths::Normalize(SelectedMaterialPtr->GetFilePath()).c_str());
	}

	const UMaterial* BaseMaterial = ResolveBaseMaterial(SelectedMaterialPtr);
	if (!BaseMaterial)
	{
		return;
	}

	const FMaterial& MaterialData = BaseMaterial->MaterialData;
	ImGui::Spacing();
	ImGui::TextDisabled("Material Colors");
	ImGui::ColorButton("Diffuse##MaterialSummary", ImVec4(MaterialData.DiffuseColor.X, MaterialData.DiffuseColor.Y, MaterialData.DiffuseColor.Z, MaterialData.Opacity), ImGuiColorEditFlags_NoTooltip, ImVec2(42.0f, 22.0f));
	ImGui::SameLine();
	ImGui::TextUnformatted("Diffuse");
	ImGui::ColorButton("Specular##MaterialSummary", ImVec4(MaterialData.SpecularColor.X, MaterialData.SpecularColor.Y, MaterialData.SpecularColor.Z, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(42.0f, 22.0f));
	ImGui::SameLine();
	ImGui::TextUnformatted("Specular");
	ImGui::ColorButton("Emissive##MaterialSummary", ImVec4(MaterialData.EmissiveColor.X, MaterialData.EmissiveColor.Y, MaterialData.EmissiveColor.Z, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(42.0f, 22.0f));
	ImGui::SameLine();
	ImGui::TextUnformatted("Emissive");
}

void FEditorMaterialWidget::RenderMaterialPreview(UPrimitiveComponent* PrimitiveComp)
{
	if (!SelectedMaterialPtr)
	{
		return;
	}

	const bool bInstance = SelectedMaterialPtr->IsA<UMaterialInstance>();
	ImGui::TextDisabled(bInstance ? "Editable Material Instance" : "Read-only Material");
	const UMaterial* BaseMaterial = ResolveBaseMaterial(SelectedMaterialPtr);

	const float PreviewWidth = std::max(180.0f, ImGui::GetContentRegionAvail().x);
	const ImVec2 PreviewSize(PreviewWidth, 180.0f);
	UStaticMesh* Mesh = ResolvePreviewMesh(PrimitiveComp);
	ID3D11ShaderResourceView* PreviewSRV = nullptr;

	if (FEditorRenderPipeline* RenderPipeline = EditorEngine->GetEditorRenderPipeline())
	{
		PreviewSRV = RenderPipeline->RenderMaterialPreview(
			EditorEngine->GetRenderer(),
			Mesh,
			SelectedMaterialPtr,
			static_cast<uint32>(PreviewSize.x),
			static_cast<uint32>(PreviewSize.y),
			PreviewYawRad,
			PreviewPitchRad,
			PreviewDistance);
	}

	const ImVec2 PreviewMin = ImGui::GetCursorScreenPos();
	if (PreviewSRV)
	{
		ImGui::Image(reinterpret_cast<ImTextureID>(PreviewSRV), PreviewSize);
	}
	else
	{
		const ImVec2 PreviewMax(PreviewMin.x + PreviewSize.x, PreviewMin.y + PreviewSize.y);
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImU32 PreviewBg = ImGui::GetColorU32(ImVec4(0.09f, 0.10f, 0.12f, 1.0f));
		DrawList->AddRectFilled(PreviewMin, PreviewMax, PreviewBg, 6.0f);
		DrawList->AddRect(PreviewMin, PreviewMax, ImGui::GetColorU32(ImVec4(0.25f, 0.28f, 0.34f, 1.0f)), 6.0f);
		DrawList->AddText(ImVec2(PreviewMin.x + 12.0f, PreviewMin.y + 12.0f),
			ImGui::GetColorU32(ImGuiCol_TextDisabled), "3D preview unavailable.");
		ImGui::Dummy(PreviewSize);
	}

	if (ImGui::IsItemHovered())
	{
		const ImGuiIO& IO = ImGui::GetIO();
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			PreviewYawRad += IO.MouseDelta.x * 0.01f;
			PreviewPitchRad = MathUtil::Clamp(PreviewPitchRad + IO.MouseDelta.y * 0.01f, -1.35f, 1.35f);
		}
		if (IO.MouseWheel != 0.0f)
		{
			PreviewDistance = MathUtil::Clamp(PreviewDistance - IO.MouseWheel * 0.35f, 1.5f, 10.0f);
		}
	}

	const ImVec2 PreviewMax(PreviewMin.x + PreviewSize.x, PreviewMin.y + PreviewSize.y);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRect(PreviewMin, PreviewMax, ImGui::GetColorU32(ImVec4(0.25f, 0.28f, 0.34f, 1.0f)), 6.0f);

	if (BaseMaterial)
	{
		const FMaterial& MaterialData = BaseMaterial->MaterialData;
		const ImVec2 SwatchSize(34.0f, 18.0f);
		const ImVec2 SwatchMin(PreviewMin.x + 8.0f, PreviewMax.y - 28.0f);
		DrawList->AddRectFilled(SwatchMin, ImVec2(SwatchMin.x + SwatchSize.x, SwatchMin.y + SwatchSize.y),
			ToColorU32(MaterialData.DiffuseColor, MaterialData.Opacity), 3.0f);
		const ImVec2 SpecMin(SwatchMin.x + 42.0f, SwatchMin.y);
		DrawList->AddRectFilled(SpecMin, ImVec2(SpecMin.x + SwatchSize.x, SpecMin.y + SwatchSize.y),
			ToColorU32(MaterialData.SpecularColor), 3.0f);
		const ImVec2 EmissiveMin(SpecMin.x + 42.0f, SpecMin.y);
		DrawList->AddRectFilled(EmissiveMin, ImVec2(EmissiveMin.x + SwatchSize.x, EmissiveMin.y + SwatchSize.y),
			ToColorU32(MaterialData.EmissiveColor), 3.0f);
	}

	ImGui::TextDisabled("Drag preview to rotate the material sphere. Wheel zooms.");
}

void FEditorMaterialWidget::RenderMaterialProperties()
{
	TMap<FString, FMaterialParamValue> DisplayParams;

	SelectedMaterialPtr->GatherAllParams(DisplayParams);
	bool bIsInstanced = SelectedMaterialPtr->IsA<UMaterialInstance>();

	if (!bIsInstanced)
	{
		ImGui::TextDisabled("Create Instance to edit material parameters.");
		ImGui::BeginDisabled();
	}

	auto SaveSelectedMaterialInstance = [this]()
	{
		UMaterialInstance* Instance = Cast<UMaterialInstance>(SelectedMaterialPtr);
		if (EditorEngine && Instance)
		{
			EditorEngine->GetAssetService().SaveMaterialInstance(SelectedMaterialPtr->GetFilePath(), Instance);
		}
	};

	for (auto& [ParamName, ParamValue] : DisplayParams)
	{
		switch (ParamValue.Type)
		{
		case EMaterialParamType::Bool:
			if (ImGui::Checkbox(ParamName.c_str(), &std::get<bool>(ParamValue.Value)))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				SaveSelectedMaterialInstance();
			}
			break;
		case EMaterialParamType::Int:
			if (ImGui::DragInt(ParamName.c_str(), &std::get<int32>(ParamValue.Value)))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				SaveSelectedMaterialInstance();
			}
			break;
		case EMaterialParamType::UInt:
			if (ImGui::DragInt(ParamName.c_str(), reinterpret_cast<int32*>(&std::get<uint32>(ParamValue.Value))))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				SaveSelectedMaterialInstance();
			}
			break;
		case EMaterialParamType::Float:
			if (ImGui::DragFloat(ParamName.c_str(), &std::get<float>(ParamValue.Value), 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				SaveSelectedMaterialInstance();
			}
			break;
		case EMaterialParamType::Vector2:
			if (ImGui::DragFloat2(ParamName.c_str(), &std::get<FVector2>(ParamValue.Value).X, 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				SaveSelectedMaterialInstance();
			}
			break;
		case EMaterialParamType::Vector3:
			if (ImGui::DragFloat3(ParamName.c_str(), &std::get<FVector>(ParamValue.Value).X, 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				SaveSelectedMaterialInstance();
			}
			break;
		case EMaterialParamType::Vector4:
			if (ImGui::DragFloat4(ParamName.c_str(), &std::get<FVector4>(ParamValue.Value).X, 0.01f))
			{
				SelectedMaterialPtr->SetParam(ParamName, ParamValue);
				SaveSelectedMaterialInstance();
			}
			break;
		case EMaterialParamType::Texture:
		{
			UTexture* CurrentTex = std::get<UTexture*>(ParamValue.Value);
			ID3D11ShaderResourceView* SRV = CurrentTex ? CurrentTex->GetSRV() : nullptr;

			if (ImGui::ImageButton(ParamName.c_str(), (void*)SRV, ImVec2(64, 64)))
			{
			}
			ImGui::SameLine();
			
			ImGui::BeginGroup();
			ImGui::Text("%s", ParamName.c_str());
			
			FString CurrentPath = CurrentTex ? CurrentTex->GetName() : "None";

			ImGui::SetNextItemWidth(200.0f);
			FString ComboId = "##Combo_" + ParamName;
			if (ImGui::BeginCombo(ComboId.c_str(), CurrentPath.c_str()))
			{
				for (TObjectIterator<UTexture> It; It; ++It)
				{
					UTexture* Texture = *It;
					if (Texture && !Texture->GetFilePath().empty())
					{
						const FString& TexPath = Texture->GetFilePath().empty() ? "None" : Texture->GetFilePath();
						bool bSelected = (TexPath == CurrentPath);
						if (ImGui::Selectable(TexPath.c_str(), bSelected))
						{
							ParamValue.Value = Texture;
							SelectedMaterialPtr->SetParam(ParamName, ParamValue);
							SaveSelectedMaterialInstance();
						}
						if (bSelected) ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::EndGroup();

			break;
		}
		}
	}

	if (!bIsInstanced)
	{
		ImGui::EndDisabled();
	}
}
