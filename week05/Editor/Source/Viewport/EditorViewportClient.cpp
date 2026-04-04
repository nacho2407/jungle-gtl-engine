#include "EditorViewportClient.h"

#include "EditorEngine.h"
#include "EditorViewportRegistry.h"
#include "UI/EditorUI.h"
#include "Core/Paths.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/ShaderMap.h"
#include "imgui.h"
#include "Viewport.h"

FEditorViewportClient::FEditorViewportClient(
	FEditorEngine& InEditorEngine,
	FEditorUI& InEditorUI,
	FEditorViewportRegistry& InViewportRegistry,
	FWindowsWindow* InMainWindow)
	: EditorUI(InEditorUI)
	, MainWindow(InMainWindow)
	, EditorEngine(InEditorEngine)
	, ViewportRegistry(InViewportRegistry)
{
}

void FEditorViewportClient::Attach(FEngine* Engine, FRenderer* Renderer)
{
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	if (!EditorEngine || !Renderer)
	{
		return;
	}

	EditorUI.Initialize(EditorEngine);
	//EditorUI.AttachToRenderer(Renderer); GameGem

	BlitRenderer.Initialize(Renderer->GetDevice());

	// Cache wireframe material for wireframe view mode.
	WireFrameMaterial = FMaterialManager::Get().FindByName(WireframeMaterialName);
	CreateGridResource(Renderer);
}

void FEditorViewportClient::CreateGridResource(FRenderer* Renderer)
{
	ID3D11Device* Device = Renderer->GetDevice();
	if (Device)
	{
		constexpr int32 GridVertexCount = 42;

		GridMesh = std::make_unique<FDynamicMesh>();
		GridMesh->Topology = EMeshTopology::EMT_TriangleList;
		for (int32 i = 0; i < GridVertexCount; ++i)
		{
			FVertex Vertex;
			GridMesh->Vertices.push_back(Vertex);
			GridMesh->Indices.push_back(i);
		}
		GridMesh->CreateVertexAndIndexBuffer(Device);

		std::wstring ShaderDirW = FPaths::ShaderDir();
		std::wstring VSPath = ShaderDirW + L"AxisVertexShader.hlsl";
		std::wstring PSPath = ShaderDirW + L"AxisPixelShader.hlsl";
		auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
		auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, PSPath.c_str());

		GridMaterial = std::make_shared<FMaterial>();
		GridMaterial->SetOriginName("M_EditorGrid");
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
		if (SlotIndex >= 0)
		{
			GridMaterial->RegisterParameter("GridSize", SlotIndex, 0, 4);
			GridMaterial->RegisterParameter("LineThickness", SlotIndex, 4, 4);
			GridMaterial->RegisterParameter("GridAxisU", SlotIndex, 16, 12);
			GridMaterial->RegisterParameter("GridAxisV", SlotIndex, 32, 12);
			GridMaterial->RegisterParameter("ViewForward", SlotIndex, 48, 12);

			float DefaultGridSize = 10.0f;
			float DefaultLineThickness = 1.0f;
			const FVector DefaultGridAxisU = FVector::ForwardVector;
			const FVector DefaultGridAxisV = FVector::RightVector;
			const FVector DefaultViewForward = FVector::ForwardVector;
			GridMaterial->SetParameterData("GridSize", &DefaultGridSize, 4);
			GridMaterial->SetParameterData("LineThickness", &DefaultLineThickness, 4);
			GridMaterial->SetParameterData("GridAxisU", &DefaultGridAxisU, sizeof(FVector));
			GridMaterial->SetParameterData("GridAxisV", &DefaultGridAxisV, sizeof(FVector));
			GridMaterial->SetParameterData("ViewForward", &DefaultViewForward, sizeof(FVector));
		}
	}
}

void FEditorViewportClient::Detach(FEngine* Engine, FRenderer* Renderer)
{
	Gizmo.EndDrag();
	EditorUI.DetachFromRenderer(Renderer);

	BlitRenderer.Release();

	GridMesh.reset();
	GridMaterial.reset();
}

void FEditorViewportClient::Tick(FEngine* Engine, float DeltaTime)
{
	IViewportClient::Tick(Engine, DeltaTime);
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	InputService.TickCameraNavigation(Engine, EditorEngine, ViewportRegistry, Gizmo);
}

