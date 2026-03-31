#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/ViewportLayout.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Renderer/Renderer.h"
#include "Engine/Core/InputSystem.h"
namespace
{
	void SetOpaqueBlendStateCallback(const ImDrawList*, const ImDrawCmd* Cmd)
	{
		ID3D11DeviceContext* DeviceContext = static_cast<ID3D11DeviceContext*>(Cmd->UserCallbackData);
		if (!DeviceContext) return;

		const float BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xffffffff);
	}

	const char* GetViewportTypeName(EEditorViewportType Type)
	{
		switch (Type)
		{
		case EVT_Perspective: return "Perspective";
		case EVT_OrthoTop:    return "Top";
		case EVT_OrthoBottom: return "Bottom";
		case EVT_OrthoFront:  return "Front";
		case EVT_OrthoBack:   return "Back";
		case EVT_OrthoLeft:   return "Left";
		case EVT_OrthoRight:  return "Right";
		default:              return "Viewport";
		}
	}

	const char* GetViewModeName(EViewMode Mode)
	{
		switch (Mode)
		{
		case EViewMode::Lit:       return "Lit";
		case EViewMode::Unlit:     return "Unlit";
		case EViewMode::Wireframe: return "Wireframe";
		default:                   return "Lit";
		}
	}

	const char* GetViewportSlotName(int32 Index)
	{
		switch (Index)
		{
		case 0: return "Viewport 0";
		case 1: return "Viewport 1";
		case 2: return "Viewport 2";
		case 3: return "Viewport 3";
		default: return "Viewport";
		}
	}
}
void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	Window = InWindow;
	EditorEngine = InEditorEngine;

	// 1차: malgun.ttf — 한글 + 기본 라틴 (주 폰트)
	ImFontGlyphRangesBuilder KoreanBuilder;
	KoreanBuilder.AddRanges(IO.Fonts->GetGlyphRangesKorean());
	KoreanBuilder.AddRanges(IO.Fonts->GetGlyphRangesDefault());
	KoreanBuilder.BuildRanges(&FontGlyphRanges);
	IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, nullptr, FontGlyphRanges.Data);

	// 2차: msyh.ttc — 한자 전체를 malgun이 없는 글리프에만 병합 (fallback)
	ImFontConfig MergeConfig;
	MergeConfig.MergeMode = true;
	IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 16.0f, &MergeConfig, IO.Fonts->GetGlyphRangesChineseFull());

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	ConsoleWidget.Initialize(InEditorEngine);
	ControlWidget.Initialize(InEditorEngine);
	MaterialWidget.Initialize(InEditorEngine);
	PropertyWidget.Initialize(InEditorEngine);
	SceneWidget.Initialize(InEditorEngine);
	ViewportOverlayWidget.Initialize(InEditorEngine);
	StatWidget.Initialize(InEditorEngine);
}
 
