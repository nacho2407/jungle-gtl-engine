#include "ObjViewerEngine.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "ObjViewerShell.h"
#include "ObjViewerViewportClient.h"

#include "Actor/Actor.h"
#include "Actor/StaticMeshActor.h"
#include "Asset/ObjManager.h"
#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/Paths.h"
#include "Core/ShowFlags.h"
#include "Debug/EngineLog.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Frustum.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/MeshData.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/ShaderMap.h"
#include "Scene/Scene.h"
#include "World/World.h"

namespace
{
	FString WideToUtf8(const std::wstring& WideString)
	{
		if (WideString.empty())
		{
			return "";
		}

		const int32 RequiredBytes = ::WideCharToMultiByte(
			CP_UTF8,
			0,
			WideString.c_str(),
			-1,
			nullptr,
			0,
			nullptr,
			nullptr);
		if (RequiredBytes <= 1)
		{
			return "";
		}

		FString Result;
		Result.resize(static_cast<size_t>(RequiredBytes));
		::WideCharToMultiByte(
			CP_UTF8,
			0,
			WideString.c_str(),
			-1,
			Result.data(),
			RequiredBytes,
			nullptr,
			nullptr);
		Result.pop_back();
		return Result;
	}

	bool MeshHasAnyUVs(const UStaticMesh* Mesh)
	{
		if (Mesh == nullptr || Mesh->GetRenderData() == nullptr)
		{
			return false;
		}

		for (const FVertex& Vertex : Mesh->GetRenderData()->Vertices)
		{
			if (Vertex.UV.X != 0.0f || Vertex.UV.Y != 0.0f)
			{
				return true;
			}
		}

		return false;
	}

	float GetMaxAbsScale(const FVector& Scale)
	{
		const float AbsX = std::fabs(Scale.X);
		const float AbsY = std::fabs(Scale.Y);
		const float AbsZ = std::fabs(Scale.Z);
		return (std::max)(AbsX, (std::max)(AbsY, AbsZ));
	}

	TArray<FString> BuildMaterialSlotNames(const UStaticMesh* Mesh)
	{
		TArray<FString> MaterialSlotNames;
		if (Mesh == nullptr || Mesh->GetRenderData() == nullptr)
		{
			return MaterialSlotNames;
		}

		uint32 SlotCount = static_cast<uint32>(Mesh->GetDefaultMaterials().size());
		for (const FMeshSection& Section : Mesh->GetRenderData()->Sections)
		{
			SlotCount = (std::max)(SlotCount, Section.MaterialIndex + 1);
		}

		if (SlotCount == 0)
		{
			SlotCount = 1;
		}

		MaterialSlotNames.resize(SlotCount, "M_Default");

		const TArray<std::shared_ptr<FMaterial>>& DefaultMaterials = Mesh->GetDefaultMaterials();
		for (uint32 SlotIndex = 0; SlotIndex < SlotCount && SlotIndex < DefaultMaterials.size(); ++SlotIndex)
		{
			const std::shared_ptr<FMaterial>& Material = DefaultMaterials[SlotIndex];
			if (Material && !Material->GetOriginName().empty())
			{
				MaterialSlotNames[SlotIndex] = Material->GetOriginName();
			}
		}

		return MaterialSlotNames;
	}

	FStaticMesh BuildBakedMeshCopy(const FStaticMesh& SourceMesh, float UniformScale)
	{
		const float BakedScale = (std::max)(UniformScale, 0.01f);

		FStaticMesh BakedMesh;
		BakedMesh.Topology = SourceMesh.Topology;
		BakedMesh.PathFileName = SourceMesh.PathFileName;
		BakedMesh.Sections = SourceMesh.Sections;
		BakedMesh.Indices = SourceMesh.Indices;
		BakedMesh.Vertices = SourceMesh.Vertices;

		for (FVertex& Vertex : BakedMesh.Vertices)
		{
			Vertex.Position = Vertex.Position * BakedScale;
			if (!Vertex.Normal.IsNearlyZero())
			{
				Vertex.Normal = Vertex.Normal.GetSafeNormal();
			}
		}

		BakedMesh.bIsDirty = true;
		return BakedMesh;
	}

