#include "ViewportLayout.h"
#include "Runtime/WindowsWindow.h"
#include "Selection/SelectionManager.h"
#include "Editor/Settings/EditorSettings.h"
#include "Slate/SViewport.h"
#include "Slate/SSplitterH.h"
#include "Slate/SSplitterV.h"
#include "Slate/SSplitterCross.h"
#include "Slate/SlateApplication.h"
#include "Input/InputSystem.h"
#include "Engine/Component/GizmoComponent.h"
#include "EditorEngine.h"

#include <algorithm>

//  뷰포트 타입 테이블  [인덱스 → EEditorViewportType]
static constexpr EEditorViewportType kViewportTypes[FEditorViewportLayout::MaxViewports] =
{
	EVT_Perspective,   // 0 : 좌상단 (원근)
	EVT_OrthoTop,      // 1 : 우상단 (탑 뷰)
	EVT_OrthoFront,    // 2 : 좌하단 (프론트 뷰)
	EVT_OrthoRight,    // 3 : 우하단 (라이트 뷰)
};

void FEditorViewportLayout::Init(FWindowsWindow* InWindow, UWorld* World, FSelectionManager* SelectionManager, UEditorEngine* EditorEngine)
{
	Window = InWindow;
	Editor = EditorEngine;

    // Client 포인터 소유/생성 주체는 Layout입니다.
    // 각 SceneViewport는 Layout이 소유한 Client 인스턴스를 참조만 합니다.
    for (int32 i = 0; i < MaxViewports; ++i)
    {
        ViewportWidgets[i].GetSceneViewport().SetClient(&ViewportClients[i]);
    }

	// Settings 에서 레이아웃 상태 복원
	const FEditorSettings& S = FEditorSettings::Get();
	SingleViewportIndex = S.SingleViewportIndex;
	const int32 SavedLayoutMode = S.ViewportLayoutMode;
	LayoutMode = (SavedLayoutMode >= 0 && SavedLayoutMode < static_cast<int32>(EEditorViewportLayoutMode::Max))
		? static_cast<EEditorViewportLayoutMode>(SavedLayoutMode)
		: (S.ActiveViewportCount == 1 ? EEditorViewportLayoutMode::OnePane : EEditorViewportLayoutMode::FourPanes2x2);
	bSingleViewport = (LayoutMode == EEditorViewportLayoutMode::OnePane);
	if (LayoutMode != EEditorViewportLayoutMode::OnePane)
	{
		LastSplitLayoutMode = LayoutMode;
	}

	// 초기 뷰포트 영역 설정 (SyncViewportRects 에서 최종 덮어씌워짐)
	InitViewportRect(static_cast<uint32>(Window->GetWidth()),
		static_cast<uint32>(Window->GetHeight()));

	// 4개 뷰포트 클라이언트 초기화
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		ViewportWidgets[i].GetSceneViewport().GetClient()->SetSettings(&FEditorSettings::Get());
		ViewportWidgets[i].GetSceneViewport().GetClient()->Initialize(Window, EditorEngine);
		ViewportWidgets[i].GetSceneViewport().GetClient()->SetWorld(World);
		ViewportWidgets[i].GetSceneViewport().GetClient()->SetGizmo(SelectionManager->GetGizmo());
		ViewportWidgets[i].GetSceneViewport().GetClient()->SetSelectionManager(SelectionManager);
		
		// 상호 참조 연결
        ViewportWidgets[i].GetSceneViewport().GetClient()->SetViewport(&ViewportWidgets[i].GetSceneViewport());
        ViewportWidgets[i].GetSceneViewport().GetClient()->SetState(&ViewportWidgets[i].GetSceneViewport().GetState());

		// 뷰포트 타입 설정 후 카메라 생성
		ViewportWidgets[i].GetSceneViewport().GetClient()->SetViewportType(kViewportTypes[i]);
		ViewportWidgets[i].GetSceneViewport().GetClient()->CreateCamera();
		ViewportWidgets[i].GetSceneViewport().GetClient()->ApplyCameraMode();
	}
}

void FEditorViewportLayout::Shutdown()
{
	DestroyViewportLayout();
}