void FEditorMainPanel::Release()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FEditorMainPanel::Render(float DeltaTime)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	RenderViewportHostWindow();

	ConsoleWidget.Render(DeltaTime);
	ControlWidget.Render(DeltaTime);
	MaterialWidget.Render(DeltaTime);
	PropertyWidget.Render(DeltaTime);
	SceneWidget.Render(DeltaTime);
	StatWidget.Render(DeltaTime);
	ViewportOverlayWidget.Render(DeltaTime);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::Update()
{
	ImGuiIO& IO = ImGui::GetIO();

	bool bViewportOperationActive = false;
	if (EditorEngine)
	{
		FViewportLayout& Layout = EditorEngine->GetViewportLayout();
		for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
		{
			if (Layout.GetViewportClient(i).IsActiveOperation())
			{
				bViewportOperationActive = true;
				break;
			}
		}
	}

	if (bViewportOperationActive)
	{
		IO.ConfigFlags |= ImGuiConfigFlags_NoMouse;
	}
	else
	{
		IO.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
	}

	InputSystem::Get().GetGuiInputState().bUsingMouse = bViewportOperationActive ? false : IO.WantCaptureMouse;
	InputSystem::Get().GetGuiInputState().bUsingKeyboard = IO.WantCaptureKeyboard;

	//	Focus는 MainPanel에서 입력 받음
	if (EditorEngine && InputSystem::Get().GetKeyUp('F') && !IO.WantTextInput)
	{
		FViewportLayout& Layout = EditorEngine->GetViewportLayout();
		const int32 FocusedIdx = Layout.GetLastFocusedViewportIndex();
		Layout.GetViewportClient(FocusedIdx).FocusSelection();
	}

	// IME는 ImGui가 텍스트 입력을 원할 때만 활성화.
	// 그 외에는 OS 수준에서 IME 컨텍스트를 NULL로 연결해 한글 조합이
	// 뷰포트에 남는 현상을 원천 차단한다.
	if (Window)
	{
		HWND hWnd = Window->GetHWND();
		if (IO.WantTextInput)
		{
			// InputText 포커스 중 — 기본 IME 컨텍스트 복원
			ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
		}
		else
		{
			// InputText 포커스 없음 — IME 컨텍스트 해제 (조합 불가)
			ImmAssociateContext(hWnd, NULL);
		}
	}
}

// ImGui로 Viewport 가 차지할 영역을 계산하고 만든다.
void FEditorMainPanel::RenderViewportHostWindow()
{
	if (!EditorEngine) return;
	constexpr ImGuiWindowFlags WindowFlags = 0;
	FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();

	if (!ImGui::Begin("Viewport", nullptr, WindowFlags))
	{
		GuiState.bViewportHostVisible = false;
		GuiState.ViewportHostRect = FViewportRect();
		EditorEngine->GetViewportLayout().SetHostRect(FViewportRect());
		ImGui::End();
		return;
	}

	const ImVec2 ContentSize = ImGui::GetContentRegionAvail();
	if (ContentSize.x > 1.0f && ContentSize.y > 1.0f)
	{
		const ImVec2 ContentPos = ImGui::GetCursorScreenPos();
		const FViewportRect HostRect(
			static_cast<int32>(ContentPos.x),
			static_cast<int32>(ContentPos.y),
			static_cast<int32>(ContentSize.x),
			static_cast<int32>(ContentSize.y));

		GuiState.bViewportHostVisible = true;
		GuiState.ViewportHostRect = HostRect;
		EditorEngine->GetViewportLayout().SetHostRect(HostRect);

		if (ID3D11ShaderResourceView* SceneColorSRV = EditorEngine->GetRenderer().GetFD3DDevice().GetViewportSceneColorSRV())
		{
			ID3D11DeviceContext* DeviceContext = EditorEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			DrawList->AddCallback(SetOpaqueBlendStateCallback, DeviceContext);
			ImGui::Image(reinterpret_cast<ImTextureID>(SceneColorSRV), ContentSize);
			DrawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
		}
		else
		{
			ImGui::Dummy(ContentSize);
		}

		// 뷰포트별 독립 메뉴바 오버레이
		{
			FViewportLayout& Layout = EditorEngine->GetViewportLayout();
			const float MenuBarH = ImGui::GetFrameHeight();

			for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
			{
				const FEditorViewportState& VState = Layout.GetViewportState(i);
				if (VState.Rect.Width <= 0 || VState.Rect.Height <= 0) continue;

				const float LocalX = static_cast<float>(VState.Rect.X - HostRect.X);
				const float LocalY = static_cast<float>(VState.Rect.Y - HostRect.Y);
				if (LocalX < 0.0f || LocalY < 0.0f) continue;

				ImGui::SetCursorScreenPos(ImVec2(ContentPos.x + LocalX, ContentPos.y + LocalY));

				char ChildID[32];
				snprintf(ChildID, sizeof(ChildID), "##VPMenu%d", i);

				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 0.75f));
				constexpr ImGuiWindowFlags OverlayFlags =
					ImGuiWindowFlags_MenuBar           |
					ImGuiWindowFlags_NoScrollbar       |
					ImGuiWindowFlags_NoScrollWithMouse |
					ImGuiWindowFlags_NoNav             |
					ImGuiWindowFlags_NoFocusOnAppearing;

				if (ImGui::BeginChild(ChildID, ImVec2(static_cast<float>(VState.Rect.Width), MenuBarH), false, OverlayFlags))
				{
					if (ImGui::BeginMenuBar())
					{
						RenderViewportMenuBarForIndex(i);
						ImGui::EndMenuBar();
					}
				}
				ImGui::EndChild();
				ImGui::PopStyleColor();
			}
		}
	}
	else
	{
		GuiState.bViewportHostVisible = false;
		GuiState.ViewportHostRect = FViewportRect();
		EditorEngine->GetViewportLayout().SetHostRect(FViewportRect());
	}

	ImGui::End();
}

