#include "ObjViewerMainPanel.h"
#include "Misc/ObjViewer/ObjViewerEngine.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Render/Renderer/Renderer.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

void FObjViewerMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UObjViewerEngine* InEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// TODO-VIEWER: Slate 구조 개편 후 엔진 포인터 삭제하기
	Engine = InEngine;
	Window = InWindow;

	// 한글 지원 폰트 로드
	IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	// 모든 하위 위젯 초기화
	MenuBarWidget.Initialize(InEngine);
	ControlWidget.Initialize(InEngine);
	StatWidget.Initialize(InEngine);
}

void FObjViewerMainPanel::Shutdown()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FObjViewerMainPanel::Render(float DeltaTime)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	// TODO-VIEWER: Slate 구조 개편 후 뷰포트 값 세팅하기
    if (Engine)
    {
        ImGuiDockNode* CentralNode = ImGui::DockBuilderGetCentralNode(dockspace_id);
        if (CentralNode)
        {
            Engine->GetViewportClient().SetViewportRect(
                CentralNode->Pos.x, CentralNode->Pos.y, 
                CentralNode->Size.x, CentralNode->Size.y);
        }
    }

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	// 하위 위젯들 렌더링
	MenuBarWidget.Render(DeltaTime);
	ControlWidget.Render(DeltaTime);
	StatWidget.Render(DeltaTime);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}