#include "EditorViewportClient.h"

#include "EditorEngine.h"
#include "UI/EditorUI.h"
#include "Actor/Actor.h"
#include "Actor/ObjActor.h"
#include "Actor/SkySphereActor.h"
#include "Component/PrimitiveComponent.h"
#include "Core/Engine.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Input/InputManager.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/ShaderMap.h"
#include "Scene/Scene.h"
#include "Serializer/SceneSerializer.h"
#include "imgui.h"
#include "Actor/ObjActor.h"
#include "Actor/SkySphereActor.h"
#include <EditorEngine.h>
#include "Viewport.h"

FEditorViewportClient::FEditorViewportClient(FEditorEngine& InEditorEngine, FEditorUI& InEditorUI, FWindowsWindow* InMainWindow)
	: EditorEngine(InEditorEngine)
	, EditorUI(InEditorUI)
	, MainWindow(InMainWindow)
{
	InitializeEntries();
}

void FEditorViewportClient::Attach(FEngine* Engine, FRenderer* Renderer)
{
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	if (!EditorEngine || !Renderer)
	{
		return;
	}

	EditorUI.Initialize(EditorEngine);
	EditorUI.AttachToRenderer(Renderer);

	BlitRenderer.Initialize(Renderer->GetDevice());

	// Wireframe 모드를 위한 머티리얼 가져와서 보관
	WireFrameMaterial = FMaterialManager::Get().FindByName(WireframeMaterialName);
	CreateGridResource(Renderer);
}