// 개별 뷰포트 메뉴바 렌더링 — Index 번 뷰포트에 대한 Layout / Type / View / Stats 메뉴
void FEditorMainPanel::RenderViewportMenuBarForIndex(int32 Index)
{
	FViewportLayout& Layout = EditorEngine->GetViewportLayout();
	FEditorViewportClient& Client = Layout.GetViewportClient(Index);
	FEditorViewportState& State = Layout.GetViewportState(Index);

	ImGui::TextDisabled("%s | %s | %s",
		GetViewportSlotName(Index),
		GetViewportTypeName(Client.GetViewportType()),
		GetViewModeName(State.ViewMode));
	ImGui::SameLine();

	if (ImGui::BeginMenu("Layout"))
	{
		const bool bSingle = Layout.IsSingleViewportMode();

		if (ImGui::MenuItem("SingleView", nullptr, bSingle))
			Layout.SetSingleViewportMode(true, Index);
		if (ImGui::MenuItem("Quad View", nullptr, !bSingle))
			Layout.SetSingleViewportMode(false);

		if (bSingle)
		{
			ImGui::Separator();
			for (int32 j = 0; j < FViewportLayout::MaxViewports; ++j)
			{
				const bool bSel = (Layout.GetSingleViewportIndex() == j);
				if (ImGui::MenuItem(GetViewportSlotName(j), nullptr, bSel))
				{
					Layout.SetSingleViewportMode(true, j);
					Layout.SetLastFocusedViewportIndex(j);
				}
			}
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Type"))
	{
		if (Index == 0)
		{
			ImGui::TextDisabled("Viewport 0 is fixed to Perspective.");
			ImGui::Separator();
			ImGui::MenuItem("Perspective", nullptr, true, false);
		}
		else
		{
			static constexpr EEditorViewportType kOrthoTypes[] =
			{
				EVT_OrthoTop, EVT_OrthoBottom,
				EVT_OrthoFront, EVT_OrthoBack,
				EVT_OrthoLeft, EVT_OrthoRight
			};
			for (EEditorViewportType Type : kOrthoTypes)
			{
				const bool bSel = (Client.GetViewportType() == Type);
				if (ImGui::MenuItem(GetViewportTypeName(Type), nullptr, bSel))
				{
					Client.SetViewportType(Type);
					Client.ApplyCameraMode();
				}
			}
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("View"))
	{
		static constexpr EViewMode Modes[] =
		{
			EViewMode::Lit,
			EViewMode::Unlit,
			EViewMode::Wireframe
		};
		static constexpr const char* Labels[] = { "Lit", "Unlit", "Wireframe" };

		for (int32 j = 0; j < 3; ++j)
		{
			const bool bSel = (State.ViewMode == Modes[j]);
			if (ImGui::MenuItem(Labels[j], nullptr, bSel))
				State.ViewMode = Modes[j];
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Stats"))
	{
		ImGui::MenuItem("FPS", nullptr, &State.bShowStatFPS);
		ImGui::MenuItem("Memory", nullptr, &State.bShowStatMemory);
		ImGui::EndMenu();
	}
}