void FEditorViewportLayout::UpdateHoverStates()
{
    // 1. 이번 프레임의 우클릭/중클릭 조작 상태를 먼저 읽습니다.
    const InputSystem& IS = InputSystem::Get();

    const bool bOperationPressed =
        IS.GetKeyDown(VK_RBUTTON) || IS.GetKeyDown(VK_MBUTTON);

    const bool bOperationDragging =
        IS.GetRightDragging() || IS.GetMiddleDragging();

    const bool bOperationReleased =
        IS.GetKeyUp(VK_RBUTTON) || IS.GetKeyUp(VK_MBUTTON);

    // 2. 우클릭/중클릭 조작이 끝나면 뷰포트 독점 조작 상태와 차단 상태를 함께 해제합니다.
    if (bOperationReleased)
    {
        ActiveOperationViewportIndex = -1;
        bBlockViewportOperationUntilRelease = false;
    }

    // 3. 스플리터 드래그로 바뀐 뷰포트 Rect를 먼저 동기화합니다.
    //    이후 hover 계산은 항상 최신 Rect 기준으로 수행되어야 합니다.
    if (GetRootSplitterV())
    {
        SyncViewportRects();
    }

    // 4. 기즈모를 드래그 중이면 현재 조작을 유지하고 hover 갱신을 건너뜁니다.
    for (int i = 0; i < MaxViewports; ++i)
    {
        if (ViewportWidgets[i].GetSceneViewport().GetClient()->GetGizmo()->IsHolding())
            return;
    }

    // 5. 마우스 좌표를 윈도우 client 기준으로 가져옵니다.
    if (!Window)
        return;
	
    POINT MousePt = InputSystem::Get().GetMousePos();
	MousePt = Window->ScreenToClientPoint(MousePt);
	const int32 MouseX = static_cast<int32>(MousePt.x);
	const int32 MouseY = static_cast<int32>(MousePt.y);

	// 6. 현재 마우스가 viewport host 안에 있는지 확인합니다.
	const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();
    const bool bInViewportHost = GuiState.IsInViewportHost(MouseX, MouseY);
    const bool bViewportMouseBlockedByUI = GuiState.bBlockViewportMouse && !GuiState.bAllowViewportMouseFocus;

    if (bViewportMouseBlockedByUI)
    {
        ActiveOperationViewportIndex = -1;
        if (bOperationPressed)
        {
            bBlockViewportOperationUntilRelease = true;
        }
        for (int32 i = 0; i < MaxViewports; ++i)
        {
            GetViewportState(i).bHovered = false;
        }
        return;
    }

    // 7. 우클릭/중클릭이 눌린 순간의 시작 위치를 먼저 기록합니다.
    //    뷰포트 위에서 시작한 조작이면 해당 뷰포트를 고정하고,
    //    뷰포트 밖에서 시작한 조작이면 버튼이 올라갈 때까지 뷰포트 입력을 차단합니다.
    if (bOperationPressed)
    {
        if (bInViewportHost)
        {
            const int32 PressedViewport = FindViewportIndexAt(MouseX, MouseY);
            if (PressedViewport >= 0)
            {
                ActiveOperationViewportIndex = PressedViewport;
                bBlockViewportOperationUntilRelease = false;
                SetLastFocusedViewportIndex(PressedViewport);
            }
            else
            {
                ActiveOperationViewportIndex = -1;
                bBlockViewportOperationUntilRelease = true;
            }
        }
        else
        {
            ActiveOperationViewportIndex = -1;
            bBlockViewportOperationUntilRelease = true;
        }
    }

	// 8. Viewport host 밖이면 모든 hover를 해제합니다.
    //    현재 구현에서는 host 밖으로 나간 순간 뷰포트 독점 조작 대상도 함께 초기화합니다.
	if (!bInViewportHost)
	{
		for (int32 i = 0; i < MaxViewports; ++i)
		{
			GetViewportState(i).bHovered = false;
		}
        if (!bBlockViewportOperationUntilRelease)
        {
            ActiveOperationViewportIndex = -1;
        }

		return;
	}

	// 9. ImGui 패널 등 뷰포트 밖에서 조작이 시작된 경우,
    //    버튼이 올라갈 때까지 어떤 뷰포트도 hovered 상태가 되지 않도록 막습니다.
	if (bBlockViewportOperationUntilRelease)
    {
        for (int32 i = 0; i < MaxViewports; ++i)
        {
            GetViewportState(i).bHovered = false;
        }
        return;
    }

    // 10. 독점 조작 중이면 해당 뷰포트만 hovered 상태로 유지합니다.
    //     조작 중 마우스가 다른 뷰포트로 이동해도 입력이 누수되지 않도록 막습니다.
    if (ActiveOperationViewportIndex >= 0 && bOperationDragging)
    {
        for (int32 i = 0; i < MaxViewports; ++i)
        {
            GetViewportState(i).bHovered = (i == ActiveOperationViewportIndex);
        }

        SetLastFocusedViewportIndex(ActiveOperationViewportIndex);
        return;
    }

    // 11. 평상시에는 현재 마우스가 위치한 뷰포트만 hovered 상태로 갱신합니다.
    const int32 HoveredViewport = FindViewportIndexAt(MouseX, MouseY);
    for (int32 i = 0; i < MaxViewports; ++i)
    {
        GetViewportState(i).bHovered = (i == HoveredViewport);
    }

    // 12. 좌클릭으로 상호작용을 시작한 뷰포트를 마지막 포커스 대상으로 기록합니다.
    if (HoveredViewport >= 0 && IS.GetKeyDown(VK_LBUTTON))
    {
        SetLastFocusedViewportIndex(HoveredViewport);
    }
}

