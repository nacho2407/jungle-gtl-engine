#pragma once

#if IS_OBJ_VIEWER

#include "Core/CoreMinimal.h"
#include "Renderer/RendererModule.h"
#include "Renderer/SceneView.h"
#include "Camera/ViewportCamera.h"
#include "Viewer/OrbitalCameraController.h"
#include "ViewerImGui.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Engine::ApplicationCore
{
    class IApplication;
    class FWindowsApplication;
}
namespace Engine::Asset { class UStaticMesh; }
class UAssetManager;
class IAssetLoader;

class FObjViewerEngineLoop
{
public:
    bool  PreInit(HINSTANCE HInstance, uint32 NCmdShow);
    int32 Run();
    void  ShutDown();

private:
    void Tick();
    bool RunFrameOnce();

    void UpdateOrbitCamera();
    void FitCameraToMesh();
    void LoadMesh(const FWString& Path);
    void OpenFileDialog();
    void DrawUI();

    static bool HandleMessage(HWND HWnd, UINT Msg, WPARAM WParam, LPARAM LParam,
                              LRESULT& OutResult, void* UserData);
    bool HandleMessageInternal(HWND HWnd, UINT Msg, WPARAM WParam, LPARAM LParam,
                               LRESULT& OutResult);

    Engine::ApplicationCore::FWindowsApplication* GetWindowsApp() const;

private:
    Engine::ApplicationCore::IApplication*  Application = nullptr;
    FRendererModule*                        Renderer    = nullptr;
    UAssetManager*                          AssetManager   = nullptr;
    IAssetLoader*                           TextureLoader  = nullptr;
    IAssetLoader*                           MeshLoader     = nullptr;
    IAssetLoader*                           MatLoader      = nullptr;

    Engine::Asset::UStaticMesh* LoadedMesh = nullptr;
    FWString                    LoadedMeshPath;
    FString                     LoadedMeshName;

    FSceneView SceneView;
    FViewportCamera*         Camera = nullptr;
    FOrbitalCameraController NavController;
    ViewerUI::FViewerImGui   ImGuiLayer;

    float Slerping      = -1.f;
    float TargetPitch   = 0.f;
    float TargetYaw     = 0.f;

    // Mouse drag state
    bool  bRMBDown   = false;
    bool  bMMBDown   = false;
    float LastMouseX = 0.f;
    float LastMouseY = 0.f;

    int32  CachedW   = 0;
    int32  CachedH   = 0;
    double PrevTime  = 0.0;
    float  DeltaTime = 0.0f;
    float  FPS       = 0.0f;

    EViewModeIndex      ViewMode   = EViewModeIndex::VMI_Lit;
    ERasterizerCullMode CullMode   = ERasterizerCullMode::CULL_Back;
    bool bConvertCoords              = false;
    bool bIsExit                     = false;
    bool bIsInSizeMoveLoop           = false;
    bool bIsRenderingDuringSizeMove  = false;

private:
};

#endif // IS_OBJ_VIEWER
