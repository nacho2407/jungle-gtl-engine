#pragma once
#include "Viewport/Window/EditorViewportPanel.h"
#include "Core/Runtime/Slate/Window/SSplitter.h"

struct FEditorContext;
class  FScene;

class FWindowOverlayManager
{
  private:
    EViewportLayout               ViewportLayout = EViewportLayout::Single;
    TArray<FEditorViewportPanel*> ViewportPanels;
    FEditorContext*               EditorContext  = nullptr;
    FScene*                       Scene          = nullptr;

    // Global window dimension
    uint32 W = 0;
    uint32 H = 0;

    // Split positions as 0-1 ratios; preserved across window resizes
    float VSplitRatio = 0.5f;
    float HSplitRatio = 0.5f;

    // Typed splitter pointers — owned by this manager
    SSplitterV* SplitterV = nullptr;
    SSplitterH* SplitterH = nullptr;

    // Splitter drag state
    bool bDraggingV = false;
    bool bDraggingH = false;

    // Half-width of the grab zone in pixels
    static constexpr float SplitterHalfThick = 4.0f;

    FEditorViewportClient::FPickCallback PickCallback;

  public:
    void                           ResetViewportDimension();
    TArray<FEditorViewportPanel*>& GetViewportPanels();
    void                           AddNewViewportPanel();
    FEditorViewportPanel*          FindPanelAtPoint(int32 X, int32 Y) const;

    // Deletes and nullifies the heap memory occupied by dynamically allocated panels
    void Release();

    // Deletes and nullifies the heap memory occupied by dynamically allocated splitters
    void ReleaseSplitters();

    // Toggled when the entire window size is changed
    void SetWindowDimension(uint32 Width, uint32 Height);

    // Sets a lambda function to use for viewport screen interactions
    void SetPickCallback(FEditorViewportClient::FPickCallback Callback);

    // Sets an editor context for each viewport panel to use
    void SetEditorContext(FEditorContext* InEditorContext) { EditorContext = InEditorContext; }

    // Sets the scene to render for ALL viewports
    void SetScene(FScene* InScene);

    // Modifies how cameras are laid out onto the viewport
    void SetViewportLayout(EViewportLayout Layout);

    // Resets camera movement settings for all managed panels
    void SetNavigationValues(float MoveSpeed, float RotationSpeed);

    // Re-create splitters for the current layout and window size
    void ResetSplitters();

    // Returns a human readable string for each layout enum feed
    FString GetViewportLayoutString(EViewportLayout Layout) const;

    SSplitter*      GetSplitterV() const;
    SSplitter*      GetSplitterH() const;
    EViewportLayout GetViewportLayout() const { return ViewportLayout; }

    // ── Splitter drag interaction ────────────────────────────────────────────
    // Returns true if the pixel X is within the grab zone of the vertical splitter
    bool HitTestSplitterV(int32 X) const;
    // Returns true if the pixel Y is within the grab zone of the horizontal splitter
    bool HitTestSplitterH(int32 Y) const;
    // Start dragging; pass which axes are active
    void BeginSplitterDrag(bool bVertical, bool bHorizontal);
    // Feed per-frame mouse delta while dragging
    void UpdateSplitterDrag(float DeltaX, float DeltaY);
    // Release drag lock
    void EndSplitterDrag();
    bool IsDraggingSplitter() const { return bDraggingV || bDraggingH; }

    float GetVSplitRatio() const { return VSplitRatio; }
    float GetHSplitRatio() const { return HSplitRatio; }

  private:
    // Push current panel PosX/Y/Width/Height to each ViewportClient
    void SyncPanelClients();
};
