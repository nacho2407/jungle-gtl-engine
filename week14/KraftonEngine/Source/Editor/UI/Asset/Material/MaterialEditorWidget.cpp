#include "UI/Asset/Material/MaterialEditorWidget.h"

#include "Editor/EditorEngine.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/MeshManager.h"
#include "Texture/Texture2D.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "UI/ContentBrowser/ContentItem.h"
#include "Viewport/Viewport.h"

#include <ImGui/imgui.h>

static uint32 GNextMaterialEditorInstanceId = 0;

FMaterialEditorWidget::FMaterialEditorWidget()
	: InstanceId(GNextMaterialEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("MaterialEditorPreview_" + Id);
	WindowIdSuffix = "###MaterialEditor_" + Id;
}

bool FMaterialEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UMaterialInterface>();
}

bool FMaterialEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object))
	{
		return true;
	}

	const UMaterialInterface* CurrentMaterial = Cast<UMaterialInterface>(EditedObject);
	const UMaterialInterface* RequestedMaterial = Cast<UMaterialInterface>(Object);
	if (!IsOpen() || !CurrentMaterial || !RequestedMaterial)
	{
		return false;
	}

	const FString& CurrentPath = CurrentMaterial->GetAssetPathFileName();
	return !CurrentPath.empty()
		&& CurrentPath != "None"
		&& CurrentPath == RequestedMaterial->GetAssetPathFileName();
}

void FMaterialEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	UMaterialInterface* Material = Cast<UMaterialInterface>(EditedObject);
	if (!Material) return;

	MaterialPath = std::filesystem::path(FPaths::RootDir()) /
		std::filesystem::path(FPaths::ToWide(Material->GetAssetPathFileName()));

	CachedJson = json::JSON();

	std::ifstream File(MaterialPath);
	if (File.is_open())
	{
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		CachedJson = json::JSON::Load(Buffer.str());
	}

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	
	UStaticMeshComponent* Comp = Actor->AddComponent<UStaticMeshComponent>();
	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();

	UStaticMesh* SphereMesh = FMeshManager::LoadStaticMesh("Content/Data/BasicShape/Sphere.OBJ", Device);
	Comp->SetStaticMesh(SphereMesh);
	Comp->SetMaterial(0, Material);

	Actor->SetRootComponent(Comp);
	Actor->SetActorLocation(FVector::ZeroVector);

	PreviewMeshComponent = Comp;

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));

	UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>();
	LightComp->SetShadowBias(0.0f);
	LightComp->PushToScene();

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();

	ViewportClient.Initialize(Device, static_cast<uint32>(ViewportSize.x), static_cast<uint32>(ViewportSize.y));
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewMeshComponent(Comp);
	ViewportClient.ResetCameraToPreviewBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);
	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FMaterialEditorWidget::Close()
{
	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();
	PreviewMeshComponent = nullptr;

	FAssetEditorWidget::Close();
}

void FMaterialEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}
}

void FMaterialEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FStaticMeshEditorViewportClient*>(&ViewportClient));
	}
}

void FMaterialEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	static float DetailsWidth = 300.0f;
	UMaterialInterface* Material = Cast<UMaterialInterface>(EditedObject);

	bool bWindowOpen = true;
	FString VisibleTitle = "Material Editor";
	const FString AssetPath = Material ? Material->GetAssetPathFileName() : FString();
	if (!AssetPath.empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += AssetPath;
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	ImGui::SetNextWindowSize(ImVec2(1280.0f, 720.0f), ImGuiCond_Once);
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	ImGui::BeginGroup();
	{
		float AvailableWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x;
		ImVec2 Size = ImVec2(AvailableWidth, ImGui::GetContentRegionAvail().y);

		ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
		ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

		FViewport* VP = ViewportClient.GetViewport();
		if (VP && Size.x > 0 && Size.y > 0)
		{
			VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));

			if (VP->GetSRV())
			{
				ImGui::Image((ImTextureID)VP->GetSRV(), Size);
				FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
			}

			constexpr float ToolbarHeight = 28.0f;

			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			DrawList->AddRectFilled(ViewportPos,
				ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight),
				IM_COL32(40, 40, 40, 255));

			FViewportToolbarContext Context;
			Context.Renderer = &GEngine->GetRenderer();
			Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
			Context.RenderOptions = &ViewportClient.GetRenderOptions();
			Context.ToolbarLeft = ViewportPos.x;
			Context.ToolbarTop = ViewportPos.y;
			Context.ToolbarWidth = Size.x;
			Context.bReservePlayStopSpace = false;
			Context.bShowAddActor = false;
			Context.bShowGizmoControls = false;

			FViewportToolbar::Render(Context);
		}
	}
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("Details", ImVec2(DetailsWidth, 0), true);
	RenderDetailsPanel(Cast<UMaterialInterface>(EditedObject));
	ImGui::EndChild();

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FMaterialEditorWidget::RenderViewport()
{
	// Viewport rendering is handled by the viewport client and ImGui integration in Render().
}

