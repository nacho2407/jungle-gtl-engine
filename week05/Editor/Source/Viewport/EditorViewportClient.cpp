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

	// 에디터 UI와 뷰포트 렌더링에 필요한 공용 리소스를 이 시점에 준비한다.
	EditorUI.Initialize(EditorEngine);
	EditorUI.InitializeRendererResources(Renderer);
	WireFrameMaterial = FMaterialManager::Get().FindByName(WireframeMaterialName);
	CreateGridResource(Renderer);
}

void FEditorViewportClient::CreateGridResource(FRenderer* Renderer)
{
	ID3D11Device* Device = Renderer->GetDevice();
	if (Device)
	{
		// 에디터 그리드는 뷰포트마다 동적 파라미터만 달라지는 공용 메시/머티리얼 구조를 사용한다.
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
			for (int32 i = 0; i < MAX_VIEWPORTS; ++i)
			{
				// 실제 그리드 방향과 뷰 방향은 뷰포트별로 달라질 수 있으므로 동적 머티리얼을 분리한다.
				GridMaterials[i] = GridMaterial->CreateDynamicMaterial();
			}
		}
	}
}

void FEditorViewportClient::Detach(FEngine* Engine, FRenderer* Renderer)
{
	// 드래그 중인 기즈모와 에디터 전용 렌더 자원을 모두 해제한다.
	Gizmo.EndDrag();
	EditorUI.ShutdownRendererResources(Renderer);

	GridMesh.reset();
	GridMaterial.reset();
	for (int32 i = 0; i < MAX_VIEWPORTS; ++i)
	{
		GridMaterials[i].reset();
	}
}

void FEditorViewportClient::Tick(FEngine* Engine, float DeltaTime)
{
	IViewportClient::Tick(Engine, DeltaTime);
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	// 카메라 내비게이션과 뷰포트 입력 상태는 전용 서비스가 담당한다.
	InputService.TickCameraNavigation(Engine, EditorEngine, ViewportRegistry, Gizmo);
}

void FEditorViewportClient::HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	// 입력 서비스가 피킹, 기즈모, 선택 갱신까지 한 번에 처리한다.
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

void FEditorViewportClient::BuildSceneRenderPacket(
	FEngine* Engine,
	ULevel* Scene,
	const FFrustum& Frustum,
	const FShowFlags& Flags,
	FSceneRenderPacket& OutPacket)
{
	if (!Engine)
	{
		return;
	}
	// 실제 수집 로직은 공통 ViewportClient의 ScenePacketBuilder 경로를 재사용한다.
	IViewportClient::BuildSceneRenderPacket(Engine, Scene, Frustum, Flags, OutPacket);
}

void FEditorViewportClient::Render(FEngine* Engine, FRenderer* Renderer)
{
	if (!Renderer)
	{
		return;
	}

	// 렌더 전마다 Slate가 계산한 뷰포트 사각형을 레지스트리 엔트리에 반영한다.
	SyncViewportRectsFromDock();
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	FMaterial* GridMaterialPtrs[MAX_VIEWPORTS] = {};
	for (int32 i = 0; i < MAX_VIEWPORTS; ++i)
	{
		GridMaterialPtrs[i] = GridMaterials[i].get();
	}

	// 에디터 프레임 조립과 실제 요청 생성은 RenderService가 담당한다.
	RenderService.RenderAll(
		Engine,
		Renderer,
		EditorEngine,
		ViewportRegistry,
		EditorUI,
		Gizmo,
		WireFrameMaterial,
		GridMesh.get(),
		GridMaterialPtrs,
		[this](FEngine* InEngine, ULevel* Scene, const FFrustum& Frustum, const FShowFlags& Flags, FSceneRenderPacket& OutPacket)
		{
			BuildSceneRenderPacket(InEngine, Scene, Frustum, Flags, OutPacket);
		});
}

void FEditorViewportClient::SyncViewportRectsFromDock()
{
	// 중앙 dock rect가 있으면 그것을 쓰고, 없으면 ImGui 메인 뷰포트 작업 영역을 fallback으로 사용한다.
	FRect Central;
	if (!EditorUI.GetCentralDockRect(Central) || !Central.IsValid())
	{
		if (!ImGui::GetCurrentContext())
		{
			return;
		}
		ImGuiViewport* VP = ImGui::GetMainViewport();
		if (!VP || VP->WorkSize.x <= 0 || VP->WorkSize.y <= 0)
		{
			return;
		}
		Central.X      = static_cast<int32>(VP->WorkPos.x - VP->Pos.x);
		Central.Y      = static_cast<int32>(VP->WorkPos.y - VP->Pos.y);
		Central.Width  = static_cast<int32>(VP->WorkSize.x);
		Central.Height = static_cast<int32>(VP->WorkSize.y);
	}
	
	FSlateApplication* Slate = EditorEngine.GetSlateApplication();
	if (Slate)
	{
		// Slate 레이아웃 결과를 바탕으로 활성 뷰포트와 각 뷰포트 영역을 갱신한다.
		Slate->SetViewportAreaRect(Central);

		for (FViewportEntry& Entry : ViewportRegistry.GetEntries())
		{
			Entry.bActive = Slate->IsViewportActive(Entry.Id);
		}
	}
}