void FEditorViewportClient::HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	InputService.HandleMessage(
		Engine,
		EditorEngine,
		Hwnd,
		Msg,
		WParam,
		LParam,
		ViewportRegistry,
		Picker,
		Gizmo,
		[this]()
		{
			EditorUI.SyncSelectedActorProperty();
		});
}

void FEditorViewportClient::HandleFileDoubleClick(const FString& FilePath)
{
	AssetInteractionService.HandleFileDoubleClick(EditorUI, ViewportRegistry, FilePath);
}

void FEditorViewportClient::HandleFileDropOnViewport(const FString& FilePath)
{
	AssetInteractionService.HandleFileDropOnViewport(
		EditorUI,
		Picker,
		ViewportRegistry,
		InputService.GetScreenMouseX(),
		InputService.GetScreenMouseY(),
		FilePath);
}

void FEditorViewportClient::BuildRenderCommands(FEngine* Engine, UScene* Scene, const FFrustum& Frustum, const FShowFlags& Flags, const FVector& CameraPosition, FRenderCommandQueue& OutQueue)
{
	if (!Engine)
	{
		return;
	}
	IViewportClient::BuildRenderCommands(Engine, Scene, Frustum, Flags, CameraPosition, OutQueue);
}

void FEditorViewportClient::Render(FEngine* Engine, FRenderer* Renderer)
{
	if (!Renderer)
	{
		return;
	}

	SyncViewportRectsFromDock();
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	RenderService.RenderAll(
		Engine,
		Renderer,
		EditorEngine,
		ViewportRegistry,
		EditorUI,
		Gizmo,
		BlitRenderer,
		WireFrameMaterial,
		GridMesh.get(),
		GridMaterial.get(),
		[this](FEngine* InEngine, UScene* Scene, const FFrustum& Frustum, const FShowFlags& Flags, const FVector& CameraPosition, FRenderCommandQueue& OutQueue)
		{
			BuildRenderCommands(InEngine, Scene, Frustum, Flags, CameraPosition, OutQueue);
		});
}

void FEditorViewportClient::SyncViewportRectsFromDock()
{
	RECT rc{};
	::GetClientRect(EditorEngine.GetRenderer()->GetHwnd(), &rc);

	const FRect NewRect{ rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top };

	static FRect CachedRect{ 0, 0, 0, 0 };
	if (CachedRect.X != NewRect.X ||
		CachedRect.Y != NewRect.Y ||
		CachedRect.Width != NewRect.Width ||
		CachedRect.Height != NewRect.Height)
	{
		CachedRect = NewRect;

		if (FSlateApplication* Slate = EditorEngine.GetSlateApplication())
		{
			Slate->SetViewportAreaRect(NewRect);
		}
	}
	FRect Central;
	if (!EditorUI.GetCentralDockRect(Central) || !Central.IsValid())
	{
		// First-frame fallback when dock rect is not ready.
		if (!ImGui::GetCurrentContext())
		{
			return;
		}
		ImGuiViewport* VP = ImGui::GetMainViewport();
		if (!VP || VP->WorkSize.x <= 0 || VP->WorkSize.y <= 0)
		{
			return;
		}
		// Convert viewport absolute coordinates to client coordinates.
		Central.X      = static_cast<int32>(VP->WorkPos.x - VP->Pos.x);
		Central.Y      = static_cast<int32>(VP->WorkPos.y - VP->Pos.y);
		Central.Width  = static_cast<int32>(VP->WorkSize.x);
		Central.Height = static_cast<int32>(VP->WorkSize.y);
	}
	
	FSlateApplication* Slate = EditorEngine.GetSlateApplication();
	if (Slate)
	{
		constexpr int32 HeaderHeight = 34;
		FRect ViewportArea = Central;
		if (ViewportArea.Height > HeaderHeight)
		{
			ViewportArea.Y += HeaderHeight;
			ViewportArea.Height -= HeaderHeight;
		}
		Slate->SetViewportAreaRect(ViewportArea);

		for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
		{
			Entry.bActive = Slate->IsViewportActive(Entry.Id);
		}
	}
}

