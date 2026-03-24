#include "Renderer/RendererModule.h"
#include "Renderer/Types/PickId.h"
#include "Renderer/Types/PickResult.h"

bool FRendererModule::StartupModule(HWND hWnd)
{
    if (hWnd == nullptr)
    {
        return false;
    }

    if (!RHI.Initialize(hWnd))
    {
        ShutdownModule();
        return false;
    }

    if (!MeshRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

    if (!LineRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

    if (!FontRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

    if (!SpriteRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

    if (!ObjectIdRenderer.Initialize(&RHI))
    {
        ShutdownModule();
        return false;
    }

#if defined(_DEBUG)
    if (RHI.GetDevice() != nullptr)
    {
        RHI.GetDevice()->QueryInterface(__uuidof(ID3D11Debug),
                                        reinterpret_cast<void**>(DebugDevice.GetAddressOf()));
    }
#endif

    return true;
}

void FRendererModule::ShutdownModule()
{
    DebugDevice.Reset();

    ObjectIdRenderer.Shutdown();
    SpriteRenderer.Shutdown();
    FontRenderer.Shutdown();
    LineRenderer.Shutdown();
    MeshRenderer.Shutdown();

#if defined(_DEBUG)
    if (DebugDevice != nullptr)
    {
        DebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
        DebugDevice.Reset();
    }
#endif

    RHI.Shutdown();
}

void FRendererModule::BeginFrame()
{
    RHI.BeginFrame();

    static const FLOAT ClearColor[4] = {0.15f, 0.15f, 0.15f, 1.0f};

    RHI.SetDefaultRenderTargets();
    RHI.Clear(ClearColor, 1.0f, 0);
}

void FRendererModule::EndFrame() { RHI.EndFrame(); }

void FRendererModule::OnWindowResized(int32 InWidth, int32 InHeight)
{
    if (InWidth <= 0 || InHeight <= 0)
    {
        return;
    }

    RHI.Resize(InWidth, InHeight);
    ObjectIdRenderer.Resize(InWidth, InHeight);
}

void FRendererModule::Render(const FEditorRenderData& InEditorRenderData,
                             const FSceneRenderData&  InSceneRenderData)
{
    CachedEditorRenderData = InEditorRenderData;
    CachedSceneRenderData = InSceneRenderData;

    // ================ Mesh =================
    MeshRenderer.BeginFrame(InSceneRenderData.SceneView, InSceneRenderData.ViewMode,
                            InSceneRenderData.bUseInstancing);

    if (IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_Primitives))
    {
        PrimitiveDrawer.Draw(MeshRenderer, InSceneRenderData);
    }

    if (IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Gizmo))
    {
        GizmoDrawer.Draw(MeshRenderer, InEditorRenderData);
    }

    MeshRenderer.EndFrame();

    // ================ Sprite =================
    if (InSceneRenderData.SceneView != nullptr &&
        IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_Sprites))
    {
        SpriteRenderer.BeginFrame(InSceneRenderData.SceneView);

        for (const FSpriteRenderItem& Item : InSceneRenderData.Sprites)
        {
            SpriteRenderer.AddSprite(Item);
        }

        SpriteRenderer.EndFrame(InSceneRenderData.SceneView);
    }

    // ================ Font =================
    if (InSceneRenderData.SceneView != nullptr &&
        IsFlagSet(InSceneRenderData.ShowFlags, ESceneShowFlags::SF_BillboardText))
    {
        FontRenderer.BeginFrame(InSceneRenderData.SceneView);

        for (const FTextRenderItem& Item : InSceneRenderData.Texts)
        {
            FontRenderer.AddText(Item);
        }

        FontRenderer.EndFrame(InSceneRenderData.SceneView);
    }

    // ================ Line =================
    if (InEditorRenderData.SceneView != nullptr)
    {
        LineRenderer.BeginFrame(InEditorRenderData.SceneView);

        if (IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Grid))
        {
            WorldGridDrawer.Draw(LineRenderer, InEditorRenderData);
        }

        if (IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_WorldAxes))
        {
            WorldAxesDrawer.Draw(LineRenderer, InEditorRenderData);
        }

        LineRenderer.EndFrame();
    }
}

bool FRendererModule::PickRaw(const FEditorRenderData& InEditorRenderData, int32 MouseX,
                              int32 MouseY, uint32& OutPickId)
{
    OutPickId = PickId::None;

    const FSceneView* SceneView = InEditorRenderData.SceneView;
    if (SceneView == nullptr)
    {
        return false;
    }

    // 해당 픽셀만 ID 버퍼에 기록한다.
    ObjectIdRenderer.BeginFrame(SceneView, MouseX, MouseY);

    // Gizmo만 판별할거면 프리미티브는 그릴 필요 없음
    // if (IsFlagSet(CachedSceneRenderData.ShowFlags, ESceneShowFlags::SF_Primitives))
    // {
    //     TArray<FObjectIdRenderItem> SceneItems;
    //     SceneItems.reserve(CachedSceneRenderData.Primitives.size());

    //     for (const FPrimitiveRenderItem& Primitive : CachedSceneRenderData.Primitives)
    //     {
    //         if (!Primitive.bVisible || !Primitive.bPickable || Primitive.ObjectId == 0)
    //         {
    //             continue;
    //         }

    //         FObjectIdRenderItem Item = {};
    //         Item.World = Primitive.World;
    //         Item.MeshType = Primitive.MeshType;
    //         Item.ObjectId = Primitive.ObjectId;
    //         SceneItems.push_back(Item);
    //     }

    //     ObjectIdRenderer.AddPrimitives(SceneItems);
    // }

    if (IsFlagSet(InEditorRenderData.ShowFlags, EEditorShowFlags::SF_Gizmo))
    {
        TArray<FObjectIdRenderItem> GizmoItems;
        GizmoDrawer.BuildObjectIdRenderItems(GizmoItems, InEditorRenderData);
        ObjectIdRenderer.AddPrimitives(GizmoItems);
    }

    return ObjectIdRenderer.RenderAndReadBack(OutPickId);
}

bool FRendererModule::Pick(const FEditorRenderData& InEditorRenderData, int32 MouseX, int32 MouseY,
                           FPickResult& OutResult)
{
    OutResult = {};

    uint32 PickedId = PickId::None;
    if (!PickRaw(InEditorRenderData, MouseX, MouseY, PickedId))
    {
        return false;
    }

    OutResult = PickResult::FromPickId(PickedId);
    return true;
}

void FRendererModule::SetVSyncEnabled(bool bEnabled) { RHI.SetVSyncEnabled(bEnabled); }

bool FRendererModule::IsVSyncEnabled() const { return RHI.IsVSyncEnabled(); }