	FVector GetLoadedModelWorldCenter(const FObjViewerModelState& ModelState)
	{
		if (ModelState.DisplayActor == nullptr)
		{
			return FVector::ZeroVector;
		}

		USceneComponent* RootComponent = ModelState.DisplayActor->GetRootComponent();
		if (RootComponent == nullptr)
		{
			return FVector::ZeroVector;
		}

		return RootComponent->GetWorldTransform().TransformPosition(ModelState.BoundsCenter);
	}

	float GetDisplayedBoundsRadius(const FObjViewerModelState& ModelState)
	{
		float Radius = ModelState.BoundsRadius;
		if (ModelState.DisplayActor)
		{
			if (USceneComponent* RootComponent = ModelState.DisplayActor->GetRootComponent())
			{
				Radius *= GetMaxAbsScale(RootComponent->GetRelativeTransform().GetScale3D());
			}
		}

		return Radius;
	}

	FVector4 GetNormalVisualizationColor(EObjViewerNormalVisualizationMode Mode)
	{
		switch (Mode)
		{
		case EObjViewerNormalVisualizationMode::Face:
			return FVector4(1.0f, 0.55f, 0.1f, 1.0f);
		case EObjViewerNormalVisualizationMode::Vertex:
		default:
			return FVector4(0.15f, 0.85f, 1.0f, 1.0f);
		}
	}
}

FObjViewerEngine::FObjViewerEngine()
	: ViewerShell(std::make_unique<FObjViewerShell>())
{
}

FObjViewerEngine::~FObjViewerEngine() = default;

ULevel* FObjViewerEngine::GetActiveScene() const
{
	return (ViewerWorldContext && ViewerWorldContext->World) ? ViewerWorldContext->World->GetScene() : nullptr;
}

UWorld* FObjViewerEngine::GetActiveWorld() const
{
	return ViewerWorldContext ? ViewerWorldContext->World : nullptr;
}

const FWorldContext* FObjViewerEngine::GetActiveWorldContext() const
{
	return (ViewerWorldContext && ViewerWorldContext->IsValid()) ? ViewerWorldContext : nullptr;
}

bool FObjViewerEngine::LoadModelFromFile(const FString& FilePath, const FString& ImportSource)
{
	FObjImportSummary ImportOptions;
	ImportOptions.ImportSource = ImportSource;
	return LoadModelFromFile(FilePath, ImportOptions);
}

bool FObjViewerEngine::LoadModelFromFile(const FString& FilePath, const FObjImportSummary& ImportOptions)
{
	if (FilePath.empty())
	{
		return false;
	}

	UWorld* ViewerWorld = GetActiveWorld();
	ULevel* ViewerScene = GetActiveScene();
	if (!ViewerWorld || !ViewerScene)
	{
		return false;
	}

	FObjImportSummary AppliedImportOptions = ImportOptions;
	AppliedImportOptions.bReplaceCurrentModel = true;
	AppliedImportOptions.UniformScale = (std::max)(AppliedImportOptions.UniformScale, 0.01f);

	FObjLoadOptions LoadOptions;
	LoadOptions.bUseLegacyObjConversion = false;
	LoadOptions.ForwardAxis = AppliedImportOptions.ForwardAxis;
	LoadOptions.UpAxis = AppliedImportOptions.UpAxis;

	UStaticMesh* LoadedMesh = FObjManager::LoadObjStaticMeshAsset(FilePath, LoadOptions);
	if (!LoadedMesh)
	{
		UE_LOG("[ObjViewer] Failed to load OBJ: %s", FilePath.c_str());
		return false;
	}

	if (AppliedImportOptions.bReplaceCurrentModel || !HasLoadedModel())
	{
		ClearLoadedModel();
	}

	AStaticMeshActor* DisplayActor = ViewerWorld->SpawnActor<AStaticMeshActor>("DroppedObjActor");
	if (!DisplayActor)
	{
		UE_LOG("[ObjViewer] Failed to spawn actor for OBJ: %s", FilePath.c_str());
		return false;
	}

	UStaticMeshComponent* MeshComponent = DisplayActor->GetComponentByClass<UStaticMeshComponent>();
	if (!MeshComponent)
	{
		UE_LOG("[ObjViewer] Failed to get mesh component for OBJ: %s", FilePath.c_str());
		return false;
	}

	MeshComponent->SetStaticMesh(LoadedMesh);

	if (USceneComponent* RootComponent = DisplayActor->GetRootComponent())
	{
		FTransform Transform = RootComponent->GetRelativeTransform();
		Transform.SetScale3D(FVector(
			AppliedImportOptions.UniformScale,
			AppliedImportOptions.UniformScale,
			AppliedImportOptions.UniformScale));

		const FVector ScaledCenter = LoadedMesh->LocalBounds.Center * AppliedImportOptions.UniformScale;
		const FVector ScaledExtent = LoadedMesh->LocalBounds.BoxExtent * AppliedImportOptions.UniformScale;
		FVector Translation = FVector::ZeroVector;

		if (AppliedImportOptions.bCenterToOrigin)
		{
			Translation = -ScaledCenter;
		}

		if (AppliedImportOptions.bPlaceOnGround)
		{
			const float BottomZ = (ScaledCenter.Z + Translation.Z) - ScaledExtent.Z;
			Translation.Z -= BottomZ;
		}

		Transform.SetTranslation(Translation);
		RootComponent->SetRelativeTransform(Transform);
	}

	UpdateLoadedModelState(FilePath, AppliedImportOptions, LoadedMesh, DisplayActor);
	if (AppliedImportOptions.bFrameCameraAfterImport)
	{
		FrameLoadedModel();
	}

	return true;
}

