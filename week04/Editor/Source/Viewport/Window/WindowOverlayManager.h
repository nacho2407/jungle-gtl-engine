#pragma once
#include "Viewport/Window/EditorViewportPanel.h"
#include "Viewport/Selection/ViewportSelectionController.h"
#include "Core/Runtime/Slate/Window/SSplitter.h"
#include "Renderer/SceneFrameRenderData.h"
#include "Renderer/WidgetRenderData.h"

struct FEditorContext;
class  FScene;

struct FGlobalRenderSetting
{
    bool bShowUUIDLabels = true;
    bool bBillboardText = true;
    bool bShowScenePrimitives = true;
    bool bShowSceneSprites = true;

    ESceneShowFlags BuildSceneShowFlags() const
    {
        ESceneShowFlags Flags = ESceneShowFlags::None;
        if (bShowScenePrimitives)
        {
            Flags |= ESceneShowFlags::SF_Primitives;
        }
        if (bBillboardText)
        {
            Flags |= ESceneShowFlags::SF_BillboardText;
        }
        if (bShowSceneSprites)
        {
            Flags |= ESceneShowFlags::SF_Sprites;
        }
        if (bShowUUIDLabels)
        {
            Flags |= ESceneShowFlags::SF_UUIDText;
        }
        return Flags;
    }
};

class FWindowOverlayManager
{
  private:
    EViewportLayout               ViewportLayout = EViewportLayout::Single;
    TArray<FEditorViewportPanel*> ViewportPanels;
    FEditorViewportPanel*         LastFocusedPanel = nullptr;
    FEditorContext*               EditorContext  = nullptr;
    FScene*                       Scene          = nullptr;
    FGlobalRenderSetting          RenderSetting;

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

    // A single shared selection controller distributed to all dynamically created sub-panels.
    // Points to the primary ViewportClient's owned controller (set via SetSharedSelectionController).
    FViewportSelectionController* SharedSelectionController = nullptr;

    // 실제 화면에 그려지는 뷰포트 영역의 위치와 크기
    float ViewportAreaX = 0.0f;
    float ViewportAreaY = 0.0f;
    float ViewportAreaWidth = 0.0f;
    float ViewportAreaHeight = 0.0f;

  public:
    void                           ResetViewportDimension();
    TArray<FEditorViewportPanel*>& GetViewportPanels();
    void                           AddNewViewportPanel();
    FEditorViewportPanel*          FindPanelAtPoint(int32 X, int32 Y);
    FEditorViewportPanel*          FindPanelAtPointClicked(int32 X, int32 Y);


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

    // Designates a single selection controller to be shared by all dynamically created sub-panels.
    // Call this once after the primary ViewportClient is initialized.
    void SetSharedSelectionController(FViewportSelectionController* SC) { SharedSelectionController = SC; }

    // Sets the scene to render for ALL viewports
    void SetScene(FScene* InScene);

    // Traverses the scene once per frame and returns view-independent render items.
    // Call this before the per-panel render loop, then pass the result to
    // FRendererModule::SetSceneFrameData.
    FSceneFrameRenderData BuildSceneFrameData() const;

    // Modifies how cameras are laid out onto the viewport
    void SetViewportLayout(EViewportLayout Layout);

    // Resets camera movement settings for all managed panels
    void SetNavigationValues(float MoveSpeed, float RotationSpeed);

    // Sets a new view orientation for last focused panel
    void SetViewportOrientation(EViewportViewOrientation InViewOrientation);

    // Re-create splitters for the current layout and window size
    void ResetSplitters();

    // Pushes each panel's viewport client data into the provided WidgetData for rendering; called
    // by the main loop
    void BuildViewportWIdgetData(FWidgetRenderData& WidgetData);

    // Returns a human readable string for each layout enum feed
    FString GetViewportLayoutString(EViewportLayout Layout) const;

    FGlobalRenderSetting& GetRenderSetting() { return RenderSetting; }

    SSplitter*      GetSplitterV() const;
    SSplitter*      GetSplitterH() const;
    EViewportLayout GetViewportLayout() const { return ViewportLayout; }

    // ── Splitter drag interaction ────────────────────────────────────────────
    // Returns true if the pixel X is within the grab zone of the vertical splitter
    bool HitTestSplitterV(int32 X, int32 Y) const;
    // Returns true if the pixel Y is within the grab zone of the horizontal splitter
    bool HitTestSplitterH(int32 X, int32 Y) const;
    // Start dragging; pass which axes are active
    void BeginSplitterDrag(bool bVertical, bool bHorizontal);
    // Feed per-frame mouse delta while dragging
    void UpdateSplitterDrag(float DeltaX, float DeltaY);
    // Release drag lock
    void EndSplitterDrag();
    bool IsDraggingSplitter() const { return bDraggingV || bDraggingH; }

    float GetVSplitRatio() const { return VSplitRatio; }
    float GetHSplitRatio() const { return HSplitRatio; }
    FEditorViewportPanel* GetLastFocusedPanel() const { return LastFocusedPanel; }

    // 실제 화면에 그려지는 게임 월드 뷰포트 영역의 위치와 크기를 설정
    void SetViewportAvailableArea(float X, float Y, float Width, float Height);

  private:
    // Push current panel PosX/Y/Width/Height to each ViewportClient
    void SyncPanelClients();

    // Reset view orientation for all panels based on current layout
    void ResetViewOrientation();
};