void FEditorViewportClient::CreateGridResource(FRenderer* Renderer)
{
	ID3D11Device* Device = Renderer->GetDevice();
	if (Device)
	{
		GridMesh = std::make_unique<FMeshData>();
		GridMesh->Topology = EMeshTopology::EMT_TriangleList;
		for (int i = 0; i < 18; ++i)
		{
			FPrimitiveVertex Vertex;
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

		int32 SlotIndex = GridMaterial->CreateConstantBuffer(Device, 32);
		if (SlotIndex >= 0)
		{
			GridMaterial->RegisterParameter("GridSize", SlotIndex, 12, 4);
			GridMaterial->RegisterParameter("LineThickness", SlotIndex, 16, 4);

			GridMaterial->SetParameterData("GridSize", &GridSize, 4);
			GridMaterial->SetParameterData("LineThickness", &LineThickness, 4);
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
	if (!Engine)
	{
		return;
	}

	if (ImGui::GetCurrentContext())
	{
		const ImGuiIO& IO = ImGui::GetIO();
		if ((IO.WantCaptureKeyboard || IO.WantCaptureMouse) && !EditorUI.IsViewportInteractive())
		{
			return;
		}
	}

	if (!EditorUI.IsViewportInteractive())
	{
		return;
	}

	IViewportClient::Tick(Engine, DeltaTime);
}

void FEditorViewportClient::HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (!Engine || !EditorUI.IsViewportInteractive())
	{
		return;
	}

	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	if (!EditorEngine)
	{
		return;
	}

	if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse && !EditorUI.IsViewportInteractive())
	{
		return;
	}

	UScene* Scene = ResolveScene(Engine);
	AActor* SelectedActor = EditorEngine->GetSelectedActor();
	if (!Scene)
	{
		return;
	}

	const bool bHasViewportMouse = EditorUI.GetViewportMousePosition(
		static_cast<int32>(static_cast<short>(LOWORD(LParam))),
		static_cast<int32>(static_cast<short>(HIWORD(LParam))),
		ScreenMouseX,
		ScreenMouseY,
		ScreenWidth,
		ScreenHeight);

	const bool bRightMouseDown = Engine->GetInputManager() &&
		Engine->GetInputManager()->IsMouseButtonDown(FInputManager::MOUSE_RIGHT);

	switch (Msg)
	{
	case WM_KEYDOWN:
		if (bRightMouseDown)
		{
			return;
		}

		switch (WParam)
		{
		case 'W':
			Gizmo.SetMode(EGizmoMode::Location);
			return;

		case 'E':
			Gizmo.SetMode(EGizmoMode::Rotation);
			return;

		case 'R':
			Gizmo.SetMode(EGizmoMode::Scale);
			return;

		case 'L':
			Gizmo.ToggleCoordinateSpace();
			UE_LOG("Gizmo Space: %s", Gizmo.GetCoordinateSpace() == EGizmoCoordinateSpace::Local ? "Local" : "World");
			return;

		default:
			return;
		}

	case WM_LBUTTONDOWN:
		if (!bHasViewportMouse)
		{
			return;
		}

		if (SelectedActor && Gizmo.BeginDrag(SelectedActor, Scene, Picker, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight))
		{
			return;
		}

		{
			AActor* PickedActor = Picker.PickActor(Scene, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight);
			EditorEngine->SetSelectedActor(PickedActor);
			EditorUI.SyncSelectedActorProperty();
		}
		return;

	case WM_MOUSEMOVE:
		if (!bHasViewportMouse)
		{
			Gizmo.ClearHover();
			return;
		}

		if (!Gizmo.IsDragging())
		{
			Gizmo.UpdateHover(SelectedActor, Scene, Picker, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight);
			return;
		}

		if (Gizmo.UpdateDrag(SelectedActor, Scene, Picker, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight))
		{
			EditorUI.SyncSelectedActorProperty();
		}
		return;

	case WM_LBUTTONUP:
		if (Gizmo.IsDragging())
		{
			Gizmo.EndDrag();
			if (bHasViewportMouse)
			{
				Gizmo.UpdateHover(SelectedActor, Scene, Picker, ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight);
			}
			else
			{
				Gizmo.ClearHover();
			}
			EditorUI.SyncSelectedActorProperty();
		}
		return;

	default:
		return;
	}
}

void FEditorViewportClient::HandleFileDoubleClick(const FString& FilePath)
{
	FEditorEngine* Engine = EditorUI.GetEngine();

	if (Engine && FilePath.ends_with(".json"))
	{
		Engine->SetSelectedActor(nullptr);
		Engine->GetScene()->ClearActors();
		bool bLoaded = FSceneSerializer::Load(Engine->GetScene(), FilePath, Engine->GetRenderer()->GetDevice());

		if (bLoaded)
		{
			UE_LOG("Scene loaded: %s", FilePath.c_str());
		}
		else
		{
			MessageBoxW(
				nullptr,
				L"Scene 정보가 잘못되었습니다.",
				L"Error",
				MB_OK | MB_ICONWARNING
			);
		}
	}
}

void FEditorViewportClient::HandleFileDropOnViewport(const FString& FilePath)
{
	FEditorEngine* Engine = EditorUI.GetEngine();

	if (Engine && Engine->GetRenderer() && FilePath.ends_with(".obj"))
	{
		const FRay Ray = Picker.ScreenToRay(Engine->GetScene()->GetCamera(), ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight);

		AObjActor* NewActor = Engine->GetScene()->SpawnActor<AObjActor>("ObjActor");
		NewActor->LoadObj(Engine->GetRenderer()->GetDevice(), FPaths::ToRelativePath(FilePath));
		FVector SpawnLocation = Ray.Origin + Ray.Direction * 5;
		NewActor->SetActorLocation(SpawnLocation);
	}
}

void FEditorViewportClient::BuildRenderCommands(FEngine* Engine, UScene* Scene, const FFrustum& Frustum, FRenderCommandQueue& OutQueue)
{
	IViewportClient::BuildRenderCommands(Engine, Scene, Frustum, OutQueue);

	if (RenderMode == ERenderMode::Wireframe)
	{
		for (auto It = OutQueue.Commands.begin(); It != OutQueue.Commands.end(); ++It)
		{
			if (It->RenderLayer != ERenderLayer::Overlay)
			{
				It->Material = WireFrameMaterial.get();
			}
		}
	}

	if (!Engine || !Scene || !Scene->GetCamera())
	{
		return;
	}

	if (GridMesh && GridMaterial && bShowGrid)
	{
		FRenderCommand GridCommand;
		GridCommand.MeshData = GridMesh.get();
		GridCommand.Material = GridMaterial.get();
		GridCommand.WorldMatrix = FMatrix::Identity;
		GridCommand.RenderLayer = ERenderLayer::Default;
		OutQueue.AddCommand(GridCommand);
	}

	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	AActor* GizmoTarget = EditorEngine ? EditorEngine->GetSelectedActor() : nullptr;
	if (GizmoTarget && !GizmoTarget->IsA<ASkySphereActor>())
	{
		Gizmo.BuildRenderCommands(GizmoTarget, Scene->GetCamera(), OutQueue);
	}
}

void FEditorViewportClient::SetGridSize(float InSize)
{
	GridSize = InSize;
	if (GridMaterial)
	{
		GridMaterial->SetParameterData("GridSize", &GridSize, 4);
	}
}

void FEditorViewportClient::SetLineThickness(float InThickness)
{
	LineThickness = InThickness;
	if (GridMaterial)
	{
		GridMaterial->SetParameterData("LineThickness", &LineThickness, 4);
	}
}

void FEditorViewportClient::Render(FEngine* Engine, FRenderer* Renderer)
{
	if (!Renderer)
	{
		return;
	}

	SyncViewportRectsFromDock();

	ID3D11Device* Device = Renderer->GetDevice();
	ID3D11DeviceContext* Context = Renderer->GetDeviceContext();
	if (!Device || !Context)
	{
		return;
	}

	UScene* Scene = ResolveScene(Engine);

	if (!Scene)
	{
		return;
	}

	constexpr float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

	for (int32 i = 0; i < static_cast<int32>(Entries.size()); ++i)
	{
		FViewportEntry& Entry = Entries[i];
		if (!Entry.bActive || !Entry.Viewport)
		{
			continue;
		}

		Entry.Viewport->EnsureResources(Device);

		ID3D11RenderTargetView* RTV = Entry.Viewport->GetRTV();
		ID3D11DepthStencilView* DSV = Entry.Viewport->GetDSV();
		if (!RTV || !DSV)
		{
			continue;
		}

		const auto& Rect = Entry.Viewport->GetRect();
		D3D11_VIEWPORT VP = {};
		VP.TopLeftX = 0.0f;
		VP.TopLeftY = 0.0f;
		VP.Width = static_cast<float>(Rect.Width);
		VP.Height = static_cast<float>(Rect.Height);
		VP.MinDepth = 0.0f;
		VP.MaxDepth = 1.0f;
		
		Context->ClearRenderTargetView(RTV, ClearColor);
		Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		Renderer->BeginScenePass(RTV, DSV, VP);

		const float AspectRatio = static_cast<float>(Rect.Width) / static_cast<float>(Rect.Height);
		FRenderCommandQueue Queue;
		Queue.Reserve(Renderer->GetPrevCommandCount());
		Queue.ProjectionMatrix = Entry.LocalState.BuildProjMatrix(AspectRatio);
		Queue.ViewMatrix = Entry.LocalState.BuildViewMatrix();

		FFrustum ffrustum;
		ffrustum.ExtractFromVP(Queue.ViewMatrix * Queue.ProjectionMatrix);
		BuildRenderCommands(Engine, Scene, ffrustum, Queue);
		Renderer->SubmitCommands(Queue);
		Renderer->ExecuteCommands();
		Renderer->EndScenePass();
	}

	Renderer->BindSwapChainRTV();

	BlitRenderer.BlitAll(Context, Entries);

	EditorUI.Render();
}

void FEditorViewportClient::SyncViewportRectsFromDock()
{
	FRect Central;
	if (!EditorUI.GetCentralDockRect(Central) || !Central.IsValid())
	{
		// 첫 프레임 fallback
		if (!ImGui::GetCurrentContext())
		{
			return;
		}
		ImGuiViewport* VP = ImGui::GetMainViewport();
		if (!VP || VP->WorkSize.x <= 0 || VP->WorkSize.y <= 0)
		{
			return;
		}
		// WorkPos도 절대 좌표이므로 창 위치(Pos)를 빼서 클라이언트 좌표로
		Central.X      = static_cast<int32>(VP->WorkPos.x - VP->Pos.x);
		Central.Y      = static_cast<int32>(VP->WorkPos.y - VP->Pos.y);
		Central.Width  = static_cast<int32>(VP->WorkSize.x);
		Central.Height = static_cast<int32>(VP->WorkSize.y);
	}

	if (Entries.empty())
	{
		return;
	}

	// 활성 뷰포트 수에 따라 분할
	int32 ActiveCount = 0;
	for (const FViewportEntry& Entry : Entries)
	{
		if (Entry.bActive) ++ActiveCount;
	}

	// 분할 rect 계산: 1개면 전체, 2개면 좌/우, 4개면 2x2
	const int32 HalfW = Central.Width  / 2;
	const int32 HalfH = Central.Height / 2;

	FRect SubRects[4];
	if (ActiveCount <= 1)
	{
		SubRects[0] = Central;
	}
	else if (ActiveCount == 2)
	{
		SubRects[0] = { Central.X,          Central.Y, HalfW, Central.Height };
		SubRects[1] = { Central.X + HalfW,  Central.Y, HalfW, Central.Height };
	}
	else // 3 or 4
	{
		SubRects[0] = { Central.X,         Central.Y,          HalfW, HalfH };
		SubRects[1] = { Central.X + HalfW, Central.Y,          HalfW, HalfH };
		SubRects[2] = { Central.X,         Central.Y + HalfH,  HalfW, HalfH };
		SubRects[3] = { Central.X + HalfW, Central.Y + HalfH,  HalfW, HalfH };
	}

	int32 Idx = 0;
	for (FViewportEntry& Entry : Entries)
	{
		if (!Entry.bActive || !Entry.Viewport)
		{
			continue;
		}
		Entry.Viewport->SetRect(SubRects[Idx]);
		++Idx;
	}
}

void FEditorViewportClient::InitializeEntries()
{
	Entries.clear();
	Entries.reserve(4);

	auto AddEntry = [this](FViewportId Id, EViewportType Type, int32 SlotIndex)
		{
			FViewportEntry Entry;
			Entry.Id = Id;
			Entry.Type = Type;
			Entry.Viewport = &EditorEngine.GetViewports()[SlotIndex];
			Entry.bActive = true;
			Entry.LocalState = FViewportLocalState::CreateDefault(Type);
			Entries.push_back(Entry);
		};

	AddEntry(0, EViewportType::Perspective, 0);
	AddEntry(1, EViewportType::Perspective, 1);
	AddEntry(2, EViewportType::Perspective, 2);
	AddEntry(3, EViewportType::Perspective, 3);
}