void FEditorViewportLayout::Tick(float DeltaTime)
{
	TickLayoutTransition(DeltaTime);
	UpdateHoverStates();
	// bHovered 가 설정된 뷰포트만 입력을 처리합니다.
	for (int32 i = 0; i < FEditorViewportLayout::MaxViewports; ++i)
	{
		GetViewportClient(i)->Tick(DeltaTime);
	}
}

void FEditorViewportLayout::OnWindowResized(uint32 Width, uint32 Height)
{
	// 윈도우 리사이즈(최대화/복원 포함) 시 기존 HostRect는 이전 프레임 값일 수 있으므로 무효화합니다.
	// 이후 RenderViewportHostWindow에서 최신 HostRect가 다시 설정됩니다.
	HostRect = FViewportRect();

	// 스플리터 트리 재배치 + SViewport → ISlateViewport 동기화
	if (GetRootSplitterV())
	{
		const FViewportRect LayoutRect(0, 0, static_cast<int32>(Width), static_cast<int32>(Height));
		GetRootSplitterV()->SetRect({
			static_cast<float>(LayoutRect.X),
			static_cast<float>(LayoutRect.Y),
			static_cast<float>(LayoutRect.Width),
			static_cast<float>(LayoutRect.Height)
		});
		GetRootSplitterV()->UpdateChildRect();
		SyncViewportRects();
	}
}

void FEditorViewportLayout::SetHostRect(const FViewportRect& InHostRect)
{
	HostRect = InHostRect;

	if (!RootSplitterV)
	{
		return;
	}

	RootSplitterV->SetRect({
		static_cast<float>(HostRect.X),
		static_cast<float>(HostRect.Y),
		static_cast<float>(HostRect.Width),
		static_cast<float>(HostRect.Height)
	});
	RootSplitterV->UpdateChildRect();
	SyncViewportRects();
}

// 영역 계산 헬퍼
void FEditorViewportLayout::InitViewportRect(uint32 Width, uint32 Height)
{
	const int32 W = static_cast<int32>(Width);
	const int32 H = static_cast<int32>(Height);

	// 50:50 초기 분할 (이후 BuildViewportLayout → SyncViewportRects 에서 최종 반영)
	const int32 HalfW = W / 2;
	const int32 HalfH = H / 2;
    
	FViewportRect Rects[4] = {
        { 0, 0, HalfW, HalfH },
        { HalfW, 0, W - HalfW, HalfH },
        { 0, HalfH, HalfW, H - HalfH },
        { HalfW, HalfH, W - HalfW, H - HalfH }
    };
	
	for (int32 i = 0; i < MaxViewports; ++i)
	{
        ViewportWidgets[i].GetSceneViewport().SetRect(Rects[i]);
		ViewportWidgets[i].GetSceneViewport().GetClient()->SetViewportSize(
			static_cast<float>(Rects[i].Width),
			static_cast<float>(Rects[i].Height));
	}
}