bool FObjViewerEngine::ExportLoadedModelAsModel(const FString& FilePath) const
{
	if (FilePath.empty() || !HasLoadedModel() || ModelState.Mesh == nullptr || ModelState.Mesh->GetRenderData() == nullptr)
	{
		UE_LOG("[ObjViewer] Export skipped because no valid mesh is loaded.");
		return false;
	}

	FStaticMesh BakedMesh = BuildBakedMeshCopy(
		*ModelState.Mesh->GetRenderData(),
		ModelState.LastImportSummary.UniformScale);
	const TArray<FString> MaterialSlotNames = BuildMaterialSlotNames(ModelState.Mesh);
	TArray<FModelMaterialInfo> MaterialInfos;
	const bool bBuiltMaterialInfos = FObjManager::BuildModelMaterialInfosFromObj(ModelState.SourceFilePath, FilePath, MaterialSlotNames, MaterialInfos);
	if (!bBuiltMaterialInfos)
	{
		UE_LOG("[ObjViewer] Falling back to default embedded material metadata for export: %s", ModelState.SourceFilePath.c_str());
	}

	const bool bSaved = FObjManager::SaveModelStaticMeshAsset(FilePath, BakedMesh, MaterialInfos);
	UE_LOG("[ObjViewer] %s .Model export: %s", bSaved ? "Succeeded" : "Failed", FilePath.c_str());
	return bSaved;
}

bool FObjViewerEngine::ReloadLoadedModel()
{
	if (ModelState.SourceFilePath.empty())
	{
		return false;
	}

	FObjImportSummary ReloadOptions = ModelState.LastImportSummary;
	ReloadOptions.ImportSource = "Reload";
	return LoadModelFromFile(ModelState.SourceFilePath, ReloadOptions);
}

void FObjViewerEngine::ClearLoadedModel()
{
	UWorld* ViewerWorld = GetActiveWorld();
	ULevel* ViewerScene = GetActiveScene();
	if (ViewerScene)
	{
		const TArray<AActor*> ExistingActors = ViewerScene->GetActors();
		for (AActor* Actor : ExistingActors)
		{
			if (Actor == nullptr || Actor->IsPendingDestroy())
			{
				continue;
			}

			if (ViewerWorld)
			{
				ViewerWorld->DestroyActor(Actor);
			}
			else
			{
				ViewerScene->DestroyActor(Actor);
			}
		}

		ViewerScene->CleanupDestroyedActors();
	}

	ModelState = {};
}

