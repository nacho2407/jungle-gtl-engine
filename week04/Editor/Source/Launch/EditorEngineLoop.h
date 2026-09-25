#pragma once

#include "Chrome/EditorChrome.h"
#include "Core/CoreMinimal.h"
#include "ApplicationCore/GenericPlatform/IApplication.h"
#include "ApplicationCore/Input/InputSystem.h"
#include "Editor/Editor.h"
#include "Launch/EngineLoop.h"

#include "Renderer/RendererModule.h"

#if defined(_WIN32)
#include "ApplicationCore/Windows/WindowsApplication.h"
#endif

class FPanelManager;
class UAssetManager;
class IAssetLoader;

class FEditorEngineLoop : public IEngineLoop, public IEditorChromeHost
{
  public:
    bool PreInit(HINSTANCE HInstance, uint32 NCmdShow) override;
    int32 Run() override;
    void ShutDown() override;

    void SetTitleBarMetrics(int32 Height,
                            const TArray<FEditorChromeRect>& InteractiveRects) override;
    void MinimizeWindow() override;
    void ToggleMaximizeWindow() override;
    void CloseWindow() override;
    bool IsWindowMaximized() const override;
    const wchar_t* GetWindowTitle() const override;
    void* GetNativeWindowHandle() const override;

  private:
    static bool HandleEditorMessage(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam,
                                    LRESULT& OutResult, void* UserData);
    bool HandleEditorMessageInternal(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam,
                                     LRESULT& OutResult);
    bool HandleWindowResize();
    bool RunFrameOnce();
    bool RunFrameOnceWithoutResize();
    void UpdateFrameTiming();
    Engine::ApplicationCore::FWindowsApplication* GetWindowsApplication() const;

    void Tick() override;
    void InitializeForTime() override;

  private:
    Engine::ApplicationCore::IApplication* Application = nullptr;
    Engine::ApplicationCore::FInputSystem* InputSystem = nullptr;

    FEditor* Editor = nullptr;
    //FWindowOverlayManager* WindowOverlayManager = nullptr;
    FRendererModule* Renderer = nullptr;
    FPanelManager* PanelManager = nullptr;
    UAssetManager* AssetManager = nullptr;
    IAssetLoader* TextureAssetLoader = nullptr;
    IAssetLoader* FontAssetLoader = nullptr;
    IAssetLoader* SubUVAtlasAssetLoader = nullptr;
    IAssetLoader*    StaticMeshLoader = nullptr;
    IAssetLoader*    MaterialLoader = nullptr;

    float DeltaTime = 0.0f;
    float MainLoopFPS = 0.0f;
    int32 CachedWindowWidth = 0;
    int32 CachedWindowHeight = 0;
    // Win32의 move/size modal loop 안에서도 즉시 렌더링하기 위한 상태입니다.
    bool bIsInSizeMoveLoop = false;
    bool bIsRenderingDuringSizeMove = false;
    bool bSavedVSyncEnabled = true;

    double PrevTime = 0.0;

    bool bIsExit = false;

    HWND HWindow = nullptr;
};
