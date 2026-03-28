#include "ObjViewerMainPanel.h"
#include "Editor/ObjViewerEngine.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Render/Renderer/Renderer.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

void FObjViewerMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UObjViewerEngine* InEngine)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    // 하위 위젯들 렌더링
    MenuBarWidget.Render(DeltaTime);
    ControlWidget.Render(DeltaTime);
    StatWidget.Render(DeltaTime);

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}