void FObjViewerEngine::FrameLoadedModel()
{
	if (!HasLoadedModel())
	{
		ResetViewerCamera();
		return;
	}

	UWorld* ViewerWorld = GetActiveWorld();
	if (!ViewerWorld)
	{
		return;
	}

	UCameraComponent* ActiveCamera = ViewerWorld->GetActiveCameraComponent();
	if (!ActiveCamera)
	{
		return;
	}

	if (FCamera* Camera = ActiveCamera->GetCamera())
	{
		float Radius = ModelState.BoundsRadius;
		if (ModelState.DisplayActor)
		{
			if (USceneComponent* RootComponent = ModelState.DisplayActor->GetRootComponent())
			{
				Radius *= GetMaxAbsScale(RootComponent->GetRelativeTransform().GetScale3D());
			}
		}

		const float Distance = (std::max)(Radius, 1.0f) * 3.0f;
		const FVector WorldCenter = GetLoadedModelWorldCenter(ModelState);
		Camera->SetPosition(WorldCenter + FVector(Distance, 0.0f, 0.0f));
		Camera->SetRotation(180.0f, 0.0f);
	}

	ActiveCamera->SetFov(50.0f);
}

void FObjViewerEngine::ResetViewerCamera()
{
	InitializeViewerCamera();
}

FObjViewerShell& FObjViewerEngine::GetShell() const
{
	return *ViewerShell;
}

void FObjViewerEngine::BindHost(FWindowsWindow* InMainWindow)
{
	MainWindow = InMainWindow;

	if (ViewerShell)
	{
		ViewerShell->Initialize(this);
		ViewerShell->SetupWindow(InMainWindow);
	}
}

bool FObjViewerEngine::InitializeWorlds(int32 Width, int32 Height)
{
	const float AspectRatio = (Height > 0)
		? (static_cast<float>(Width) / static_cast<float>(Height))
		: 1.0f;

	ViewerWorldContext = CreateWorldContext("ObjViewerScene", EWorldType::Preview, AspectRatio, false);
	if (!ViewerWorldContext || !ViewerWorldContext->World)
	{
		return false;
	}

	InitializeViewerCamera();
	return true;
}

bool FObjViewerEngine::InitializeMode()
{
	WireframeMaterial = FMaterialManager::Get().FindByName(WireframeMaterialName);
	CreateGridResources();
	return true;
}

std::unique_ptr<IViewportClient> FObjViewerEngine::CreateViewportClient()
{
	return std::make_unique<FObjViewerViewportClient>();
}

void FObjViewerEngine::TickWorlds(float DeltaTime)
{
	if (UWorld* ViewerWorld = GetActiveWorld())
	{
		ViewerWorld->Tick(DeltaTime);
	}
}

