#include "ObjViewerEngineLoop.h"

#if IS_OBJ_VIEWER

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

#include "Core/Misc/NameSubsystem.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetLoader/StaticMeshLoader.h"
#include "Asset/AssetLoader/MaterialLoader.h"
#include "Asset/AssetLoader/TextureLoader.h"
#include "Asset/StaticMesh.h"

#include "Renderer/Types/RenderItem.h"
#include "Renderer/SceneFrameRenderData.h"

#include "ApplicationCore/Windows/WindowsApplication.h"
#include "imgui.h"

// ─── PreInit ─────────────────────────────────────────────────────────────────

bool FObjViewerEngineLoop::PreInit(HINSTANCE HInstance, uint32 NCmdShow)
{
    (void)HInstance;
    (void)NCmdShow;

    Engine::Core::Misc::FNameSubsystem::Init();

    Application = Engine::ApplicationCore::FWindowsApplication::Create();
    if (!Application) return false;

    auto* WinApp = GetWindowsApp();
    if (!WinApp) return false;

    if (!WinApp->CreateApplicationWindow(L"OBJ Viewer", 1280, 720))
        return false;

    HWND Hwnd = static_cast<HWND>(Application->GetNativeWindowHandle());
    DragAcceptFiles(Hwnd, TRUE);

    Renderer = new FRendererModule();
    if (!Renderer->StartupModule(Hwnd)) return false;

    AssetManager  = new UAssetManager();
    TextureLoader = new FTextureLoader(&Renderer->GetRHI());
    MatLoader     = new FMaterialLoader(AssetManager);
    MeshLoader    = new FStaticMeshLoader(&Renderer->GetRHI(), AssetManager);
    AssetManager->RegisterLoader(TextureLoader);
    AssetManager->RegisterLoader(MatLoader);
    AssetManager->RegisterLoader(MeshLoader);

    ImGuiLayer.Init(Hwnd, Renderer->GetRHI().GetDevice(), Renderer->GetRHI().GetDeviceContext());
    ImGuiIO& IO = ImGui::GetIO();
    ImFontGlyphRangesBuilder Builder;
    Builder.AddRanges(IO.Fonts->GetGlyphRangesKorean());
    Builder.AddRanges(IO.Fonts->GetGlyphRangesChineseFull());
    ImVector<ImWchar> Ranges;
    Builder.BuildRanges(&Ranges);
    if (ImFont* CjkFont = IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf",
                                                       18.0f, nullptr, Ranges.Data))
    {
        IO.FontDefault = CjkFont;
    }

    WinApp->SetMessageHandler(&FObjViewerEngineLoop::HandleMessage, this);

    CachedW = Application->GetWindowWidth();
    CachedH = Application->GetWindowHeight();
    Camera = new FViewportCamera();
    Camera->SetFOV(3.141592f * 0.5f); // 90°
    Camera->OnResize(CachedW, CachedH);

    NavController.SetCamera(Camera);
    NavController.SetOrbitPivot(FVector::Zero());
    NavController.SetOrbitRadius(5.f);
    NavController.SetOrbitAngles(0.f, 0.f);
    NavController.UpdateCamera();
    UpdateOrbitCamera();

    PrevTime = FPlatformTime::Seconds();
    return true;
}

// ─── Run / Tick ───────────────────────────────────────────────────────────────

int32 FObjViewerEngineLoop::Run()
{
    while (!bIsExit)
        Tick();
    return 0;
}

void FObjViewerEngineLoop::Tick()
{
    Application->PumpMessages();
    if (Application->IsExitRequested())
    {
        bIsExit = true;
        return;
    }
    RunFrameOnce();
}

// ─── ShutDown ─────────────────────────────────────────────────────────────────

void FObjViewerEngineLoop::ShutDown()
{
    if (auto* WinApp = GetWindowsApp())
        WinApp->SetMessageHandler(nullptr, nullptr);

    ImGuiLayer.Shutdown();

    delete MeshLoader;    MeshLoader    = nullptr;
    delete MatLoader;     MatLoader     = nullptr;
    delete TextureLoader; TextureLoader = nullptr;
    delete AssetManager;  AssetManager  = nullptr;

    if (Camera)
    {
        delete Camera;
        Camera = nullptr;
    }

    if (Renderer)
    {
        Renderer->ShutdownModule();
        delete Renderer;
        Renderer = nullptr;
    }

    if (Application)
    {
        Application->DestroyApplicationWindow();
        delete Application;
        Application = nullptr;
    }

    Engine::Core::Misc::FNameSubsystem::Shutdown();
}

// ─── Frame ────────────────────────────────────────────────────────────────────

