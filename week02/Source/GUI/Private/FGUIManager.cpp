#include "FGUIManager.h"

#include "WindowsApplication.h"
#include "URenderer.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

FGUIManager::FGUIManager()
{
}

FGUIManager::~FGUIManager()
{
    Shutdown();
}

bool FGUIManager::Initialize(FWindowsApplication* InWindowApp, URenderer* InRenderer)
{
    if (bInitialized)
    {
        return true;
    }

    if (!InWindowApp || !InRenderer || !InRenderer->Device || !InRenderer->DeviceContext)
    {
        return false;
    }

    WindowApp = InWindowApp;
    Renderer = InRenderer;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& Io = ImGui::GetIO();
    (void)Io;

    //Io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(WindowApp->GetHWND()))
    {
        ImGui::DestroyContext();
        WindowApp = nullptr;
        Renderer = nullptr;
        return false;
    }

    if (!ImGui_ImplDX11_Init(Renderer->Device, Renderer->DeviceContext))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        WindowApp = nullptr;
        Renderer = nullptr;
        return false;
    }

    bInitialized = true;
    return true;
}

void FGUIManager::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    Renderer = nullptr;
    WindowApp = nullptr;
    bInitialized = false;
}

void FGUIManager::BeginFrame()
{
    if (!bInitialized)
    {
        return;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void FGUIManager::EndFrame()
{
    if (!bInitialized)
    {
        return;
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool FGUIManager::WantsMouseCapture() const
{
    return bInitialized && ImGui::GetIO().WantCaptureMouse;
}

bool FGUIManager::WantsKeyboardCapture() const
{
    return bInitialized && ImGui::GetIO().WantCaptureKeyboard;
}