void FObjViewerEngine::RenderFrame()
{
	FRenderer* Renderer = GetRenderer();
	if (!Renderer || Renderer->IsOccluded())
	{
		return;
	}

	if (ViewerShell)
	{
		ViewerShell->PrepareViewportSurface(Renderer);

		const FObjViewerViewportSurface& Surface = ViewerShell->GetViewportSurface();
		if (Surface.IsValid())
		{
			D3D11_VIEWPORT SceneViewport = {};
			SceneViewport.TopLeftX = 0.0f;
			SceneViewport.TopLeftY = 0.0f;
			SceneViewport.Width = static_cast<float>(Surface.GetWidth());
			SceneViewport.Height = static_cast<float>(Surface.GetHeight());
			SceneViewport.MinDepth = 0.0f;
			SceneViewport.MaxDepth = 1.0f;

			Renderer->SetSceneRenderTarget(Surface.GetRTV(), Surface.GetDSV(), SceneViewport);

			if (UWorld* ActiveWorld = GetActiveWorld())
			{
				const float AspectRatio = static_cast<float>(Surface.GetWidth()) / static_cast<float>(Surface.GetHeight());
				UpdateWorldAspectRatio(ActiveWorld, AspectRatio);
			}
		}
		else
		{
			Renderer->ClearSceneRenderTarget();
		}
	}

	Renderer->BeginFrame();

	UWorld* ActiveWorld = GetActiveWorld();
	ULevel* Scene = GetViewportClient() ? GetViewportClient()->ResolveScene(this) : GetActiveScene();
	FShowFlags ViewerShowFlags;
	ViewerShowFlags.SetFlag(EEngineShowFlags::SF_UUID, false);
	ViewerShowFlags.SetFlag(EEngineShowFlags::SF_DebugDraw, NormalSettings.bVisible);
	if (Scene && ActiveWorld)
	{
		UCameraComponent* ActiveCamera = ActiveWorld->GetActiveCameraComponent();
		if (ActiveCamera)
		{
			FRenderCommandQueue Queue;
			Queue.Reserve(Renderer->GetPrevCommandCount());
			Queue.ViewMatrix = ActiveCamera->GetViewMatrix();
			Queue.ProjectionMatrix = ActiveCamera->GetProjectionMatrix();

			FFrustum Frustum;
			const FMatrix ViewProjection = Queue.ViewMatrix * Queue.ProjectionMatrix;
			Frustum.ExtractFromVP(ViewProjection);

			if (IViewportClient* ViewportClient = GetViewportClient())
			{
				const FVector CameraPosition = Queue.ViewMatrix.GetInverse().GetTranslation();
				ViewportClient->BuildRenderCommands(this, Scene, Frustum, ViewerShowFlags, CameraPosition, Queue);
			}
			ApplyWireframeOverride(Queue);
			AppendGridRenderCommand(Queue);

			Renderer->SubmitCommands(Queue);
			Renderer->ExecuteCommands();
			AppendNormalVisualizationDebugDraw();
			GetDebugDrawManager().Flush(Renderer, ViewerShowFlags, ActiveWorld);
		}
	}

	if (ViewerShell)
	{
		ViewerShell->Render();
	}

	Renderer->EndFrame();
	Renderer->ClearSceneRenderTarget();
}

void FObjViewerEngine::InitializeViewerCamera() const
{
	UWorld* ViewerWorld = GetActiveWorld();
	if (!ViewerWorld)
	{
		return;
	}

	UCameraComponent* ActiveCamera = ViewerWorld->GetActiveCameraComponent();
	if (!ActiveCamera)
	{
		return;
	}

	if (FCamera* Camera = ActiveCamera->GetCamera())
	{
		Camera->SetPosition({ 8.0f, 0.0f, 0.0f });
		Camera->SetRotation(180.0f, 0.0f);
	}

	ActiveCamera->SetFov(50.0f);
}