//  Viewport Layout 생성 (2 x 2)
void FEditorViewportLayout::BuildViewportLayout(int32 Width, int32 Height)
{
	DestroyViewportLayout();  // 기존 위젯이 있으면 먼저 해제

	// 4개 SViewport 생성 + ISlateViewport(FSceneViewport) 연결
	for (int32 i = 0; i < MaxViewports; ++i)
	{
        FSceneViewport& VP = ViewportWidgets[i].GetSceneViewport();
        // Build 시 DestroyViewportLayout()에서 끊어진 Client-Viewport-State 연결을 복구한다.
        VP.SetClient(&ViewportClients[i]);
        if (FEditorViewportClient* VC = ViewportWidgets[i].GetSceneViewport().GetClient())
        {
            VC->SetViewport(&ViewportWidgets[i].GetSceneViewport());
            VC->SetState(&ViewportWidgets[i].GetSceneViewport().GetState());
        }

		FViewportRenderResource RenderResource = Editor->GetRenderer().AcquireViewportResource(&VP, VP.GetRect().Width, VP.GetRect().Height, i);
        VP.SetRenderTargetSet(&RenderResource.GetView());
        ViewportWidgets[i].SetViewportInterface(&VP);
	}

	// 스플리터 트리 구성
	//   SSplitterV (루트, 위/아래)
	//     SideLT (위)   = TopSplitterH → [0] 좌상단(Perspective), [1] 우상단(Top)
	//     SideRB (아래) = BotSplitterH → [2] 좌하단(Front),       [3] 우하단(Right)
	TopSplitterH = new SSplitterH();
	TopSplitterH->SetSideLT(&ViewportWidgets[0]);
	TopSplitterH->SetSideRB(&ViewportWidgets[1]);

	BotSplitterH = new SSplitterH();
	BotSplitterH->SetSideLT(&ViewportWidgets[2]);
	BotSplitterH->SetSideRB(&ViewportWidgets[3]);

	RootSplitterV = new SSplitterV();
	RootSplitterV->SetSideLT(TopSplitterH);
	RootSplitterV->SetSideRB(BotSplitterH);

	// 세로 바를 항상 같은 X 비율로 유지 (TopSplitterH ↔ BotSplitterH)
	TopSplitterH->SetLinkedSplitter(BotSplitterH);
	BotSplitterH->SetLinkedSplitter(TopSplitterH);

	// 교차점 핸들: V 바와 H 바가 겹치는 영역을 드래그하면 4개 뷰포트 동시 조정
	CrossWidget = new SSplitterCross();
	CrossWidget->SetSplitterV(RootSplitterV);
	CrossWidget->SetSplitterH(TopSplitterH);
	RootSplitterV->SetCrossWidget(CrossWidget);

	// FSlateApplication::RootWindow 에 트리 연결 (소유권은 이쪽)
	SWindow* RootWindow = FSlateApplication::Get().GetRootWindow();
	if (RootWindow)
	{
		RootWindow->SetRect({ 0.f, 0.f, static_cast<float>(Width), static_cast<float>(Height) });
	}

	// 초기 크기 → 자식 영역 재귀 계산
	const FViewportRect LayoutRect = (HostRect.Width > 0 && HostRect.Height > 0)
		? HostRect
		: FViewportRect(0, 0, Width, Height);
	RootSplitterV->SetRect({
		static_cast<float>(LayoutRect.X),
		static_cast<float>(LayoutRect.Y),
		static_cast<float>(LayoutRect.Width),
		static_cast<float>(LayoutRect.Height)
	});

	// 저장된 스플리터 비율 복원 (UpdateCildRect 전에 설정해야 올바르게 분배됨)
	const float VRatio = FEditorSettings::Get().SplitterVRatio;
	const float HRatio = FEditorSettings::Get().SplitterHRatio;
	RootSplitterV->SetSplitRatio(VRatio);
	TopSplitterH->SetSplitRatio(HRatio);
	BotSplitterH->SetSplitRatio(HRatio);

	RootSplitterV->UpdateChildRect();

	// SViewport(FRect) → FSceneViewport::SetRect(FViewportRect) 동기화
	SyncViewportRects();
	UpdateSlateSplitterAttachment();
}

void FEditorViewportLayout::SetSingleViewportMode(bool bSingle, int32 Index)
{
	SetLayoutMode(bSingle ? EEditorViewportLayoutMode::OnePane : EEditorViewportLayoutMode::FourPanes2x2, Index);
}