bool FObjViewerEngineLoop::RunFrameOnce()
{
    if (bIsRenderingDuringSizeMove) return false;

    // Handle window resize
    const int32 W = Application->GetWindowWidth();
    const int32 H = Application->GetWindowHeight();
    if (W != CachedW || H != CachedH)
    {
        CachedW = W;
        CachedH = H;
        Renderer->OnWindowResized(W, H);
        UpdateOrbitCamera();
    }

    bIsRenderingDuringSizeMove = true;

    // Frame timing
    const double Now = FPlatformTime::Seconds();
    double Raw = Now - PrevTime;
    PrevTime = Now;
    if (Raw < 1.0 / 1000.0) Raw = 1.0 / 1000.0;
    else if (Raw > 1.0 / 15.0) Raw = 1.0 / 15.0;
    DeltaTime = static_cast<float>(Raw);
    FPS       = static_cast<float>(1.0 / Raw);

    if (Slerping > 1.f)
    {
        NavController.SetOrbitAngles(TargetPitch, TargetYaw);
        NavController.UpdateCamera();
        UpdateOrbitCamera();
        Slerping = -1.f;
    }
    else if (Slerping >= 0.f)
    {
        NavController.SlerpCamera(TargetPitch, TargetYaw, Slerping);
        UpdateOrbitCamera();
        Slerping += DeltaTime / 2;
    }

    // Build scene frame data
    FSceneFrameRenderData FrameData;
    if (LoadedMesh && LoadedMesh->GetRenderResource())
    {
        FStaticMeshRenderItem Item;

        // LH Y-up → Z-up: maps (X=right, Y=up, Z=fwd) to (X=fwd, Y=right, Z=up).
        // Row-vector form: v_eng = v_obj * M, where rows are (0,1,0), (0,0,1), (1,0,0).
        // Pure permutation (det=+1) so normals transform correctly under the same matrix.
        if (bConvertCoords)
        {
            FMatrix CoordConv = FMatrix::Identity;
            CoordConv.SetAxes(FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0));
            Item.World = FMatrix::MakeScale(ModelScale * AbsoluteScale) * CoordConv;
        }
        else
        {
            Item.World = FMatrix::MakeScale(ModelScale * AbsoluteScale);
        }

        Item.RenderResource = LoadedMesh->GetRenderResource();
        Item.WorldAABB      = LoadedMesh->GetRenderResource()->BoundingBox;
        Item.State.ObjectId = 1;
        Item.State.SetVisible(true);
        Item.State.SetPickable(false);

        for (uint32 i = 0; i < LoadedMesh->GetMaterialSlotsSize(); ++i)
        {
            Item.Materials.push_back(LoadedMesh->GetMaterial(i));
        }
        FrameData.StaticMeshes.push_back(std::move(Item));
    }

    FEditorRenderData RenderData;
    RenderData.SceneView      = &SceneView;
    RenderData.bShowGrid      = false;
    RenderData.bShowWorldAxes = true;
    RenderData.CullMode       = CullMode;

    Renderer->BeginFrame();
    Renderer->SetSceneFrameData(std::move(FrameData));
    Renderer->Render(RenderData, ViewMode);
    DrawUI();
    Renderer->EndFrame();

    bIsRenderingDuringSizeMove = false;
    return true;
}

// ─── Orbit camera ─────────────────────────────────────────────────────────────

void FObjViewerEngineLoop::UpdateOrbitCamera()
{
    // NavController has already repositioned the camera; just sync the SceneView.
    SceneView.SetViewMatrix(Camera->GetViewMatrix());
    SceneView.SetProjectionMatrix(Camera->GetProjectionMatrix());
    SceneView.SetViewLocation(Camera->GetLocation());
    SceneView.SetViewRect({ 0, 0, CachedW, CachedH });
    SceneView.SetClipPlanes(0.1f, 3000.f);
}

void FObjViewerEngineLoop::FitCameraToMesh()
{
    if (!LoadedMesh || !LoadedMesh->GetRenderResource()) return;

    const auto& BB       = LoadedMesh->GetRenderResource()->BoundingBox;

    // Normalize extreme sizes to fit the camera
    if (BB.Max.Size() > 10000)
    {
        AbsoluteScale = 1 / BB.Max.Size();
    }

    const FVector Center = (BB.Max + BB.Min) * 0.5f * AbsoluteScale;
    const float Diagonal = (BB.Max - BB.Min).Size() * AbsoluteScale;

    NavController.SetOrbitPivot(Center);
    NavController.SetOrbitRadius(Diagonal * 1.5f + 0.1f);
    NavController.SetOrbitAngles(0.f, 0.f);
    NavController.UpdateCamera();
    UpdateOrbitCamera();
}

// ─── Mesh loading ─────────────────────────────────────────────────────────────

