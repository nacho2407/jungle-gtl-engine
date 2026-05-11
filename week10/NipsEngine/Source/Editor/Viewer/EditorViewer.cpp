#include "EditorViewer.h"
#include "EditorEngine.h"
#include "Editor/EditorRenderPipeline.h"
#include "imgui.h"

void FEditorViewer::Init(
    FWindowsWindow* InWindow,
    UEditorEngine* InEditor,
    UWorld* InWorld,
    FSelectionManager* InSelectionManager)
{
    Window = InWindow;
    Editor = InEditor;

    Viewport.SetClient(&Client);

    Client.Initialize(InWindow, InEditor);
    Client.SetWorld(InWorld);
    Client.SetGizmo(InSelectionManager->GetGizmo());
    Client.SetSelectionManager(InSelectionManager);

    Client.SetViewport(&Viewport);
    Client.SetState(&Viewport.GetState());

    Client.SetViewportType(EEditorViewportType::EVT_Perspective);
    Client.CreateCamera();
    Client.ApplyCameraMode();

	// 임시
	FViewportRect Rect = { 0, 0, 300, 300 };
    Viewport.SetRect(Rect);
}

void FEditorViewer::Shutdown()
{
    Viewport.SetClient(nullptr);
}

void FEditorViewer::Tick(float DeltaTime)
{
    Client.Tick(DeltaTime);
}

void FEditorViewer::Resize(int32 Width, int32 Height)
{
    if (Width <= 0 || Height <= 0)
    {
        return;
    }

    const FViewportRect& CurRect = Viewport.GetRect();

    if (CurRect.Width == Width && CurRect.Height == Height)
    {
        return; // 변화 없음
    }

    FViewportRect NewRect = {
        0,
        0,
        Width,
        Height
    };

    Viewport.SetRect(NewRect);
}
