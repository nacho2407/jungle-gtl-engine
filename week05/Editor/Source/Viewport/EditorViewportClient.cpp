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
#include "Camera/Camera.h"
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

			float DefaultGridSize = 10.0f;
			float DefaultLineThickness = 1.0f;
			GridMaterial->SetParameterData("GridSize", &DefaultGridSize, 4);
			GridMaterial->SetParameterData("LineThickness", &DefaultLineThickness, 4);
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

	FSlateApplication* Slate = EditorUI.GetEngine()->GetSlateApplication();
	if (!Slate || Slate->GetFocusedViewportId() == INVALID_VIEWPORT_ID)
	{
		return;
	}

	if (ImGui::GetCurrentContext())
	{
		const ImGuiIO& IO = ImGui::GetIO();
		if (IO.WantCaptureKeyboard || IO.WantCaptureMouse)
		{
			return;
		}
	}

	IViewportClient::Tick(Engine, DeltaTime);

	FInputManager* Input = Engine->GetInputManager();
	if (!Input || !Input->IsMouseButtonDown(FInputManager::MOUSE_RIGHT) || Gizmo.IsDragging())
	{
		return;
	}

	FViewportEntry* FocusedEntry = FindEntryByViewportID(Slate->GetFocusedViewportId());
	if (!FocusedEntry)
	{
		return;
	}

	const float DeltaX = Input->GetMouseDeltaX();
	const float DeltaY = Input->GetMouseDeltaY();

	if (FocusedEntry->LocalState.ProjectionType == EViewportType::Perspective)
	{
		float Sensitivity = 0.2f;
		if (FCamera* Cam = Engine->GetScene()->GetCamera())
			Sensitivity = Cam->GetMouseSensitivity();

		FocusedEntry->LocalState.Rotation.Yaw   += DeltaX * Sensitivity;
		FocusedEntry->LocalState.Rotation.Pitch -= DeltaY * Sensitivity;
		if (FocusedEntry->LocalState.Rotation.Pitch >  89.0f) FocusedEntry->LocalState.Rotation.Pitch =  89.0f;
		if (FocusedEntry->LocalState.Rotation.Pitch < -89.0f) FocusedEntry->LocalState.Rotation.Pitch = -89.0f;
	}
	else
	{
		// Ortho pan: ViewRight/Up는 뷰포트 타입에서 결정
		FVector ViewFwd, ViewUp;
		switch (FocusedEntry->LocalState.ProjectionType)
		{
		case EViewportType::OrthoTop:
			ViewFwd = FVector(0, 0, -1);
			ViewUp  = FVector(1, 0,  0);
			break;
		case EViewportType::OrthoFront:
			ViewFwd = FVector(-1, 0, 0);
			ViewUp  = FVector( 0, 0, 1);
			break;
		case EViewportType::OrthoRight:
			ViewFwd = FVector(0, -1, 0);
			ViewUp  = FVector(0,  0, 1);
			break;
		default:
			return;
		}
		const FVector ViewRight = FVector::CrossProduct(ViewUp, ViewFwd).GetSafeNormal();
		const float PanSpeed = FocusedEntry->LocalState.OrthoZoom * 0.002f;
		FocusedEntry->LocalState.OrthoTarget -= ViewRight * DeltaX * PanSpeed;
		FocusedEntry->LocalState.OrthoTarget += ViewUp    * DeltaY * PanSpeed;
	}
}