void FObjViewerEngineLoop::LoadMesh(const FWString& Path)
{
    UAsset* Asset = AssetManager->Load(Path);
    Engine::Asset::UStaticMesh* SM = Cast<Engine::Asset::UStaticMesh>(Asset);
    if (!SM) return;

    LoadedMesh     = SM;
    LoadedMeshPath = Path;

    const auto LastSep = Path.find_last_of(L"\\/");
    const FWString NameW = (LastSep != FWString::npos) ? Path.substr(LastSep + 1) : Path;
    LoadedMeshName.clear();
    LoadedMeshName.reserve(NameW.size());
    for (wchar_t Ch : NameW)
        LoadedMeshName += static_cast<char>(Ch < 128 ? Ch : '?');

    FitCameraToMesh();
}

void FObjViewerEngineLoop::OpenFileDialog()
{
    wchar_t FilePath[MAX_PATH] = {};
    OPENFILENAMEW OFN  = {};
    OFN.lStructSize    = sizeof(OFN);
    OFN.hwndOwner      = static_cast<HWND>(Application->GetNativeWindowHandle());
    OFN.lpstrFilter = L"OBJ Files (*.obj)\0*.obj\0"
                      L"All Files (*.*)\0*.*\0\0";
    OFN.lpstrFile      = FilePath;
    OFN.nMaxFile       = MAX_PATH;
    OFN.Flags          = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    OFN.lpstrTitle     = L"Open OBJ File";

    if (GetOpenFileNameW(&OFN))
        LoadMesh(FilePath);
}

// ─── ImGui overlay ────────────────────────────────────────────────────────────

void FObjViewerEngineLoop::DrawUI()
{
    ViewerUI::FViewerUIInput In;
    In.MeshName         = LoadedMesh ? LoadedMeshName.c_str() : nullptr;
    In.FPS              = FPS;
    In.CurrentViewMode  = ViewMode;
    In.SelectedCullMode = CullMode;
    In.bConvertCoords   = bConvertCoords;
    In.ModelScale       = ModelScale;
    In.AbsoluteScale    = AbsoluteScale;

    const ViewerUI::FViewerUIOutput Out = ImGuiLayer.Draw(In);
    if (Out.bOpenRequested)
        OpenFileDialog();
    ViewMode       = Out.SelectedViewMode;
    CullMode       = Out.SelectedCullMode;
    bConvertCoords = Out.bConvertCoords;
    if (Out.bIsScaleChanged)
    {
        ModelScale    = Out.ModelScale;
        AbsoluteScale = Out.AbsoluteScale;
    }

    if (Out.CameraCommand != ViewerUI::ECC_None)
    {
        // Fit pivot and radius to the mesh WITHOUT resetting angles.
        // The slerp animates from the current angles to the target — snapping
        // angles here (as FitCameraToMesh does) would defeat the purpose.
        if (LoadedMesh && LoadedMesh->GetRenderResource())
        {
            const auto& BB = LoadedMesh->GetRenderResource()->BoundingBox;
            NavController.SetOrbitPivot((BB.Max + BB.Min) * 0.5f * AbsoluteScale);
            NavController.SetOrbitRadius(((BB.Max - BB.Min).Size() * 1.5f + 0.1f) * AbsoluteScale);
        }

        switch (Out.CameraCommand)
        {
        case ViewerUI::ECC_Forward: TargetPitch =   0.f; TargetYaw =    0.f; break;
        case ViewerUI::ECC_Back:    TargetPitch =   0.f; TargetYaw =  180.f; break;
        case ViewerUI::ECC_Left:    TargetPitch =   0.f; TargetYaw =   90.f; break;
        case ViewerUI::ECC_Right:   TargetPitch =   0.f; TargetYaw =  -90.f; break;
        case ViewerUI::ECC_Up:      TargetPitch = -89.f; TargetYaw =    0.f; break;
        case ViewerUI::ECC_Bottom:  TargetPitch =  89.f; TargetYaw =    0.f; break;
        default:                                                             break;
        }
        Slerping = 0.f; // RunFrameOnce drives the animation from here
    }
}

// ─── Helper ──────────────────────────────────────────────────────────────



// ─── WndProc handler ──────────────────────────────────────────────────────────

bool FObjViewerEngineLoop::HandleMessage(HWND HWnd, UINT Msg, WPARAM WParam, LPARAM LParam,
                                         LRESULT& OutResult, void* UserData)
{
    auto* Self = static_cast<FObjViewerEngineLoop*>(UserData);
    return Self ? Self->HandleMessageInternal(HWnd, Msg, WParam, LParam, OutResult) : false;
}

