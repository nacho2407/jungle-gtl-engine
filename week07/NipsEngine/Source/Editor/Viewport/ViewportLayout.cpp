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

//  뷰포트 타입 테이블  [인덱스 → EEditorViewportType]
static constexpr EEditorViewportType kViewportTypes[FViewportLayout::MaxViewports] =
{
	EVT_Perspective,   // 0 : 좌상단 (원근)
	EVT_OrthoTop,      // 1 : 우상단 (탑 뷰)
	EVT_OrthoFront,    // 2 : 좌하단 (프론트 뷰)
	EVT_OrthoRight,    // 3 : 우하단 (라이트 뷰)
};

void FViewportLayout::Init(FWindowsWindow* InWindow, UWorld* World, FSelectionManager* SelectionManager, UEditorEngine* EditorEngine)
{
	Window = InWindow;
	Editor = EditorEngine;
	// Settings 에서 레이아웃 상태 복원
	const FEditorSettings& S = FEditorSettings::Get();
	bSingleViewport    = (S.ActiveViewportCount == 1);
	SingleViewportIndex = S.SingleViewportIndex;

	// 초기 뷰포트 영역 설정 (SyncViewportRects 에서 최종 덮어씌워짐)
	InitViewportRect(static_cast<uint32>(Window->GetWidth()),
		static_cast<uint32>(Window->GetHeight()));

	// 4개 뷰포트 클라이언트 초기화
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		ViewportClients[i].SetSettings(&FEditorSettings::Get());
		ViewportClients[i].Initialize(Window, EditorEngine);
		ViewportClients[i].SetWorld(World);
		ViewportClients[i].SetGizmo(SelectionManager->GetGizmo());
		ViewportClients[i].SetSelectionManager(SelectionManager);
		
		// 상호 참조 연결
		SceneViewports[i].SetClient(&ViewportClients[i]);
		ViewportClients[i].SetViewport(&SceneViewports[i]);
		ViewportClients[i].SetState(&ViewportStates[i]);

		// 뷰포트 타입 설정 후 카메라 생성
		ViewportClients[i].SetViewportType(kViewportTypes[i]);
		ViewportClients[i].CreateCamera();
		ViewportClients[i].ApplyCameraMode();
	}
}

void FViewportLayout::Shutdown()
{
	DestroyViewportLayout();
}

void FViewportLayout::UpdateHoverStates()
{
	// 1. PumpMessages 에서 처리된 WM_MOUSEMOVE 드래그 결과를 즉시 뷰포트 Rect 에 반영
	//    이렇게 해야 아래 bHovered 계산이 현재 프레임 Rect 를 사용할 수 있습니다.
	if (GetRootSplitterV())
	{
		SyncViewportRects();
	}

	// 1 - 1. 특정 뷰포트에서 기즈모 홀딩중이라면 스킵합니다.
	for (int i = 0; i < MaxViewports; ++i)
	{
		if (ViewportClients[i].GetGizmo()->IsHolding())
			return;
	}

	// 2. 어느 뷰포트에 마우스가 있는지 판단합니다.
	if (!Window)
		return;
	
	POINT MousePt = InputSystem::Get().GetMousePos();
	MousePt = Window->ScreenToClientPoint(MousePt);
	const int32 MouseX = static_cast<int32>(MousePt.x);
	const int32 MouseY = static_cast<int32>(MousePt.y);

	// 독점 조작 중(회전·팬·궤도)인 뷰포트가 있으면 해당 뷰포트만 hovered 유지합니다.
	// 조작 중 마우스가 다른 뷰포트로 이동해도 입력이 누수되지 않도록 막습니다.

	const FGuiInputState& GuiState = InputSystem::Get().GetGuiInputState();

	// Viewport host 밖이면 모든 hover를 해제합니다.

	if (!GuiState.IsInViewportHost(MouseX, MouseY))
	{
		for (int32 i = 0; i < MaxViewports; ++i)
		{
			GetViewportState(i).bHovered = false;
		}
		return;
	}

	// Find Active Viewport 
	int32 ActiveOpViewport = -1;
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		if (GetViewportClient(i).IsActiveOperation())
		{
			ActiveOpViewport = i;
			break;
		}
	}

	// 독점 조작하는 뷰포트가 있다면 상태값 유지 + 포커스 인덱스 갱신
	if (ActiveOpViewport >= 0)
	{
		for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
			GetViewportState(i).bHovered = (i == ActiveOpViewport);

		SetLastFocusedViewportIndex(ActiveOpViewport);
	}
	else
	{
		// Hover 된 뷰포트 찾아서 상태값 변경하기
		bool bFoundHover = false;
		for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
		{
			FEditorViewportState& ViewportState = GetViewportState(i);
			if (!bFoundHover && ViewportState.Rect.Contains(MouseX, MouseY))
			{
				ViewportState.bHovered = true;
				bFoundHover = true;

				// 좌클릭 시 해당 뷰포트를 마지막 포커스로 등록
				if (InputSystem::Get().GetKeyDown(VK_LBUTTON))
				{
					SetLastFocusedViewportIndex(i);
				}
			}
			else
			{
				ViewportState.bHovered = false;
			}
		}
	}
}

void FViewportLayout::Tick(float DeltaTime)
{
	UpdateHoverStates();
	// bHovered 가 설정된 뷰포트만 입력을 처리합니다.
	for (int32 i = 0; i < FViewportLayout::MaxViewports; ++i)
	{
		GetViewportClient(i).Tick(DeltaTime);
	}
}