void FEditorViewportClient::HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	if (!Engine || !EditorEngine)
		return;

	FSlateApplication* Slate = EditorEngine->GetSlateApplication();
	if (!Slate)
		return;

	const int32 MouseX = static_cast<int32>(static_cast<short>(LOWORD(LParam)));
	const int32 MouseY = static_cast<int32>(static_cast<short>(HIWORD(LParam)));

	// 1. Slate 마우스 이벤트 전달: FocusedViewportId 갱신 + SSplitter 드래그 처리
	switch (Msg)
	{
	case WM_LBUTTONDOWN: Slate->ProcessMouseDown(MouseX, MouseY); break;
	case WM_RBUTTONDOWN: Slate->ProcessMouseDown(MouseX, MouseY); break;
	case WM_MOUSEMOVE:   Slate->ProcessMouseMove(MouseX, MouseY); break;
	case WM_LBUTTONUP:   Slate->ProcessMouseUp(MouseX, MouseY);   break;
	default: break;
	}

	// Ortho zoom (휠) — Splitter/ImGui 차단 전에 먼저 처리
	if (Msg == WM_MOUSEWHEEL)
	{
		if (!ImGui::GetCurrentContext() || !ImGui::GetIO().WantCaptureMouse)
		{
			FViewportEntry* FocusedEntry = FindEntryByViewportID(Slate->GetFocusedViewportId());
			if (FocusedEntry && FocusedEntry->LocalState.ProjectionType != EViewportType::Perspective)
			{
				const float WheelDelta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(WParam)) / WHEEL_DELTA;
				FocusedEntry->LocalState.OrthoZoom *= (1.0f - WheelDelta * 0.1f);
				if (FocusedEntry->LocalState.OrthoZoom <     1.0f) FocusedEntry->LocalState.OrthoZoom =     1.0f;
				if (FocusedEntry->LocalState.OrthoZoom > 10000.0f) FocusedEntry->LocalState.OrthoZoom = 10000.0f;
			}
		}
		return;
	}

	// 2. SSplitter 드래그 중 → 뷰포트 로직 전부 차단
	if (Slate->IsDraggingSplitter())
		return;

	// 3. ImGui 마우스 캡처 → 차단
	if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)
		return;

	UScene* Scene = ResolveScene(Engine);
	AActor* SelectedActor = EditorEngine->GetSelectedActor();
	if (!Scene)
		return;

	const bool bRightMouseDown = Engine->GetInputManager() &&
		Engine->GetInputManager()->IsMouseButtonDown(FInputManager::MOUSE_RIGHT);
	FViewportEntry* Entry = FindEntryByViewportID(Slate->GetFocusedViewportId());

	switch (Msg)
	{
	case WM_KEYDOWN:
	{
		if (Slate->GetFocusedViewportId() == INVALID_VIEWPORT_ID || bRightMouseDown)
			return;
		switch (WParam)
		{
		case 'W': Gizmo.SetMode(EGizmoMode::Location);   return;
		case 'E': Gizmo.SetMode(EGizmoMode::Rotation);   return;
		case 'R': Gizmo.SetMode(EGizmoMode::Scale);      return;
		case 'L':
			Gizmo.ToggleCoordinateSpace();
			UE_LOG("Gizmo Space: %s", Gizmo.GetCoordinateSpace() == EGizmoCoordinateSpace::Local ? "Local" : "World");
			return;
		default: return;
		}
	}

	case WM_LBUTTONDOWN:
	{
		// ProcessMouseDown에서 FocusedViewportId가 방금 갱신됨
		FViewport* VP = GetViewportById(Slate->GetFocusedViewportId());
		if (!VP) return;
		const FRect& R = VP->GetRect();
		ScreenMouseX = MouseX - R.X;
		ScreenMouseY = MouseY - R.Y;

		if (SelectedActor && Gizmo.BeginDrag(SelectedActor, Entry, Picker, ScreenMouseX, ScreenMouseY))
			return;

		AActor* PickedActor = Picker.PickActor(Scene, Entry, ScreenMouseX, ScreenMouseY);
		EditorEngine->SetSelectedActor(PickedActor);
		EditorUI.SyncSelectedActorProperty();
		return;
	}

	case WM_MOUSEMOVE:
	{
		FViewport* VP = GetViewportById(Slate->GetHoveredViewportId());
		if (!VP)
		{
			Gizmo.ClearHover();
			return;
		}
		FViewportEntry* HoveredEntry = FindEntryByViewportID(Slate->GetHoveredViewportId());
		const FRect& R = VP->GetRect();
		ScreenMouseX = MouseX - R.X;
		ScreenMouseY = MouseY - R.Y;

		if (!Gizmo.IsDragging())
		{
			Gizmo.UpdateHover(SelectedActor, HoveredEntry, Picker, ScreenMouseX, ScreenMouseY);
			return;
		}
		if (Gizmo.UpdateDrag(SelectedActor, HoveredEntry, Picker, ScreenMouseX, ScreenMouseY))
			EditorUI.SyncSelectedActorProperty();
		return;
	}

	case WM_LBUTTONUP:
	{
		if (!Gizmo.IsDragging()) return;
		Gizmo.EndDrag();
		FViewport* VP = GetViewportById(Slate->GetHoveredViewportId());
		if (VP)
		{
			FViewportEntry* HoveredEntry = FindEntryByViewportID(Slate->GetHoveredViewportId());
			const FRect& R = VP->GetRect();
			ScreenMouseX = MouseX - R.X;
			ScreenMouseY = MouseY - R.Y;
			Gizmo.UpdateHover(SelectedActor, HoveredEntry, Picker, ScreenMouseX, ScreenMouseY);
		}
		else
		{
			Gizmo.ClearHover();
		}
		EditorUI.SyncSelectedActorProperty();
		return;
	}

	default:
		return;
	}
}

FViewport* FEditorViewportClient::GetViewportById(FViewportId Id) const
{
	if (Id == INVALID_VIEWPORT_ID) return nullptr;
	for (const FViewportEntry& Entry : Entries)
	{
		if (Entry.Id == Id && Entry.bActive && Entry.Viewport)
			return Entry.Viewport;
	}
	return nullptr;
}

const FViewportEntry* FEditorViewportClient::FindEntryByViewportID(FViewportId ViewportId) const
{
	for (const FViewportEntry& Entry : Entries)
	{
		if (Entry.Id == ViewportId)
			return &Entry;
	}
	return nullptr;
}

FViewportEntry* FEditorViewportClient::FindEntryByViewportID(FViewportId ViewportId)
{
	for (FViewportEntry& Entry : Entries)
	{
		if (Entry.Id == ViewportId)
			return &Entry;
	}
	return nullptr;
}