void FEditorViewportLayout::SetLayoutMode(EEditorViewportLayoutMode InMode, int32 FocusIndex)
{
	bLayoutTransitionActive = false;
	LayoutTransitionElapsed = 0.0f;

	if (InMode < EEditorViewportLayoutMode::OnePane || InMode >= EEditorViewportLayoutMode::Max)
	{
		InMode = EEditorViewportLayoutMode::FourPanes2x2;
	}

	if (FocusIndex >= 0)
	{
		SingleViewportIndex = (FocusIndex >= MaxViewports) ? MaxViewports - 1 : FocusIndex;
		SetLastFocusedViewportIndex(SingleViewportIndex);
	}
	else if (InMode == EEditorViewportLayoutMode::OnePane)
	{
		SingleViewportIndex = LastFocusedViewportIndex;
	}

	LayoutMode = InMode;
	bSingleViewport = (LayoutMode == EEditorViewportLayoutMode::OnePane);
	if (!bSingleViewport)
	{
		LastSplitLayoutMode = LayoutMode;
	}

	FEditorSettings::Get().ViewportLayoutMode = static_cast<int32>(LayoutMode);
	FEditorSettings::Get().ActiveViewportCount = GetLayoutSlotCount(LayoutMode);
	FEditorSettings::Get().SingleViewportIndex = SingleViewportIndex;
	SyncViewportRects();
	UpdateSlateSplitterAttachment();
}

void FEditorViewportLayout::SetLayoutModeAnimated(EEditorViewportLayoutMode InMode, int32 FocusIndex)
{
	if (InMode < EEditorViewportLayoutMode::OnePane || InMode >= EEditorViewportLayoutMode::Max)
	{
		InMode = EEditorViewportLayoutMode::FourPanes2x2;
	}

	if (!RootSplitterV)
	{
		SetLayoutMode(InMode, FocusIndex);
		return;
	}

	if (InMode == LayoutMode && (InMode != EEditorViewportLayoutMode::OnePane || FocusIndex < 0 || FocusIndex == SingleViewportIndex))
	{
		return;
	}

	for (int32 i = 0; i < MaxViewports; ++i)
	{
		LayoutTransitionStartRects[i] = GetSceneViewport(i).GetRect();
	}
	const FViewportRect AnchorSourceRect = LayoutTransitionStartRects[0];
	const FRect RootRect = RootSplitterV->GetRect();
	const int32 TransitionAnchorX = AnchorSourceRect.Width > 0 ? AnchorSourceRect.X : static_cast<int32>(RootRect.X);
	const int32 TransitionAnchorY = AnchorSourceRect.Height > 0 ? AnchorSourceRect.Y : static_cast<int32>(RootRect.Y);
	auto AnchorHiddenRect = [TransitionAnchorX, TransitionAnchorY](FViewportRect& Rect)
	{
		if (Rect.Width <= 0 || Rect.Height <= 0)
		{
			Rect.X = TransitionAnchorX;
			Rect.Y = TransitionAnchorY;
		}
	};
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		AnchorHiddenRect(LayoutTransitionStartRects[i]);
	}

	if (FocusIndex >= 0)
	{
		SingleViewportIndex = std::clamp(FocusIndex, 0, MaxViewports - 1);
		SetLastFocusedViewportIndex(SingleViewportIndex);
	}
	else if (InMode == EEditorViewportLayoutMode::OnePane)
	{
		SingleViewportIndex = LastFocusedViewportIndex;
	}

	LayoutMode = InMode;
	bSingleViewport = (LayoutMode == EEditorViewportLayoutMode::OnePane);
	if (!bSingleViewport)
	{
		LastSplitLayoutMode = LayoutMode;
	}

	FEditorSettings::Get().ViewportLayoutMode = static_cast<int32>(LayoutMode);
	FEditorSettings::Get().ActiveViewportCount = GetLayoutSlotCount(LayoutMode);
	FEditorSettings::Get().SingleViewportIndex = SingleViewportIndex;

	RootSplitterV->UpdateChildRect();
	ComputeLayoutRects(LayoutMode, SingleViewportIndex, RootSplitterV->GetRect(), LayoutTransitionTargetRects);
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		AnchorHiddenRect(LayoutTransitionTargetRects[i]);
	}

	bLayoutTransitionActive = true;
	LayoutTransitionElapsed = 0.0f;
	UpdateSlateSplitterAttachment();
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		SetViewportRect(i, LayoutTransitionStartRects[i]);
	}
}

void FEditorViewportLayout::ToggleViewportSplit()
{
	if (LayoutMode == EEditorViewportLayoutMode::OnePane)
	{
		SetLayoutModeAnimated(LastSplitLayoutMode);
	}
	else
	{
		SetLayoutModeAnimated(EEditorViewportLayoutMode::OnePane, LastFocusedViewportIndex);
	}
}

int32 FEditorViewportLayout::GetActiveViewportCount() const
{
	return GetLayoutSlotCount(LayoutMode);
}

