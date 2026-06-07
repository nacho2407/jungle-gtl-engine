#include "UI/Asset/Material/MaterialEditorWidget.h"

#include "Editor/EditorEngine.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Component/Light/DirectionalLightComponent.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Static/StaticMeshAsset.h"
#include "Mesh/MeshManager.h"
#include "Object/Object.h"
#include "Texture/Texture2D.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "UI/ContentBrowser/ContentItem.h"
#include "Viewport/Viewport.h"

#include <ImGui/imgui.h>

#include <cmath>
#include <memory>
#include <utility>

static uint32 GNextMaterialEditorInstanceId = 0;

namespace
{
	constexpr uint32 PreviewSphereSlices = 32;
	constexpr uint32 PreviewSphereStacks = 16;
	constexpr float PreviewSphereRadius = 0.5f;

	FNormalVertex MakePreviewSphereVertex(float X, float Y, float Z, float U, float V)
	{
		FVector Normal(X, Y, Z);
		Normal.Normalize();

		FVector Tangent(-Y, X, 0.0f);
		if (Tangent.IsNearlyZero())
		{
			Tangent = FVector(1.0f, 0.0f, 0.0f);
		}
		else
		{
			Tangent.Normalize();
		}

		FNormalVertex Out{};
		Out.pos = FVector(X, Y, Z);
		Out.normal = Normal;
		Out.color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Out.tex = FVector2(U, V);
		Out.tangent = FVector4(Tangent.X, Tangent.Y, Tangent.Z, 1.0f);
		return Out;
	}

	UStaticMesh* CreateTransientMaterialPreviewSphere(ID3D11Device* Device)
	{
		if (!Device)
		{
			return nullptr;
		}

		std::unique_ptr<FStaticMesh> MeshAsset = std::make_unique<FStaticMesh>();
		MeshAsset->PathFileName = "__Transient/Editor/MaterialPreviewSphere";
		MeshAsset->Vertices.reserve((PreviewSphereSlices + 1) * (PreviewSphereStacks + 1));
		MeshAsset->Indices.reserve(PreviewSphereSlices * PreviewSphereStacks * 6);

		constexpr float Pi = 3.14159265358979323846f;
		for (uint32 Stack = 0; Stack <= PreviewSphereStacks; ++Stack)
		{
			const float V = static_cast<float>(Stack) / static_cast<float>(PreviewSphereStacks);
			const float Phi = Pi * V;
			const float SinPhi = std::sin(Phi);
			const float CosPhi = std::cos(Phi);

			for (uint32 Slice = 0; Slice <= PreviewSphereSlices; ++Slice)
			{
				const float U = static_cast<float>(Slice) / static_cast<float>(PreviewSphereSlices);
				const float Theta = 2.0f * Pi * U;
				const float X = PreviewSphereRadius * SinPhi * std::cos(Theta);
				const float Y = PreviewSphereRadius * SinPhi * std::sin(Theta);
				const float Z = PreviewSphereRadius * CosPhi;
				MeshAsset->Vertices.push_back(MakePreviewSphereVertex(X, Y, Z, U, V));
			}
		}

		for (uint32 Stack = 0; Stack < PreviewSphereStacks; ++Stack)
		{
			for (uint32 Slice = 0; Slice < PreviewSphereSlices; ++Slice)
			{
				const uint32 First = Stack * (PreviewSphereSlices + 1) + Slice;
				const uint32 Second = First + PreviewSphereSlices + 1;

				MeshAsset->Indices.push_back(First);
				MeshAsset->Indices.push_back(Second);
				MeshAsset->Indices.push_back(First + 1);

				MeshAsset->Indices.push_back(Second);
				MeshAsset->Indices.push_back(Second + 1);
				MeshAsset->Indices.push_back(First + 1);
			}
		}

		FStaticMeshSection Section{};
		Section.MaterialIndex = 0;
		Section.MaterialSlotName = "Preview";
		Section.FirstIndex = 0;
		Section.NumTriangles = static_cast<uint32>(MeshAsset->Indices.size() / 3);
		MeshAsset->Sections.push_back(Section);
		MeshAsset->CacheBounds();

		TArray<FStaticMaterial> Materials;
		FStaticMaterial Slot;
		Slot.MaterialSlotName = "Preview";
		Materials.push_back(Slot);

		UStaticMesh* Mesh = UObjectManager::Get().CreateObject<UStaticMesh>();
		if (!Mesh)
		{
			return nullptr;
		}

		Mesh->SetStaticMaterials(std::move(Materials));
		Mesh->SetStaticMeshAsset(MeshAsset.release());
		Mesh->SetAssetPathFileName("__Transient/Editor/MaterialPreviewSphere");
		Mesh->InitResources(Device);
		return Mesh;
	}