bool FObjViewerEngineLoop::HandleMessageInternal(HWND HWnd, UINT Msg, WPARAM WParam,
                                                  LPARAM LParam, LRESULT& OutResult)
{
    // Let ImGui see the message first so WantCaptureMouse is up-to-date
    const LRESULT ImGuiResult   = ImGuiLayer.ForwardMessage(HWnd, Msg, WParam, LParam);
    const bool bImGuiWantsMouse = ImGuiLayer.WantCaptureMouse();

    switch (Msg)
    {
    case WM_CLOSE:
        bIsExit = true;
        if (Application) Application->DestroyApplicationWindow();
        OutResult = 0;
        return true;

    case WM_ENTERSIZEMOVE:
        bIsInSizeMoveLoop = true;
        OutResult = 0;
        return true;

    case WM_EXITSIZEMOVE:
        bIsInSizeMoveLoop = false;
        OutResult = 0;
        return false;

    case WM_SIZE:
        if (bIsInSizeMoveLoop && WParam != SIZE_MINIMIZED)
        {
            const int32 W = static_cast<int32>(LOWORD(LParam));
            const int32 H = static_cast<int32>(HIWORD(LParam));
            if (W > 0 && H > 0 && Renderer)
            {
                CachedW = W; CachedH = H;
                Renderer->OnWindowResized(W, H);
                UpdateOrbitCamera();
                RunFrameOnce();
            }
        }
        break;

    case WM_PAINT:
        if (bIsInSizeMoveLoop)
        {
            PAINTSTRUCT PS = {};
            BeginPaint(HWnd, &PS);
            EndPaint(HWnd, &PS);
            RunFrameOnce();
            OutResult = 0;
            return true;
        }
        break;

    case WM_DROPFILES:
    {
        HDROP Drop = reinterpret_cast<HDROP>(WParam);
        wchar_t DroppedPath[MAX_PATH] = {};
        if (DragQueryFileW(Drop, 0, DroppedPath, MAX_PATH))
        {
            FWString P(DroppedPath);
            const auto Dot = P.find_last_of(L'.');
            if (Dot != FWString::npos)
            {
                FWString Ext = P.substr(Dot);
                for (auto& C : Ext) C = static_cast<wchar_t>(towlower(C));
                if (Ext == L".obj") LoadMesh(P);
            }
        }
        DragFinish(Drop);
        OutResult = 0;
        return true;
    }

    case WM_RBUTTONDOWN:
        if (!bImGuiWantsMouse)
        {
            bRMBDown   = true;
            LastMouseX = static_cast<float>(GET_X_LPARAM(LParam));
            LastMouseY = static_cast<float>(GET_Y_LPARAM(LParam));
            SetCapture(HWnd);
        }
        break;

    case WM_RBUTTONUP:
        bRMBDown = false;
        ReleaseCapture();
        break;

    case WM_MBUTTONDOWN:
        if (!bImGuiWantsMouse)
        {
            bMMBDown   = true;
            LastMouseX = static_cast<float>(GET_X_LPARAM(LParam));
            LastMouseY = static_cast<float>(GET_Y_LPARAM(LParam));
            SetCapture(HWnd);
        }
        break;

    case WM_MBUTTONUP:
        bMMBDown = false;
        ReleaseCapture();
        break;

    case WM_MOUSEMOVE:
    {
        if (!bRMBDown && !bMMBDown) break;

        const float MX = static_cast<float>(GET_X_LPARAM(LParam));
        const float MY = static_cast<float>(GET_Y_LPARAM(LParam));
        const float DX = MX - LastMouseX;
        const float DY = MY - LastMouseY;
        LastMouseX = MX;
        LastMouseY = MY;

        if (bRMBDown)
        {
            Slerping = -1.f; // cancel any in-progress camera alignment slerp
            NavController.AddYawInput(DX);
            NavController.AddPitchInput(-DY);
            UpdateOrbitCamera();
        }
        else if (bMMBDown)
        {
            // Pan: move orbit pivot (and camera) along the camera's local right/up plane
            NavController.AddPanInput(-DX, DY);
            UpdateOrbitCamera();
        }
        break;
    }

    case WM_MOUSEWHEEL:
        if (!bImGuiWantsMouse)
        {
            const float Delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(WParam)) / WHEEL_DELTA;
            // Proportional zoom: 10% of current radius per wheel tick
            NavController.Dolly(Delta * NavController.GetOrbitRadius() * 0.1f);
            UpdateOrbitCamera();
        }
        break;

    default:
        break;
    }

    if (ImGuiResult != 0)
    {
        OutResult = ImGuiResult;
        return true;
    }

    return false;
}

Engine::ApplicationCore::FWindowsApplication* FObjViewerEngineLoop::GetWindowsApp() const
{
    return static_cast<Engine::ApplicationCore::FWindowsApplication*>(Application);
}

#endif // IS_OBJ_VIEWER