void FEditorViewportLayout::SetLastFocusedViewportIndex(int32 Index)
{
	if (Index < 0) Index = 0;
	if (Index >= MaxViewports) Index = MaxViewports - 1;
	LastFocusedViewportIndex = Index;

	FEditorViewportClient* MainViewport = GetViewportClient(LastFocusedViewportIndex);
}

void FEditorViewportLayout::SyncViewportRects()
{
	if (!RootSplitterV)
	{
		return;
	}

	if (bLayoutTransitionActive)
	{
		return;
	}

	const FRect& Full = RootSplitterV->GetRect();

	// 1개 모드: SingleViewportIndex 뷰포트에 전체 영역 할당, 나머지는 크기 0
	if (LayoutMode == EEditorViewportLayoutMode::OnePane)
	{
		for (int32 i = 0; i < MaxViewports; ++i)
		{
			if (i == SingleViewportIndex)
			{
				const FViewportRect VR(
					static_cast<int32>(Full.X),
					static_cast<int32>(Full.Y),
					static_cast<int32>(Full.Width),
					static_cast<int32>(Full.Height));
				SetViewportRect(i, VR);
			}
			else
			{
				SetViewportRect(i, FViewportRect(0, 0, 0, 0));
			}
		}
		return;
	}

	if (LayoutMode != EEditorViewportLayoutMode::FourPanes2x2)
	{
		ApplyPresetViewportRects(Full);
		return;
	}

	// 4분할 모드: 스플리터 트리에서 각 SViewport 의 FRect 를 읽어 반영
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		// if (!ViewportWidgets[i]) continue;

		const FRect& R = ViewportWidgets[i].GetRect();
		const FViewportRect VR(
			static_cast<int32>(R.X),
			static_cast<int32>(R.Y),
			static_cast<int32>(R.Width),
			static_cast<int32>(R.Height)
		);

		// 스플리터 드래그로 바뀐 크기를 ViewportState, SceneViewport,
		// ViewportClient 카메라 종횡비에 모두 반영합니다.
		SetViewportRect(i, VR);
	}
}

void FEditorViewportLayout::SetViewportRect(int32 Index, const FViewportRect& Rect)
{
	if (Index < 0 || Index >= MaxViewports)
	{
		return;
	}

	ViewportWidgets[Index].GetSceneViewport().SetRect(Rect);
	if (Rect.Width > 0 && Rect.Height > 0)
	{
		ViewportWidgets[Index].GetSceneViewport().GetClient()->SetViewportSize(
			static_cast<float>(Rect.Width),
			static_cast<float>(Rect.Height));
	}
}

int32 FEditorViewportLayout::GetLayoutSlotCount(EEditorViewportLayoutMode InMode)
{
	switch (InMode)
	{
	case EEditorViewportLayoutMode::OnePane:
		return 1;
	case EEditorViewportLayoutMode::TwoPanesHoriz:
	case EEditorViewportLayoutMode::TwoPanesVert:
		return 2;
	case EEditorViewportLayoutMode::ThreePanesLeft:
	case EEditorViewportLayoutMode::ThreePanesRight:
	case EEditorViewportLayoutMode::ThreePanesTop:
	case EEditorViewportLayoutMode::ThreePanesBottom:
		return 3;
	default:
		return 4;
	}
}

void FEditorViewportLayout::ApplyPresetViewportRects(const FRect& FullRect)
{
	FViewportRect Rects[MaxViewports] = {};
	ComputeLayoutRects(LayoutMode, SingleViewportIndex, FullRect, Rects);
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		SetViewportRect(i, Rects[i]);
	}
}