void FObjViewerEngine::CreateGridResources()
{
	FRenderer* Renderer = GetRenderer();
	if (Renderer == nullptr || GridMesh || GridMaterial)
	{
		return;
	}

	ID3D11Device* Device = Renderer->GetDevice();
	if (Device == nullptr)
	{
		return;
	}

	constexpr int32 GridVertexCount = 42;

	GridMesh = std::make_unique<FDynamicMesh>();
	GridMesh->Topology = EMeshTopology::EMT_TriangleList;
	for (int32 Index = 0; Index < GridVertexCount; ++Index)
	{
		FVertex Vertex;
		GridMesh->Vertices.push_back(Vertex);
		GridMesh->Indices.push_back(Index);
	}
	GridMesh->CreateVertexAndIndexBuffer(Device);

	std::wstring ShaderDirW = FPaths::ShaderDir();
	std::wstring VSPath = ShaderDirW + L"AxisVertexShader.hlsl";
	std::wstring PSPath = ShaderDirW + L"AxisPixelShader.hlsl";
	auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
	auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, PSPath.c_str());

	GridMaterial = std::make_shared<FMaterial>();
	GridMaterial->SetOriginName("M_ObjViewerGrid");
	GridMaterial->SetVertexShader(VS);
	GridMaterial->SetPixelShader(PS);

	FRasterizerStateOption RasterizerOption;
	RasterizerOption.FillMode = D3D11_FILL_SOLID;
	RasterizerOption.CullMode = D3D11_CULL_NONE;
	auto RS = Renderer->GetRenderStateManager()->GetOrCreateRasterizerState(RasterizerOption);
	GridMaterial->SetRasterizerOption(RasterizerOption);
	GridMaterial->SetRasterizerState(RS);

	FDepthStencilStateOption DepthStencilOption;
	DepthStencilOption.DepthEnable = true;
	DepthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	auto DSS = Renderer->GetRenderStateManager()->GetOrCreateDepthStencilState(DepthStencilOption);
	GridMaterial->SetDepthStencilOption(DepthStencilOption);
	GridMaterial->SetDepthStencilState(DSS);

	int32 SlotIndex = GridMaterial->CreateConstantBuffer(Device, 64);
	if (SlotIndex < 0)
	{
		return;
	}

	GridMaterial->RegisterParameter("GridSize", SlotIndex, 0, 4);
	GridMaterial->RegisterParameter("LineThickness", SlotIndex, 4, 4);
	GridMaterial->RegisterParameter("GridAxisU", SlotIndex, 16, 12);
	GridMaterial->RegisterParameter("GridAxisV", SlotIndex, 32, 12);
	GridMaterial->RegisterParameter("ViewForward", SlotIndex, 48, 12);

	const FVector DefaultGridAxisU = FVector::ForwardVector;
	const FVector DefaultGridAxisV = FVector::RightVector;
	const FVector DefaultViewForward = FVector::ForwardVector;
	GridMaterial->SetParameterData("GridSize", &GridSettings.GridSize, 4);
	GridMaterial->SetParameterData("LineThickness", &GridSettings.LineThickness, 4);
	GridMaterial->SetParameterData("GridAxisU", &DefaultGridAxisU, sizeof(FVector));
	GridMaterial->SetParameterData("GridAxisV", &DefaultGridAxisV, sizeof(FVector));
	GridMaterial->SetParameterData("ViewForward", &DefaultViewForward, sizeof(FVector));
}

void FObjViewerEngine::ApplyWireframeOverride(FRenderCommandQueue& Queue) const
{
	if (!bWireframeEnabled || !WireframeMaterial)
	{
		return;
	}

	for (FRenderCommand& Command : Queue.Commands)
	{
		if (Command.RenderLayer != ERenderLayer::Overlay)
		{
			Command.Material = WireframeMaterial.get();
		}
	}
}

void FObjViewerEngine::AppendNormalVisualizationDebugDraw()
{
	if (!NormalSettings.bVisible || !HasLoadedModel() || ModelState.Mesh == nullptr || ModelState.Mesh->GetRenderData() == nullptr)
	{
		return;
	}

	AStaticMeshActor* DisplayActor = ModelState.DisplayActor;
	if (DisplayActor == nullptr)
	{
		return;
	}

	USceneComponent* RootComponent = DisplayActor->GetRootComponent();
	if (RootComponent == nullptr)
	{
		return;
	}

	const FStaticMesh* RenderData = ModelState.Mesh->GetRenderData();
	const FMatrix& WorldMatrix = RootComponent->GetWorldTransform();
	const float NormalLength = (std::max)(GetDisplayedBoundsRadius(ModelState) * NormalSettings.LengthScale, 0.001f);
	const FVector4 NormalColor = GetNormalVisualizationColor(NormalSettings.Mode);
	FDebugDrawManager& DebugDrawManager = GetDebugDrawManager();

	if (NormalSettings.Mode == EObjViewerNormalVisualizationMode::Face)
	{
		for (size_t Index = 0; Index + 2 < RenderData->Indices.size(); Index += 3)
		{
			const uint32 Index0 = RenderData->Indices[Index];
			const uint32 Index1 = RenderData->Indices[Index + 1];
			const uint32 Index2 = RenderData->Indices[Index + 2];
			if (Index0 >= RenderData->Vertices.size() || Index1 >= RenderData->Vertices.size() || Index2 >= RenderData->Vertices.size())
			{
				continue;
			}

			const FVector WorldPosition0 = WorldMatrix.TransformPosition(RenderData->Vertices[Index0].Position);
			const FVector WorldPosition1 = WorldMatrix.TransformPosition(RenderData->Vertices[Index1].Position);
			const FVector WorldPosition2 = WorldMatrix.TransformPosition(RenderData->Vertices[Index2].Position);
			const FVector FaceNormal = FVector::CrossProduct(WorldPosition1 - WorldPosition0, WorldPosition2 - WorldPosition0).GetSafeNormal();
			if (FaceNormal.IsNearlyZero())
			{
				continue;
			}

			const FVector FaceCenter = (WorldPosition0 + WorldPosition1 + WorldPosition2) / 3.0f;
			DebugDrawManager.DrawLine(FaceCenter, FaceCenter + (FaceNormal * NormalLength), NormalColor);
		}

		return;
	}

	for (const FVertex& Vertex : RenderData->Vertices)
	{
		if (Vertex.Normal.IsNearlyZero())
		{
			continue;
		}

		const FVector WorldStart = WorldMatrix.TransformPosition(Vertex.Position);
		const FVector WorldNormal = WorldMatrix.TransformVector(Vertex.Normal).GetSafeNormal();
		if (WorldNormal.IsNearlyZero())
		{
			continue;
		}

		DebugDrawManager.DrawLine(WorldStart, WorldStart + (WorldNormal * NormalLength), NormalColor);
	}
}