void FMaterialEditorWidget::RenderDetailsPanel(UMaterialInterface* Material)
{
	if (!Material)
	{
		ImGui::Text("No material selected.");
		return;
	}
	
	if (UMaterial* BaseMaterial = Cast<UMaterial>(Material))
	{
		RenderMaterialSettings(BaseMaterial);

		if (ImGui::Button("Create Instance"))
		{
			CreateMaterialInstanceAsset(BaseMaterial);
		}
	}
	else if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Material))
	{
		ImGui::SeparatorText("Material Instance");

		UMaterial* Parent = Instance->GetParent();
		ImGui::Text("Parent: %s", Parent ? Parent->GetAssetPathFileName().c_str() : "None");

		if (Parent && ImGui::Button("Open Parent"))
		{
			if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
			{
				EditorEngine->OpenAssetEditorForObject(Parent);
			}
		}
	}

	ImGui::SeparatorText("Shader Parameters");
	RenderShaderParameters(Material);

	ImGui::SeparatorText("Textures");
	RenderTextureSection(Material);
}

void FMaterialEditorWidget::RenderMaterialSettings(UMaterial* Material)
{
	ImGui::SeparatorText("Material Settings");

	TArray<FPropertyValue> Props;
	Material->GetEditableProperties(Props);

	if (ImGui::BeginTable("##MaterialSettings", 2,
		ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 140.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		for (int32 Index = 0; Index < (int32)Props.size(); ++Index)
		{
			FPropertyValue& Prop = Props[Index];

			ImGui::PushID(Index);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(FEditorPropertyRenderer::GetPropertyDisplayName(Prop));

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);

			FEditorPropertyRenderOptions Options;
			const bool bChanged = PropertyRenderer.RenderPropertyWidget(Props, Index, Options);

			if (bChanged)
			{
				Material->PostEditProperty(Prop.GetName());
				SaveMaterialJson();

				if (PreviewMeshComponent)
				{
					PreviewMeshComponent->SetMaterial(0, Material);
				}
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}
}

void FMaterialEditorWidget::RenderShaderParameters(UMaterialInterface* Material)
{
	if (!Material) return;

	UMaterial* LayerOwner = Cast<UMaterial>(Material);
	if (!LayerOwner)
	{
		if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Material))
		{
			LayerOwner = Instance->GetParent();
		}
	}
	
	if (!LayerOwner)
	{
		ImGui::TextDisabled("No parameter layout.");
		return;
	}

	const auto& Layout = LayerOwner->GetParameterInfo();

	for (const auto& [ParamName, Info] : Layout)
	{
		if (!Info) continue;

		ImGui::PushID(ParamName.c_str());
		ImGui::TextUnformatted(ParamName.c_str());

		bool bChanged = false;

		switch (Info->Size)
		{
		case sizeof(float):
		{
			float Param = 0.0f;
			if (Material->GetScalarParameter(ParamName, Param))
			{
				bChanged = ImGui::DragFloat("##Value", &Param);
				if (bChanged)
				{
					Material->SetScalarParameter(ParamName, Param);
					CachedJson[MatKeys::Parameters][ParamName] = Param;
				}
			}
			break;
		}
		case sizeof(float) * 3:
		{
			FVector Param;
			if (Material->GetVector3Parameter(ParamName, Param))
			{
				bChanged = ImGui::DragFloat3("##Value", &Param.X);
				if (bChanged)
				{
					Material->SetVector3Parameter(ParamName, Param);
					CachedJson[MatKeys::Parameters][ParamName][0] = Param.X;
					CachedJson[MatKeys::Parameters][ParamName][1] = Param.Y;
					CachedJson[MatKeys::Parameters][ParamName][2] = Param.Z;
				}
			}
			break;
		}
		case sizeof(float) * 4:
		{
			FVector4 Param;
			if (Material->GetVector4Parameter(ParamName, Param))
			{
				bChanged = ImGui::DragFloat4("##Value", &Param.X);
				if (bChanged)
				{
					Material->SetVector4Parameter(ParamName, Param);
					CachedJson[MatKeys::Parameters][ParamName][0] = Param.X;
					CachedJson[MatKeys::Parameters][ParamName][1] = Param.Y;
					CachedJson[MatKeys::Parameters][ParamName][2] = Param.Z;
					CachedJson[MatKeys::Parameters][ParamName][3] = Param.W;
				}
			}
			break;
		}
		case sizeof(float) * 16:
		{
			FMatrix Param;
			if (Material->GetMatrixParameter(ParamName, Param))
			{
				bool bRowChanged = false;
				bRowChanged |= ImGui::DragFloat4("##row0", Param.Data + 0);
				bRowChanged |= ImGui::DragFloat4("##row1", Param.Data + 4);
				bRowChanged |= ImGui::DragFloat4("##row2", Param.Data + 8);
				bRowChanged |= ImGui::DragFloat4("##row3", Param.Data + 12);

				if (bRowChanged)
				{
					Material->SetMatrixParameter(ParamName, Param);
					bChanged = true;
					// JSON matrix 저장 포맷이 아직 없으면 일단 저장 생략해도 됩니다.
				}
			}
			break;
		}
		default:
			ImGui::TextDisabled("Unsupported parameter size: %u", Info->Size);
			break;
		}

		if (bChanged)
		{
			MarkDirty();
			SaveMaterialJson();

			if (PreviewMeshComponent)
			{
				PreviewMeshComponent->SetMaterial(0, Material);
			}
		}

		ImGui::PopID();
	}
}