void FEditorViewportLayout::ComputeLayoutRects(EEditorViewportLayoutMode InMode, int32 InSingleViewportIndex, const FRect& FullRect, FViewportRect (&OutRects)[MaxViewports]) const
{
	const int32 X = static_cast<int32>(FullRect.X);
	const int32 Y = static_cast<int32>(FullRect.Y);
	const int32 W = static_cast<int32>(FullRect.Width);
	const int32 H = static_cast<int32>(FullRect.Height);
	const int32 HalfW = W / 2;
	const int32 HalfH = H / 2;
	const int32 ThirdW = W / 3;
	const int32 ThirdH = H / 3;

	for (int32 i = 0; i < MaxViewports; ++i)
	{
		OutRects[i] = FViewportRect(0, 0, 0, 0);
	}

	if (InMode == EEditorViewportLayoutMode::OnePane)
	{
		const int32 ClampedSingleViewportIndex = std::clamp(InSingleViewportIndex, 0, MaxViewports - 1);
		OutRects[ClampedSingleViewportIndex] = FViewportRect(X, Y, W, H);
		return;
	}

	if (InMode == EEditorViewportLayoutMode::FourPanes2x2)
	{
		for (int32 i = 0; i < MaxViewports; ++i)
		{
			const FRect& R = ViewportWidgets[i].GetRect();
			OutRects[i] = FViewportRect(
				static_cast<int32>(R.X),
				static_cast<int32>(R.Y),
				static_cast<int32>(R.Width),
				static_cast<int32>(R.Height));
		}
		return;
	}

	switch (InMode)
	{
	case EEditorViewportLayoutMode::TwoPanesHoriz:
		OutRects[0] = FViewportRect(X, Y, HalfW, H);
		OutRects[1] = FViewportRect(X + HalfW, Y, W - HalfW, H);
		break;
	case EEditorViewportLayoutMode::TwoPanesVert:
		OutRects[0] = FViewportRect(X, Y, W, HalfH);
		OutRects[1] = FViewportRect(X, Y + HalfH, W, H - HalfH);
		break;
	case EEditorViewportLayoutMode::ThreePanesLeft:
		OutRects[0] = FViewportRect(X, Y, HalfW, H);
		OutRects[1] = FViewportRect(X + HalfW, Y, W - HalfW, HalfH);
		OutRects[2] = FViewportRect(X + HalfW, Y + HalfH, W - HalfW, H - HalfH);
		break;
	case EEditorViewportLayoutMode::ThreePanesRight:
		OutRects[0] = FViewportRect(X, Y, HalfW, HalfH);
		OutRects[1] = FViewportRect(X, Y + HalfH, HalfW, H - HalfH);
		OutRects[2] = FViewportRect(X + HalfW, Y, W - HalfW, H);
		break;
	case EEditorViewportLayoutMode::ThreePanesTop:
		OutRects[0] = FViewportRect(X, Y, W, HalfH);
		OutRects[1] = FViewportRect(X, Y + HalfH, HalfW, H - HalfH);
		OutRects[2] = FViewportRect(X + HalfW, Y + HalfH, W - HalfW, H - HalfH);
		break;
	case EEditorViewportLayoutMode::ThreePanesBottom:
		OutRects[0] = FViewportRect(X, Y, HalfW, HalfH);
		OutRects[1] = FViewportRect(X + HalfW, Y, W - HalfW, HalfH);
		OutRects[2] = FViewportRect(X, Y + HalfH, W, H - HalfH);
		break;
	case EEditorViewportLayoutMode::FourPanesLeft:
		OutRects[0] = FViewportRect(X, Y, HalfW, H);
		OutRects[1] = FViewportRect(X + HalfW, Y, W - HalfW, ThirdH);
		OutRects[2] = FViewportRect(X + HalfW, Y + ThirdH, W - HalfW, ThirdH);
		OutRects[3] = FViewportRect(X + HalfW, Y + ThirdH * 2, W - HalfW, H - ThirdH * 2);
		break;
	case EEditorViewportLayoutMode::FourPanesRight:
		OutRects[0] = FViewportRect(X, Y, HalfW, ThirdH);
		OutRects[1] = FViewportRect(X, Y + ThirdH, HalfW, ThirdH);
		OutRects[2] = FViewportRect(X, Y + ThirdH * 2, HalfW, H - ThirdH * 2);
		OutRects[3] = FViewportRect(X + HalfW, Y, W - HalfW, H);
		break;
	case EEditorViewportLayoutMode::FourPanesTop:
		OutRects[0] = FViewportRect(X, Y, W, HalfH);
		OutRects[1] = FViewportRect(X, Y + HalfH, ThirdW, H - HalfH);
		OutRects[2] = FViewportRect(X + ThirdW, Y + HalfH, ThirdW, H - HalfH);
		OutRects[3] = FViewportRect(X + ThirdW * 2, Y + HalfH, W - ThirdW * 2, H - HalfH);
		break;
	case EEditorViewportLayoutMode::FourPanesBottom:
		OutRects[0] = FViewportRect(X, Y, ThirdW, HalfH);
		OutRects[1] = FViewportRect(X + ThirdW, Y, ThirdW, HalfH);
		OutRects[2] = FViewportRect(X + ThirdW * 2, Y, W - ThirdW * 2, HalfH);
		OutRects[3] = FViewportRect(X, Y + HalfH, W, H - HalfH);
		break;
	default:
		break;
	}
}