void FObjViewerEngine::AppendGridRenderCommand(FRenderCommandQueue& Queue) const
{
	if (!GridSettings.bVisible || !GridMesh || !GridMaterial)
	{
		return;
	}

	const FMatrix ViewInverse = Queue.ViewMatrix.GetInverse();
	const FVector GridAxisU = FVector::ForwardVector;
	const FVector GridAxisV = FVector::RightVector;
	const FVector ViewForward = ViewInverse.GetForwardVector().GetSafeNormal();

	GridMaterial->SetParameterData("GridSize", &GridSettings.GridSize, 4);
	GridMaterial->SetParameterData("LineThickness", &GridSettings.LineThickness, 4);
	GridMaterial->SetParameterData("GridAxisU", &GridAxisU, sizeof(FVector));
	GridMaterial->SetParameterData("GridAxisV", &GridAxisV, sizeof(FVector));
	GridMaterial->SetParameterData("ViewForward", &ViewForward, sizeof(FVector));

	FRenderCommand GridCommand;
	GridCommand.RenderMesh = GridMesh.get();
	GridCommand.Material = GridMaterial.get();
	GridCommand.WorldMatrix = FMatrix::Identity;
	GridCommand.RenderLayer = ERenderLayer::Default;
	Queue.AddCommand(GridCommand);
}

void FObjViewerEngine::UpdateLoadedModelState(
	const FString& FilePath,
	const FObjImportSummary& ImportOptions,
	UStaticMesh* Mesh,
	AStaticMeshActor* DisplayActor)
{
	ModelState = {};
	ModelState.bLoaded = true;
	ModelState.SourceFilePath = FilePath;
	ModelState.DisplayActor = DisplayActor;
	ModelState.Mesh = Mesh;
	ModelState.LastImportSummary = ImportOptions;

	const std::filesystem::path SourcePath(FPaths::ToWide(FilePath));
	ModelState.FileName = WideToUtf8(SourcePath.filename().wstring());

	std::error_code ErrorCode;
	ModelState.FileSizeBytes = std::filesystem::file_size(SourcePath, ErrorCode);
	if (ErrorCode)
	{
		ModelState.FileSizeBytes = 0;
	}

	if (Mesh && Mesh->GetRenderData())
	{
		FStaticMesh* RenderData = Mesh->GetRenderData();
		ModelState.VertexCount = static_cast<int32>(RenderData->Vertices.size());
		ModelState.IndexCount = static_cast<int32>(RenderData->Indices.size());
		ModelState.TriangleCount = static_cast<int32>(RenderData->Indices.size() / 3);
		ModelState.SectionCount = RenderData->GetNumSection();
		ModelState.bHasUV = MeshHasAnyUVs(Mesh);
	}

	if (Mesh)
	{
		ModelState.BoundsCenter = Mesh->LocalBounds.Center;
		ModelState.BoundsRadius = Mesh->LocalBounds.Radius;
		ModelState.BoundsExtent = Mesh->LocalBounds.BoxExtent;
	}
}