void FMaterialEditorWidget::RenderTextureSection(UMaterialInterface* Material)
{
	if (!Material) return;

	UMaterial* LayoutOwner = Cast<UMaterial>(Material);
	if (!LayoutOwner)
	{
		if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Material))
		{
			LayoutOwner = Instance->GetParent();
		}
	}

	if (!LayoutOwner)
	{
		ImGui::TextDisabled("No texture layout.");
		return;
	}

	TMap<FString, UTexture2D*>* Textures = LayoutOwner->GetTexture();

	for (auto& Pair : *Textures)
	{
		FString SlotName = Pair.first.c_str();

		UTexture2D* Texture = Pair.second;
		Material->GetTextureParameter(SlotName, Texture);

		ImGui::PushID(SlotName.c_str());
		ImGui::TextUnformatted(SlotName.c_str());

		if (Texture && Texture->GetSRV())
		{
			ImGui::Image(Texture->GetSRV(), ImVec2(100, 100));
		}
		else
		{
			ImGui::Button("None", ImVec2(100, 100));
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PNGElement"))
			{
				FContentItem ContentItem = *reinterpret_cast<const FContentItem*>(payload->Data);

				FString NewTexturePath = FPaths::ToUtf8(
					ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring()
				);

				ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;

				const bool bIsColorTexture =
					SlotName == "DiffuseTexture" ||
					SlotName == "EmissiveTexture" ||
					SlotName == "Custom0Texture" ||
					SlotName == "Custom1Texture";

				UTexture2D* NewTexture = UTexture2D::LoadFromFile(
					NewTexturePath,
					Device,
					bIsColorTexture ? ETextureColorSpace::SRGB : ETextureColorSpace::Linear);

				if (NewTexture && Material->SetTextureParameter(SlotName, NewTexture))
				{
					if (UMaterial* BaseMaterial = Cast<UMaterial>(Material))
					{
						BaseMaterial->RebuildCachedSRVs();
					}

					CachedJson[MatKeys::Textures][SlotName.c_str()] = NewTexturePath.c_str();

					MarkDirty();
					SaveMaterialJson();

					if (PreviewMeshComponent)
					{
						PreviewMeshComponent->SetMaterial(0, Material);
					}
				}
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();

		if (ImGui::Button("Clear"))
		{
			if (Material->SetTextureParameter(SlotName, nullptr))
			{
				if (UMaterial* BaseMaterial = Cast<UMaterial>(Material))
				{
					BaseMaterial->RebuildCachedSRVs();
				}

				CachedJson[MatKeys::Textures][SlotName.c_str()] = "";

				MarkDirty();
				SaveMaterialJson();

				if (PreviewMeshComponent)
				{
					PreviewMeshComponent->SetMaterial(0, Material);
				}
			}
		}

		ImGui::PopID();
	}
}

void FMaterialEditorWidget::SaveMaterialJson()
{
	UMaterialInterface* Material = Cast<UMaterialInterface>(EditedObject);
	if (!Material || MaterialPath.empty()) return;

	if (CachedJson.IsNull())
	{
		std::ifstream InFile(MaterialPath);
		if (InFile.is_open())
		{
			std::stringstream Buffer;
			Buffer << InFile.rdbuf();
			CachedJson = json::JSON::Load(Buffer.str());
		}
	}

	if (CachedJson.IsNull())
	{
		CachedJson = json::JSON();
	}

	if (UMaterial* BaseMaterial = Cast<UMaterial>(Material))
	{
		const FEnum* ShadowEnum = FEnum::FindEnumByName("EMaterialShadowMode");
		if (ShadowEnum && ShadowEnum->GetNames())
		{
			const int32 Index = static_cast<int32>(Material->GetShadowMode());
			if (Index >= 0 && static_cast<uint32>(Index) < ShadowEnum->GetCount())
			{
				CachedJson[MatKeys::ShadowMode] = ShadowEnum->GetNames()[Index];
			}
		}
	}

	std::ofstream File(MaterialPath);
	if (!File.is_open())
	{
		return;
	}

	File << CachedJson.dump(4);
}

void FMaterialEditorWidget::CreateMaterialInstanceAsset(UMaterial* ParentMaterial)
{
	if (!ParentMaterial) return;

	const std::filesystem::path ProjectRoot = FPaths::RootDir();
	const std::filesystem::path ParentRelativePath = FPaths::ToWide(ParentMaterial->GetAssetPathFileName());

	const std::filesystem::path ParentFullPath = ProjectRoot / ParentRelativePath;
	const std::filesystem::path InstanceDir = ParentFullPath.parent_path();

	std::filesystem::create_directories(InstanceDir);

	const std::wstring ParentStem = ParentFullPath.stem().wstring();

	std::filesystem::path InstancePath;
	FString RelativePath;

	for (int32 Index = 0; Index < 1000; ++Index)
	{
		std::wstring FileName = ParentStem + L"_Inst";
		if (Index > 0)
		{
			FileName += L"_" + std::to_wstring(Index);
		}
		FileName += L".matinst";

		InstancePath = InstanceDir / FileName;

		if (!std::filesystem::exists(InstancePath))
		{
			RelativePath = FPaths::ToUtf8(InstancePath.lexically_relative(ProjectRoot).generic_wstring());
			break;
		}
	}

	if (RelativePath.empty()) return;

	json::JSON JsonData;
	JsonData[MatKeys::PathFileName] = RelativePath.c_str();
	JsonData[MatKeys::ParentMaterial] = ParentMaterial->GetAssetPathFileName().c_str();
	JsonData[MatKeys::Parameters] = json::JSON::Make(json::JSON::Class::Object);
	JsonData[MatKeys::Textures] = json::JSON::Make(json::JSON::Class::Object);

	std::ofstream File(InstancePath);
	if (!File.is_open()) return;

	File << JsonData.dump(4);
	File.close();

	FMaterialManager::Get().ScanMaterialAssets();
	FMaterialManager::Get().GetOrCreateMaterialInstance(RelativePath);
}