	UStaticMesh* LoadMaterialPreviewSphereMesh(ID3D11Device* Device)
	{
		static const char* CandidatePaths[] = {
			"Content/Data/BasicShape/Sphere_StaticMesh.uasset",
			"Content/Data/BasicShape/Sphere.OBJ",
			"Content/Data/BasicShape/Sphere.obj",
			"Content/Data/BasicShape/sphere.obj",
		};

		for (const char* Path : CandidatePaths)
		{
			if (UStaticMesh* Mesh = FMeshManager::LoadStaticMesh(Path, Device))
			{
				return Mesh;
			}
		}

		UE_LOG("MaterialEditor preview sphere asset missing. Using transient procedural sphere fallback.");
		return CreateTransientMaterialPreviewSphere(Device);
	}

	void EnsureJsonObject(json::JSON& JsonData, const char* Key)
	{
		if (!JsonData.hasKey(Key) || JsonData[Key].JSONType() != json::JSON::Class::Object)
		{
			JsonData[Key] = json::JSON::Make(json::JSON::Class::Object);
		}
	}

	void WriteMaterialInstanceOverridesToJson(UMaterialInstance* Instance, json::JSON& JsonData)
	{
		if (!Instance)
		{
			return;
		}

		if (UMaterial* Parent = Instance->GetParent())
		{
			JsonData[MatKeys::ParentMaterial] = Parent->GetAssetPathFileName().c_str();
		}

		EnsureJsonObject(JsonData, MatKeys::Parameters);
		EnsureJsonObject(JsonData, MatKeys::Textures);

		for (const auto& Pair : Instance->GetScalarOverrides())
		{
			JsonData[MatKeys::Parameters][Pair.first] = Pair.second;
		}

		for (const auto& Pair : Instance->GetVector3Overrides())
		{
			const FVector& Value = Pair.second;
			JsonData[MatKeys::Parameters][Pair.first] = json::Array(Value.X, Value.Y, Value.Z);
		}

		for (const auto& Pair : Instance->GetVector4Overrides())
		{
			const FVector4& Value = Pair.second;
			JsonData[MatKeys::Parameters][Pair.first] = json::Array(Value.X, Value.Y, Value.Z, Value.W);
		}

		for (const auto& Pair : Instance->GetMatrixOverrides())
		{
			const FMatrix& Value = Pair.second;
			json::JSON MatrixArray = json::Array();
			for (int32 Index = 0; Index < 16; ++Index)
			{
				MatrixArray.append(Value.Data[Index]);
			}
			JsonData[MatKeys::Parameters][Pair.first] = MatrixArray;
		}

		for (const auto& Pair : Instance->GetTextureOverrides())
		{
			JsonData[MatKeys::Textures][Pair.first] = Pair.second ? Pair.second->GetSourcePath().c_str() : "";
		}

		const FVector4 EmissiveColor = Instance->GetEmissiveColor();
		JsonData[MatKeys::bOverrideEmissiveColor] = Instance->HasEmissiveColorOverride();
		JsonData[MatKeys::EmissiveColor] = json::Array(
			EmissiveColor.X,
			EmissiveColor.Y,
			EmissiveColor.Z,
			EmissiveColor.W);
		JsonData[MatKeys::bOverrideEmissiveIntensity] = Instance->HasEmissiveIntensityOverride();
		JsonData[MatKeys::EmissiveIntensity] = Instance->GetEmissiveIntensity();
		JsonData[MatKeys::bOverrideEnableBloom] = Instance->HasBloomEnabledOverride();
		JsonData[MatKeys::bEnableBloom] = Instance->IsBloomEnabled();
	}
}

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

	UStaticMesh* SphereMesh = LoadMaterialPreviewSphereMesh(Device);
	Comp->SetStaticMesh(SphereMesh);
	Comp->SetMaterial(0, Material);
	Comp->SetNeverCullForRendering(true);

	Actor->SetRootComponent(Comp);
	Actor->SetActorLocation(FVector::ZeroVector);
	WorldContext.World->UpdateActorInOctree(Actor);
	WorldContext.World->GetPartition().FlushPrimitive();

	PreviewMeshComponent = Comp;

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));

	UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>();
	LightComp->SetShadowBias(0.0f);
	LightComp->PushToScene();

	// Open() can be called from Content Browser or a deferred open queue, not from
	// this editor window. Do not use ImGui::GetContentRegionAvail() here: it may
	// read another window's remaining space or return 0, producing an empty preview RT.
	constexpr uint32 InitialPreviewWidth = 512;
	constexpr uint32 InitialPreviewHeight = 512;
	ViewportClient.Initialize(Device, InitialPreviewWidth, InitialPreviewHeight);
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewMeshComponent(Comp);
	ViewportClient.ResetCameraToPreviewBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);
	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FMaterialEditorWidget::Close()
{
	if (IsDirty())
	{
		if (UMaterialInterface* Material = Cast<UMaterialInterface>(EditedObject))
		{
			FMaterialManager::Get().ReloadMaterialInterface(Material->GetAssetPathFileName());
		}
	}

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
	
	const bool bCanSave = IsDirty();

	if (!bCanSave)
	{
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Save"))
	{
		if (SaveMaterialJson())
		{
			ClearDirty();
		}
	}

	if (!bCanSave)
	{
		ImGui::EndDisabled();
	}

	ImGui::Separator();
	
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

		RenderBloomOverrides(Instance);
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
				MarkDirty();

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

void FMaterialEditorWidget::RenderBloomOverrides(UMaterialInstance* Instance)
{
	if (!Instance)
	{
		return;
	}

	ImGui::SeparatorText("Bloom Overrides");

	bool bAnyChanged = false;

	bool bOverrideColor = Instance->HasEmissiveColorOverride();
	FVector4 EmissiveColor = Instance->GetEmissiveColor();
	if (ImGui::Checkbox("Override Emissive Color", &bOverrideColor))
	{
		Instance->SetEmissiveColorOverride(bOverrideColor, EmissiveColor);
		CachedJson[MatKeys::bOverrideEmissiveColor] = bOverrideColor;
		CachedJson[MatKeys::EmissiveColor] = json::Array(EmissiveColor.X, EmissiveColor.Y, EmissiveColor.Z, EmissiveColor.W);
		bAnyChanged = true;
	}
	ImGui::BeginDisabled(!bOverrideColor);
	if (ImGui::ColorEdit4("Emissive Color", &EmissiveColor.X))
	{
		Instance->SetEmissiveColorOverride(true, EmissiveColor);
		CachedJson[MatKeys::bOverrideEmissiveColor] = true;
		CachedJson[MatKeys::EmissiveColor] = json::Array(EmissiveColor.X, EmissiveColor.Y, EmissiveColor.Z, EmissiveColor.W);
		bAnyChanged = true;
	}
	ImGui::EndDisabled();

	bool bOverrideIntensity = Instance->HasEmissiveIntensityOverride();
	float EmissiveIntensity = Instance->GetEmissiveIntensity();
	if (ImGui::Checkbox("Override Emissive Intensity", &bOverrideIntensity))
	{
		Instance->SetEmissiveIntensityOverride(bOverrideIntensity, EmissiveIntensity);
		CachedJson[MatKeys::bOverrideEmissiveIntensity] = bOverrideIntensity;
		CachedJson[MatKeys::EmissiveIntensity] = EmissiveIntensity;
		bAnyChanged = true;
	}
	ImGui::BeginDisabled(!bOverrideIntensity);
	if (ImGui::DragFloat("Emissive Intensity", &EmissiveIntensity, 0.05f, 0.0f, 100.0f))
	{
		Instance->SetEmissiveIntensityOverride(true, EmissiveIntensity);
		CachedJson[MatKeys::bOverrideEmissiveIntensity] = true;
		CachedJson[MatKeys::EmissiveIntensity] = EmissiveIntensity;
		bAnyChanged = true;
	}
	ImGui::EndDisabled();

	bool bOverrideBloom = Instance->HasBloomEnabledOverride();
	bool bEnableBloom = Instance->IsBloomEnabled();
	if (ImGui::Checkbox("Override Enable Bloom", &bOverrideBloom))
	{
		Instance->SetBloomEnabledOverride(bOverrideBloom, bEnableBloom);
		CachedJson[MatKeys::bOverrideEnableBloom] = bOverrideBloom;
		CachedJson[MatKeys::bEnableBloom] = bEnableBloom;
		bAnyChanged = true;
	}
	ImGui::BeginDisabled(!bOverrideBloom);
	if (ImGui::Checkbox("Enable Bloom", &bEnableBloom))
	{
		Instance->SetBloomEnabledOverride(true, bEnableBloom);
		CachedJson[MatKeys::bOverrideEnableBloom] = true;
		CachedJson[MatKeys::bEnableBloom] = bEnableBloom;
		bAnyChanged = true;
	}
	ImGui::EndDisabled();

	if (bAnyChanged)
	{
		MarkDirty();

		if (PreviewMeshComponent)
		{
			PreviewMeshComponent->SetMaterial(0, Instance);
		}
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
					auto MatrixArray = json::Array();
					for (int32 i = 0; i < 16; ++i)
					{
						MatrixArray.append(Param.Data[i]);
					}
					CachedJson[MatKeys::Parameters][ParamName] = MatrixArray;
					bChanged = true;
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
	if (Textures->find("DiffuseTexture") == Textures->end())
	{
		(*Textures)["DiffuseTexture"] = nullptr;
	}

	for (auto& Pair : *Textures)
	{
		FString SlotName = Pair.first.c_str();

		UTexture2D* Texture = Pair.second;
		Material->GetTextureParameter(SlotName, Texture);

		ImGui::PushID(SlotName.c_str());
		ImGui::TextUnformatted(SlotName == "DiffuseTexture" ? "Diffuse Texture" : SlotName.c_str());

		if (Texture && Texture->GetSRV())
		{
			ImGui::Image(Texture->GetSRV(), ImVec2(100, 100));
		}
		else
		{
			ImGui::Button("##TextureDropTarget", ImVec2(100, 100));

			const ImVec2 Min = ImGui::GetItemRectMin();
			const ImVec2 Max = ImGui::GetItemRectMax();
			const char* GuideText = "Drag PNG\nhere";
			const ImVec2 TextSize = ImGui::CalcTextSize(GuideText);
			const ImVec2 TextPos(
				Min.x + (Max.x - Min.x - TextSize.x) * 0.5f,
				Min.y + (Max.y - Min.y - TextSize.y) * 0.5f);
			ImGui::GetWindowDrawList()->AddText(
				TextPos,
				ImGui::GetColorU32(ImGuiCol_TextDisabled),
				GuideText);
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

				if (PreviewMeshComponent)
				{
					PreviewMeshComponent->SetMaterial(0, Material);
				}
			}
		}

		ImGui::PopID();
	}
}

bool FMaterialEditorWidget::SaveMaterialJson()
{
	UMaterialInterface* Material = Cast<UMaterialInterface>(EditedObject);
	if (!Material || MaterialPath.empty()) return false;

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

	const std::filesystem::path ProjectRoot = FPaths::RootDir();
	const FString RelativeMaterialPath = FPaths::ToUtf8(
		MaterialPath.lexically_relative(ProjectRoot).generic_wstring());
	CachedJson[MatKeys::PathFileName] = RelativeMaterialPath.c_str();

	if (UMaterial* BaseMaterial = Cast<UMaterial>(Material))
	{
		if (CachedJson[MatKeys::ShaderPath].ToString().empty())
		{
			CachedJson[MatKeys::ShaderPath] = "Shaders/Geometry/UberLit.hlsl";
		}

		if (!CachedJson.hasKey(MatKeys::Parameters) || CachedJson[MatKeys::Parameters].JSONType() != json::JSON::Class::Object)
		{
			CachedJson[MatKeys::Parameters] = json::JSON::Make(json::JSON::Class::Object);
		}

		if (!CachedJson.hasKey(MatKeys::Textures) || CachedJson[MatKeys::Textures].JSONType() != json::JSON::Class::Object)
		{
			CachedJson[MatKeys::Textures] = json::JSON::Make(json::JSON::Class::Object);
		}

		const FEnum* ShadowEnum = FEnum::FindEnumByName("EMaterialShadowMode");
		if (ShadowEnum && ShadowEnum->GetNames())
		{
			const int32 Index = static_cast<int32>(Material->GetShadowMode());
			if (Index >= 0 && static_cast<uint32>(Index) < ShadowEnum->GetCount())
			{
				CachedJson[MatKeys::ShadowMode] = ShadowEnum->GetNames()[Index];
			}
		}

		using namespace RenderStateStrings;

		CachedJson[MatKeys::RenderPass] = ToString(RenderPassMap, BaseMaterial->GetRenderPass());
		CachedJson[MatKeys::BlendState] = ToString(BlendStateMap, BaseMaterial->GetBlendState());
		CachedJson[MatKeys::DepthStencilState] = ToString(DepthStencilStateMap, BaseMaterial->GetDepthStencilState());
		CachedJson[MatKeys::RasterizerState] = ToString(RasterizerStateMap, BaseMaterial->GetRasterizerState());

		const FVector4 EmissiveColor = BaseMaterial->GetEmissiveColor();
		CachedJson[MatKeys::EmissiveColor] = json::Array(
			EmissiveColor.X,
			EmissiveColor.Y,
			EmissiveColor.Z,
			EmissiveColor.W);
		CachedJson[MatKeys::EmissiveIntensity] = BaseMaterial->GetEmissiveIntensity();
		CachedJson[MatKeys::bEnableBloom] = BaseMaterial->IsBloomEnabled();
	}
	else if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Material))
	{
		WriteMaterialInstanceOverridesToJson(Instance, CachedJson);
	}

	std::ofstream File(MaterialPath);
	if (!File.is_open())
	{
		return false;
	}

	File << CachedJson.dump(4);
	return File.good();
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
	JsonData[MatKeys::bOverrideEmissiveColor] = false;
	JsonData[MatKeys::bOverrideEmissiveIntensity] = false;
	JsonData[MatKeys::bOverrideEnableBloom] = false;

	std::ofstream File(InstancePath);
	if (!File.is_open()) return;

	File << JsonData.dump(4);
	File.close();

	FMaterialManager::Get().ScanMaterialAssets();
	FMaterialManager::Get().GetOrCreateMaterialInstance(RelativePath);
}