void FEditorViewportLayout::TickLayoutTransition(float DeltaTime)
{
	if (!bLayoutTransitionActive)
	{
		return;
	}

	LayoutTransitionElapsed += DeltaTime;
	const float T = std::clamp(LayoutTransitionElapsed / LayoutTransitionDuration, 0.0f, 1.0f);
	const float SmoothT = T * T * (3.0f - 2.0f * T);

	auto LerpInt = [SmoothT](int32 A, int32 B)
	{
		return static_cast<int32>(static_cast<float>(A) + (static_cast<float>(B - A) * SmoothT) + 0.5f);
	};

	for (int32 i = 0; i < MaxViewports; ++i)
	{
		const FViewportRect& A = LayoutTransitionStartRects[i];
		const FViewportRect& B = LayoutTransitionTargetRects[i];
		SetViewportRect(i, FViewportRect(
			LerpInt(A.X, B.X),
			LerpInt(A.Y, B.Y),
			std::max(0, LerpInt(A.Width, B.Width)),
			std::max(0, LerpInt(A.Height, B.Height))));
	}

	if (T >= 1.0f)
	{
		EndLayoutTransition();
	}
}

void FEditorViewportLayout::EndLayoutTransition()
{
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		SetViewportRect(i, LayoutTransitionTargetRects[i]);
	}
	bLayoutTransitionActive = false;
	LayoutTransitionElapsed = 0.0f;
	UpdateSlateSplitterAttachment();
}

void FEditorViewportLayout::UpdateSlateSplitterAttachment()
{
	SWindow* RootWindow = FSlateApplication::Get().GetRootWindow();
	if (!RootWindow)
	{
		return;
	}

	const bool bShouldAttachSplitter =
		RootSplitterV != nullptr
		&& !bLayoutTransitionActive
		&& LayoutMode == EEditorViewportLayoutMode::FourPanes2x2;
	RootWindow->SetChild(bShouldAttachSplitter ? RootSplitterV : nullptr);
}

void FEditorViewportLayout::DestroyViewportLayout()
{
	// SlateApplication 이 보유한 Widget 먼저 해제
	FSlateApplication::Get().ClearWidgetRefs();

	// SlateApplication RootWindow 의 자식 참조 해제
	if (SWindow* RootWindow = FSlateApplication::Get().GetRootWindow())
		RootWindow->SetChild(nullptr);

	// 뷰포트 위젯 삭제 전에 SideLT/SideRB 참조 끊기
	if (TopSplitterH) { TopSplitterH->SetSideLT(nullptr); TopSplitterH->SetSideRB(nullptr); }
	if (BotSplitterH) { BotSplitterH->SetSideLT(nullptr); BotSplitterH->SetSideRB(nullptr); }
	if (RootSplitterV) { RootSplitterV->SetSideLT(nullptr); RootSplitterV->SetSideRB(nullptr); }

	// LinkedSplitter 양방향 참조 해제 (delete 순서와 무관하게 dangling 방지)
	if (TopSplitterH) TopSplitterH->SetLinkedSplitter(nullptr);
	if (BotSplitterH) BotSplitterH->SetLinkedSplitter(nullptr);

	// CrossWidget 참조 해제 후 삭제
	if (RootSplitterV) RootSplitterV->SetCrossWidget(nullptr);
	delete CrossWidget; CrossWidget = nullptr;

	for (int32 i = 0; i < MaxViewports; ++i)
	{
		// delete ViewportWidgets[i];
		// ViewportWidgets[i] = nullptr;
        if (FEditorViewportClient* VC = ViewportWidgets[i].GetSceneViewport().GetClient())
        {
            VC->SetViewport(nullptr);
            VC->SetState(nullptr);
        }

		FSceneViewport& VP = ViewportWidgets[i].GetSceneViewport();
        Editor->GetRenderer().ReleaseViewportResource(&VP, i);
        VP.SetClient(nullptr);

	}
	delete TopSplitterH; TopSplitterH = nullptr;
	delete BotSplitterH; BotSplitterH = nullptr;
	delete RootSplitterV; RootSplitterV = nullptr;
}

int32 FEditorViewportLayout::FindViewportIndexAt(int32 MouseX, int32 MouseY) const
{
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		const FViewportRect& VR = GetSceneViewport(i).GetRect();
		if (VR.Contains(MouseX, MouseY))
		{
			return i;
		}
	}
    return -1;
}