FViewportEntry* FEditorViewportClient::FindEntryByType(EViewportType Type)
{
	for (FViewportEntry& Entry : Entries)
	{
		if (Entry.LocalState.ProjectionType == Type)
			return &Entry;
	}
	return nullptr;
}

const FViewportEntry* FEditorViewportClient::FindEntryByType(EViewportType Type) const
{
	for (const FViewportEntry& Entry : Entries)
	{
		if (Entry.LocalState.ProjectionType == Type)
			return &Entry;
	}
	return nullptr;
}

void FEditorViewportClient::HandleFileDoubleClick(const FString& FilePath)
{
	FEditorEngine* Engine = EditorUI.GetEngine();

	if (Engine && FilePath.ends_with(".json"))
	{
		Engine->SetSelectedActor(nullptr);
		Engine->GetScene()->ClearActors();

		FCameraSerializeData CameraData;
		bool bLoaded = FSceneSerializer::Load(Engine->GetScene(), FilePath,
		                                      Engine->GetRenderer()->GetDevice(), &CameraData);

		if (bLoaded)
		{
			if (CameraData.bValid)
			{
				FViewportEntry* PerspEntry = FindEntryByType(EViewportType::Perspective);
				if (PerspEntry)
				{
					PerspEntry->LocalState.Position = CameraData.Location;
					PerspEntry->LocalState.Rotation = CameraData.Rotation;
					PerspEntry->LocalState.FovY     = CameraData.FOV;
					PerspEntry->LocalState.NearPlane = CameraData.NearClip;
					PerspEntry->LocalState.FarPlane  = CameraData.FarClip;
				}
			}
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
	FViewportEntry* Entry = FindEntryByViewportID(Engine->GetSlateApplication()->GetFocusedViewportId());

	if (Engine && Engine->GetRenderer() && FilePath.ends_with(".obj"))
	{
		const FRay Ray = Picker.ScreenToRay(*Entry, ScreenMouseX, ScreenMouseY);

		AObjActor* NewActor = Engine->GetScene()->SpawnActor<AObjActor>("ObjActor");
		NewActor->LoadObj(Engine->GetRenderer()->GetDevice(), FPaths::ToRelativePath(FilePath));
		FVector SpawnLocation = Ray.Origin + Ray.Direction * 5;
		NewActor->SetActorLocation(SpawnLocation);
	}
}

void FEditorViewportClient::BuildRenderCommands(FEngine* Engine, UScene* Scene, const FFrustum& Frustum, const FShowFlags& Flags, FRenderCommandQueue& OutQueue)
{
	if (!Engine)
	{
		return;
	}
	IViewportClient::BuildRenderCommands(Engine, Scene, Frustum, Flags, OutQueue);
}

static void ApplyWireframe(FRenderCommandQueue& Queue, FMaterial* WireMat)
{
	for (auto& Cmd : Queue.Commands)
	{
		if (Cmd.RenderLayer != ERenderLayer::Overlay)
			Cmd.Material = WireMat;
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
		BuildRenderCommands(Engine, Scene, ffrustum, Entry.LocalState.ShowFlags, Queue);

		{
			FEditorEngine* EditorEng = static_cast<FEditorEngine*>(Engine);
			AActor* GizmoTarget = EditorEng ? EditorEng->GetSelectedActor() : nullptr;
			if (GizmoTarget && !GizmoTarget->IsA<ASkySphereActor>())
			{
				Gizmo.BuildRenderCommands(GizmoTarget, &Entry, Queue);
			}
		}

		if (Entry.LocalState.ViewMode == ERenderMode::Wireframe && WireFrameMaterial)
			ApplyWireframe(Queue, WireFrameMaterial.get());

		if (Entry.LocalState.bShowGrid && GridMesh && GridMaterial)
		{
			GridMaterial->SetParameterData("GridSize", &Entry.LocalState.GridSize,       4);
			GridMaterial->SetParameterData("LineThickness", &Entry.LocalState.LineThickness,  4);
			FRenderCommand GridCmd;
			GridCmd.MeshData    = GridMesh.get();
			GridCmd.Material    = GridMaterial.get();
			GridCmd.WorldMatrix = FMatrix::Identity;
			GridCmd.RenderLayer = ERenderLayer::Default;
			Queue.AddCommand(GridCmd);
		}

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

	FSlateApplication* Slate = EditorEngine.GetSlateApplication();
	if (Slate)
	{
		Slate->SetViewportAreaRect(Central);
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
			Entry.Viewport = &EditorEngine.GetViewports()[SlotIndex];
			Entry.bActive = true;
			Entry.LocalState = FViewportLocalState::CreateDefault(Type);
			Entries.push_back(Entry);
		};

	AddEntry(0, EViewportType::Perspective, 0);
	AddEntry(1, EViewportType::OrthoTop, 1);
	AddEntry(2, EViewportType::OrthoRight, 2);
	AddEntry(3, EViewportType::OrthoFront, 3);
}
