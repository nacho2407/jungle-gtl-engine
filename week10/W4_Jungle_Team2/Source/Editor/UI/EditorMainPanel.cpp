#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Renderer/Renderer.h"
#include "Engine/Core/InputSystem.h"
namespace
{
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

	InputSystem::Get().GetGuiInputState().bUsingMouse = IO.WantCaptureMouse;
	InputSystem::Get().GetGuiInputState().bUsingKeyboard = IO.WantCaptureKeyboard;

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
	constexpr ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_MenuBar;
	if (!ImGui::Begin("Viewport", nullptr, WindowFlags))
	{
		ImGui::End();
		return;
	}

	if (ImGui::BeginMenuBar())
	{
		RenderViewportMenuBar();
		ImGui::EndMenuBar();
	}

	const ImVec2 ContentSize = ImGui::GetContentRegionAvail();
	if (ContentSize.x > 1.0f && ContentSize.y > 1.0f)
	{
		// ImGui::Image(reinterpret_cast<ImTextureID>(SceneColorSRV), ContentSize);
		ImGui::Dummy(ContentSize);
	}

	ImGui::End();
}

void FEditorMainPanel::RenderViewportMenuBar()
{
	FViewportLayout& Layout = EditorEngine->GetViewportLayout();
	const int32 TargetIndex = ResolveViewportMenuTarget();
	FEditorViewportClient& TargetClient = Layout.GetViewportClient(TargetIndex);
	FEditorViewportState& TargetState = Layout.GetViewportState(TargetIndex);
	ImGui::Text("Target : %s | %s | %s",
		GetViewportSlotName(TargetIndex),
		GetViewportTypeName(TargetClient.GetViewportType()),
		GetViewModeName(TargetState.ViewMode));
	ImGui::SameLine();

	if (ImGui::BeginMenu("Target"))
	{
		for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
		{
			const bool bSelected = (i == TargetIndex);
			if (ImGui::MenuItem(GetViewportSlotName(i), nullptr, bSelected))
			{
				Layout.SetLastFocusedViewportIndex(i);
			}
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Layout"))
	{
		const bool bSingle = Layout.IsSingleViewportMode();

		if (ImGui::MenuItem("SingleView", nullptr, bSingle))
		{
			Layout.SetSingleViewportMode(true, TargetIndex);
		}
		if (ImGui::MenuItem("Quad View", nullptr, !bSingle))
		{
			Layout.SetSingleViewportMode(false);
		}

		if (bSingle)
		{
			ImGui::Separator();
			for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
			{
				const bool bSelected = (Layout.GetSingleViewportIndex() == i);
				if (ImGui::MenuItem(GetViewportSlotName(i), nullptr, bSelected))
				{
					Layout.SetSingleViewportMode(true, i);
					Layout.SetLastFocusedViewportIndex(i);
				}
			}
		}
	}
	ImGui::EndMenu();

	if (ImGui::BeginMenu("Type"))
	{
		if (TargetIndex == 0)
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
				const bool bSelected = (TargetClient.GetViewportType() == Type);
				if (ImGui::MenuItem(GetViewportTypeName(Type), nullptr, bSelected))
				{
					TargetClient.SetViewportType(Type);
					TargetClient.ApplyCameraMode();
				}
			}
		}

		ImGui::EndMenu();
	}

	if(ImGui::BeginMenu("View"))
	{
		static constexpr EViewMode Modes[] =
		{
			EViewMode::Lit,
			EViewMode::Unlit,
			EViewMode::Wireframe
		};

		static constexpr const char* Labels[] = {
			"Lit",
			"Unlit",
			"Wireframe"
		};

		for (int32 i = 0; i < 3; ++i)
		{
			const bool bSelected = (TargetState.ViewMode == Modes[i]);
			if (ImGui::MenuItem(Labels[i], nullptr, bSelected))
			{
				TargetState.ViewMode = Modes[i];
			}
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Stats"))
	{
		ImGui::MenuItem("FPS", nullptr, &TargetState.bShowStatFPS);
		ImGui::MenuItem("Memory", nullptr, &TargetState.bShowStatMemory);
		ImGui::EndMenu();
	}

}

// Target Viewport의 index를 반환한다.
int32 FEditorMainPanel::ResolveViewportMenuTarget() const
{
	if (!EditorEngine) return 0;

	const FViewportLayout& Layout = EditorEngine->GetViewportLayout();

	for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
	{
		const FEditorViewportState& State = Layout.GetViewportState(i);
		if (State.bHovered && State.Rect.Width > 0 && State.Rect.Height > 0)
		{
			return i;
		}
	}

	return Layout.GetLastFocusedViewportIndex();
}
