#include "EditorViewportClient.h"

#include "UI/EditorUI.h"
#include "Actor/Actor.h"
#include "Core/Core.h"
#include "Input/InputManager.h"
#include "Debug/EngineLog.h"
#include "Platform/Windows/Window.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/Materialmanager.h"
#include "Renderer/Material.h"
#include "Renderer/ShaderMap.h"
#include "Scene/Scene.h"
#include "Serializer/SceneSerializer.h"
#include "Component/PrimitiveComponent.h"
#include "Core/Paths.h"
#include "imgui.h"
#include "Actor/ObjActor.h"
#include "Actor/SkySphereActor.h"
#include <FEditorEngine.h>

CEditorViewportClient::CEditorViewportClient(FEditorEngine& InEditorEngine, CEditorUI& InEditorUI, CWindow* InMainWindow)
	: EditorEngine(InEditorEngine)
	, EditorUI(InEditorUI)
	, MainWindow(InMainWindow)
{
	InitializeEntries();
}

void CEditorViewportClient::Attach(CCore* Core, CRenderer* Renderer)
{
	if (!Core || !Renderer || !MainWindow)
	{
		return;
	}

	EditorUI.Initialize(Core);
	EditorUI.SetupWindow(MainWindow);
	EditorUI.AttachToRenderer(Renderer);

	// Wireframe 모드를 위한 머티리얼 가져와서 보관
	WireFrameMaterial = FMaterialManager::Get().FindByName(WireframeMaterialName);

	CreateGridResource(Renderer);
}