void FViewportLayout::OnWindowResized(uint32 Width, uint32 Height)
{
	// 스플리터 트리 재배치 + SViewport → ISlateViewport 동기화
	if (GetRootSplitterV())
	{
		const FViewportRect LayoutRect = (HostRect.Width > 0 && HostRect.Height > 0)
			? HostRect
			: FViewportRect(0, 0, static_cast<int32>(Width), static_cast<int32>(Height));
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

void FViewportLayout::SetHostRect(const FViewportRect& InHostRect)
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
void FViewportLayout::InitViewportRect(uint32 Width, uint32 Height)
{
	const int32 W = static_cast<int32>(Width);
	const int32 H = static_cast<int32>(Height);

	// 50:50 초기 분할 (이후 BuildViewportLayout → SyncViewportRects 에서 최종 반영)
	const int32 HalfW = W / 2;
	const int32 HalfH = H / 2;

	ViewportStates[0].Rect = { 0,     0,     HalfW,      HalfH };  // 좌상단
	ViewportStates[1].Rect = { HalfW, 0,     W - HalfW,  HalfH };  // 우상단
	ViewportStates[2].Rect = { 0,     HalfH, HalfW,      H - HalfH };  // 좌하단
	ViewportStates[3].Rect = { HalfW, HalfH, W - HalfW,  H - HalfH };  // 우하단

	for (int32 i = 0; i < MaxViewports; ++i)
	{
		SceneViewports[i].SetRect(ViewportStates[i].Rect);

		ViewportClients[i].SetViewportSize(
			static_cast<float>(ViewportStates[i].Rect.Width),
			static_cast<float>(ViewportStates[i].Rect.Height));
	}
}

//  Viewport Layout 생성 (2 x 2)
void FViewportLayout::BuildViewportLayout(int32 Width, int32 Height)
{
	DestroyViewportLayout();  // 기존 위젯이 있으면 먼저 해제

	// 4개 SViewport 생성 + ISlateViewport(FSceneViewport) 연결
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		ViewportWidgets[i] = new SViewport();
		ViewportWidgets[i]->SetViewportInterface(&SceneViewports[i]);
	}

	// 스플리터 트리 구성
	//   SSplitterV (루트, 위/아래)
	//     SideLT (위)   = TopSplitterH → [0] 좌상단(Perspective), [1] 우상단(Top)
	//     SideRB (아래) = BotSplitterH → [2] 좌하단(Front),       [3] 우하단(Right)
	TopSplitterH = new SSplitterH();
	TopSplitterH->SetSideLT(ViewportWidgets[0]);
	TopSplitterH->SetSideRB(ViewportWidgets[1]);

	BotSplitterH = new SSplitterH();
	BotSplitterH->SetSideLT(ViewportWidgets[2]);
	BotSplitterH->SetSideRB(ViewportWidgets[3]);

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
		RootWindow->SetChild(RootSplitterV);
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
}

void FViewportLayout::SetSingleViewportMode(bool bSingle, int32 Index)
{
	bSingleViewport     = bSingle;
	SingleViewportIndex = (Index < 0) ? 0 : (Index >= MaxViewports ? MaxViewports - 1 : Index);

	// Settings 즉시 반영 (Shutdown 의 SaveToFile 에서 파일에 기록됨)
	FEditorSettings::Get().ActiveViewportCount = bSingleViewport ? 1 : MaxViewports;
	FEditorSettings::Get().SingleViewportIndex = SingleViewportIndex;

	SyncViewportRects();
}

void FViewportLayout::SetLastFocusedViewportIndex(int32 Index)
{
	if (Index < 0) Index = 0;
	if (Index >= MaxViewports) Index = MaxViewports - 1;
	LastFocusedViewportIndex = Index;

	FEditorViewportClient& MainViewport = GetViewportClient(LastFocusedViewportIndex);
}

void FViewportLayout::SyncViewportRects()
{
	// 1개 모드: SingleViewportIndex 뷰포트에 전체 영역 할당, 나머지는 크기 0
	if (bSingleViewport && RootSplitterV)
	{
		const FRect& Full = RootSplitterV->GetRect();
		for (int32 i = 0; i < MaxViewports; ++i)
		{
			if (i == SingleViewportIndex)
			{
				const FViewportRect VR(
					static_cast<int32>(Full.X),
					static_cast<int32>(Full.Y),
					static_cast<int32>(Full.Width),
					static_cast<int32>(Full.Height));
				ViewportStates[i].Rect = VR;
				SceneViewports[i].SetRect(VR);
				ViewportClients[i].SetViewportSize(Full.Width, Full.Height);
			}
			else
			{
				const FViewportRect ZeroVR(0, 0, 0, 0);
				ViewportStates[i].Rect = ZeroVR;
				SceneViewports[i].SetRect(ZeroVR);
			}
		}
		return;
	}

	// 4분할 모드: 스플리터 트리에서 각 SViewport 의 FRect 를 읽어 반영
	for (int32 i = 0; i < MaxViewports; ++i)
	{
		if (!ViewportWidgets[i]) continue;

		const FRect& R = ViewportWidgets[i]->GetRect();
		const FViewportRect VR(
			static_cast<int32>(R.X),
			static_cast<int32>(R.Y),
			static_cast<int32>(R.Width),
			static_cast<int32>(R.Height)
		);

		// 스플리터 드래그로 바뀐 크기를 ViewportState, SceneViewport,
		// ViewportClient 카메라 종횡비에 모두 반영합니다.
		ViewportStates[i].Rect = VR;
		SceneViewports[i].SetRect(VR);
		ViewportClients[i].SetViewportSize(R.Width, R.Height);
	}
}

void FViewportLayout::DestroyViewportLayout()
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
		delete ViewportWidgets[i];
		ViewportWidgets[i] = nullptr;
	}
	delete TopSplitterH; TopSplitterH = nullptr;
	delete BotSplitterH; BotSplitterH = nullptr;
	delete RootSplitterV; RootSplitterV = nullptr;
}
