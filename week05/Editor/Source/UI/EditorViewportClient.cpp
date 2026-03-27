#include "EditorViewportClient.h"

#include "EditorUI.h"
#include "Actor/Actor.h"
#include "Actor/ObjActor.h"
#include "Actor/SkySphereActor.h"
#include "Component/PrimitiveComponent.h"
#include "Core/Engine.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Input/InputManager.h"
#include "Platform/Windows/WindowsWindow.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderStateManager.h"
#include "Renderer/ShaderMap.h"
#include "Scene/Scene.h"
#include "Serializer/SceneSerializer.h"
#include "imgui.h"

CEditorViewportClient::CEditorViewportClient(CEditorUI& InEditorUI, FWindowsWindow* InMainWindow)
	: EditorUI(InEditorUI)
	, MainWindow(InMainWindow)
{
}

void CEditorViewportClient::Attach(FEngine* Engine, CRenderer* Renderer)
{
	if (!Engine || !Renderer || !MainWindow)
	{
		return;
	}

	EditorUI.Initialize(Engine);
	EditorUI.SetupWindow(MainWindow);
	EditorUI.AttachToRenderer(Renderer);

	WireFrameMaterial = FMaterialManager::Get().FindByName(WireframeMaterialName);
	CreateGridResource(Renderer);
}

void CEditorViewportClient::CreateGridResource(CRenderer* Renderer)
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

void CEditorViewportClient::Detach(FEngine* Engine, CRenderer* Renderer)
{
	Gizmo.EndDrag();
	EditorUI.DetachFromRenderer(Renderer);

	GridMesh.reset();
	GridMaterial.reset();
}

void CEditorViewportClient::Tick(FEngine* Engine, float DeltaTime)
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

void CEditorViewportClient::HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (!Engine || !EditorUI.IsViewportInteractive())
	{
		return;
	}

	if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse && !EditorUI.IsViewportInteractive())
	{
		return;
	}

	UScene* Scene = ResolveScene(Engine);
	AActor* SelectedActor = Engine->GetSelectedActor();
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
		Engine->GetInputManager()->IsMouseButtonDown(CInputManager::MOUSE_RIGHT);

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
			Engine->SetSelectedActor(PickedActor);
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
	FEngine* Engine = EditorUI.GetEngine();

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

void CEditorViewportClient::HandleFileDropOnViewport(const FString& FilePath)
{
	FEngine* Engine = EditorUI.GetEngine();

	if (Engine && Engine->GetRenderer() && FilePath.ends_with(".obj"))
	{
		const FRay Ray = Picker.ScreenToRay(Engine->GetScene()->GetCamera(), ScreenMouseX, ScreenMouseY, ScreenWidth, ScreenHeight);

		AObjActor* NewActor = Engine->GetScene()->SpawnActor<AObjActor>("ObjActor");
		NewActor->LoadObj(Engine->GetRenderer()->GetDevice(), FPaths::ToRelativePath(FilePath));
		FVector SpawnLocation = Ray.Origin + Ray.Direction * 5;
		NewActor->SetActorLocation(SpawnLocation);
	}
}

void CEditorViewportClient::BuildRenderCommands(FEngine* Engine, UScene* Scene, const FFrustum& Frustum, FRenderCommandQueue& OutQueue)
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

	AActor* GizmoTarget = Engine->GetSelectedActor();
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
		GridMaterial->SetParameterData("GridSize", &GridSize, 4);
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