void CEditorViewportClient::CreateGridResource(CRenderer* Renderer)
{
	// 그리드 리소스 초기화
	ID3D11Device* Device = Renderer->GetDevice();
	if (Device)
	{
		// 그리드 메시 생성 (18개의 정점, SV_VertexID 호환용)
		GridMesh = std::make_unique<FMeshData>();
		GridMesh->Topology = EMeshTopology::EMT_TriangleList;
		for (int i = 0; i < 18; ++i)
		{
			FPrimitiveVertex v;
			GridMesh->Vertices.push_back(v);
			GridMesh->Indices.push_back(i);
		}
		GridMesh->CreateVertexAndIndexBuffer(Device);

		// 그리드 머티리얼 생성
		std::wstring ShaderDirW = FPaths::ShaderDir();
		std::wstring VSPath = ShaderDirW + L"AxisVertexShader.hlsl";
		std::wstring PSPath = ShaderDirW + L"AxisPixelShader.hlsl";
		auto VS = FShaderMap::Get().GetOrCreateVertexShader(Device, VSPath.c_str());
		auto PS = FShaderMap::Get().GetOrCreatePixelShader(Device, PSPath.c_str());

		GridMaterial = std::make_shared<FMaterial>();
		GridMaterial->SetOriginName("M_EditorGrid");
		GridMaterial->SetVertexShader(VS);
		GridMaterial->SetPixelShader(PS);

		FRasterizerStateOption rasterizerOption;
		rasterizerOption.FillMode = D3D11_FILL_SOLID;
		rasterizerOption.CullMode = D3D11_CULL_NONE;
		auto RS = Renderer->GetRenderStateManager()->GetOrCreateRasterizerState(rasterizerOption);
		GridMaterial->SetRasterizerOption(rasterizerOption);
		GridMaterial->SetRasterizerState(RS);

		FDepthStencilStateOption depthStencilOption;
		depthStencilOption.DepthEnable = true;
		depthStencilOption.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		auto DSS = Renderer->GetRenderStateManager()->GetOrCreateDepthStencilState(depthStencilOption);
		GridMaterial->SetDepthStencilOption(depthStencilOption);
		GridMaterial->SetDepthStencilState(DSS);

		// b2: Per-Material Constant Buffer (32 bytes)
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

void CEditorViewportClient::Detach(CCore* Core, CRenderer* Renderer)
{
	Gizmo.EndDrag();
	EditorUI.DetachFromRenderer(Renderer);

	GridMesh.reset();
	GridMaterial.reset();
}

void CEditorViewportClient::Tick(CCore* Core, float DeltaTime)
{
	if (!Core)
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

	IViewportClient::Tick(Core, DeltaTime);
}

void CEditorViewportClient::HandleMessage(CCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (!Core || !EditorUI.IsViewportInteractive())
	{
		return;
	}

	if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse && !EditorUI.IsViewportInteractive())
	{
		return;
	}

	UScene* Scene = ResolveScene(Core);
	AActor* SelectedActor = Core->GetSelectedActor();
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

	const bool bRightMouseDown = Core->GetInputManager() &&
		Core->GetInputManager()->IsMouseButtonDown(CInputManager::MOUSE_RIGHT);

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
			Core->SetSelectedActor(PickedActor);
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

void CEditorViewportClient::HandleFileDoubleClick(const FString& FilePath)
{
	CCore* Core = EditorUI.GetCore();

	if (Core)
	{
		if (FilePath.ends_with(".json"))
		{
			Core->SetSelectedActor(nullptr);
			Core->GetScene()->ClearActors();
			bool bLoaded = FSceneSerializer::Load(Core->GetScene(), FilePath, Core->GetRenderer()->GetDevice());

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
}

void CEditorViewportClient::HandleFileDropOnViewport(const FString& FilePath)
{
	CCore* Core = EditorUI.GetCore();

	if (Core && Core->GetRenderer())
	{
		if (FilePath.ends_with(".obj"))
		{
			const FRay Ray = Picker.ScreenToRay(Core->GetScene()->GetCamera(), ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight);

			AObjActor* NewActor = Core->GetScene()->SpawnActor<AObjActor>("ObjActor");
			
			NewActor->LoadObj(Core->GetRenderer()->GetDevice(), FPaths::ToRelativePath(FilePath));
			FVector V = Ray.Origin + Ray.Direction * 5;
			NewActor->SetActorLocation(V);
		}
	}
}

void CEditorViewportClient::BuildRenderCommands(CCore* Core, UScene* Scene,
	const FFrustum& Frustum, FRenderCommandQueue& OutQueue)
{
	IViewportClient::BuildRenderCommands(Core, Scene, Frustum, OutQueue);  // non-const 부모 호출

	// RenderMode 처리
	if (RenderMode == ERenderMode::Wireframe)
	{
		for (auto it = OutQueue.Commands.begin(); it != OutQueue.Commands.end(); it++)
		{
			// TODO: 아래의 if문 삭제하고 UUID 렌더러를 컴포넌트가 아닌 EditorViewportClient의 기능으로 재구현
			if(it->RenderLayer != ERenderLayer::Overlay)
				it->Material = WireFrameMaterial.get();
		}
	}

	if (!Core || !Scene || !Scene->GetCamera())
	{
		return;
	}

	// 그리드(Axis) 명령 삽입
	if (GridMesh && GridMaterial&&bShowGrid)
	{
		FRenderCommand GridCmd;
		GridCmd.MeshData = GridMesh.get();
		GridCmd.Material = GridMaterial.get();
		GridCmd.WorldMatrix = FMatrix::Identity;
		GridCmd.RenderLayer = ERenderLayer::Default;
		OutQueue.AddCommand(GridCmd);
	}

	AActor* GizmoTarget = Core->GetSelectedActor();
	if (GizmoTarget && !GizmoTarget->IsA<ASkySphereActor>())
	{
		Gizmo.BuildRenderCommands(GizmoTarget, Scene->GetCamera(), OutQueue);
	}
}

void CEditorViewportClient::SetGridSize(float InSize)
{
	GridSize = InSize;
	if (GridMaterial)
	{
		GridMaterial->SetParameterData("GridSize",&GridSize, 4);
	}
}

void CEditorViewportClient::SetLineThickness(float InThickness)
{
	LineThickness = InThickness;
	if (GridMaterial)
	{
		GridMaterial->SetParameterData("LineThickness", &LineThickness, 4);
	}
}

void CEditorViewportClient::Render(CCore* Core, CRenderer* Renderer)
{
	if (!Renderer)
	{
		return;
	}

	ID3D11Device* Device = Renderer->GetDevice();
	ID3D11DeviceContext* Context = Renderer->GetDeviceContext();
	if (!Device || !Context)
	{
		return;
	}

	const float Colors[4][4] =
	{
		{ 0.25f, 0.10f, 0.10f, 1.0f },
		{ 0.10f, 0.25f, 0.10f, 1.0f },
		{ 0.10f, 0.10f, 0.25f, 1.0f },
		{ 0.25f, 0.25f, 0.10f, 1.0f },
	};

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

		Context->OMSetRenderTargets(1, &RTV, DSV);

		const auto& Rect = Entry.Viewport->GetRect();
		D3D11_VIEWPORT VP = {};
		VP.TopLeftX = 0.0f;
		VP.TopLeftY = 0.0f;
		VP.Width = static_cast<float>(Rect.Width);
		VP.Height = static_cast<float>(Rect.Height);
		VP.MinDepth = 0.0f;
		VP.MaxDepth = 1.0f;

		Context->RSSetViewports(1, &VP);
		Context->ClearRenderTargetView(RTV, Colors[i % 4]);
		Context->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}
}

void CEditorViewportClient::InitializeEntries()
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
	AddEntry(1, EViewportType::OrthoTop, 1);
	AddEntry(2, EViewportType::OrthoFront, 2);
	AddEntry(3, EViewportType::OrthoRight, 3);
}
