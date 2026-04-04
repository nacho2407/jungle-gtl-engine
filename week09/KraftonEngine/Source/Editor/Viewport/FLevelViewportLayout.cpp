#include "Editor/Viewport/FLevelViewportLayout.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Render/Pipeline/Renderer.h"
#include "Viewport/Viewport.h"
#include "UI/SSplitter.h"
#include "Math/MathUtils.h"
#include "Platform/Paths.h"
#include "ImGui/imgui.h"
#include "WICTextureLoader.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"

// ─── 레이아웃별 슬롯 수 ─────────────────────────────────────

int32 FLevelViewportLayout::GetSlotCount(EViewportLayout Layout)
{
	switch (Layout)
	{
	case EViewportLayout::OnePane:          return 1;
	case EViewportLayout::TwoPanesHoriz:
	case EViewportLayout::TwoPanesVert:     return 2;
	case EViewportLayout::ThreePanesLeft:
	case EViewportLayout::ThreePanesRight:
	case EViewportLayout::ThreePanesTop:
	case EViewportLayout::ThreePanesBottom: return 3;
	default:                                return 4;
	}
}

// ─── 아이콘 파일명 매핑 ──────────────────────────────────────

static const wchar_t* GetLayoutIconFileName(EViewportLayout Layout)
{
	switch (Layout)
	{
	case EViewportLayout::OnePane:          return L"ViewportLayout_OnePane.png";
	case EViewportLayout::TwoPanesHoriz:   return L"ViewportLayout_TwoPanesHoriz.png";
	case EViewportLayout::TwoPanesVert:    return L"ViewportLayout_TwoPanesVert.png";
	case EViewportLayout::ThreePanesLeft:  return L"ViewportLayout_ThreePanesLeft.png";
	case EViewportLayout::ThreePanesRight: return L"ViewportLayout_ThreePanesRight.png";
	case EViewportLayout::ThreePanesTop:   return L"ViewportLayout_ThreePanesTop.png";
	case EViewportLayout::ThreePanesBottom:return L"ViewportLayout_ThreePanesBottom.png";
	case EViewportLayout::FourPanes2x2:    return L"ViewportLayout_FourPanes2x2.png";
	case EViewportLayout::FourPanesLeft:   return L"ViewportLayout_FourPanesLeft.png";
	case EViewportLayout::FourPanesRight:  return L"ViewportLayout_FourPanesRight.png";
	case EViewportLayout::FourPanesTop:    return L"ViewportLayout_FourPanesTop.png";
	case EViewportLayout::FourPanesBottom: return L"ViewportLayout_FourPanesBottom.png";
	default:                               return L"";
	}
}

// ─── 아이콘 로드/해제 ────────────────────────────────────────

void FLevelViewportLayout::LoadLayoutIcons(ID3D11Device* Device)
{
	if (!Device) return;

	std::wstring IconDir = FPaths::Combine(FPaths::RootDir(), L"Asset/Editor/Icons/");

	for (int32 i = 0; i < static_cast<int32>(EViewportLayout::MAX); ++i)
	{
		std::wstring Path = IconDir + GetLayoutIconFileName(static_cast<EViewportLayout>(i));
		DirectX::CreateWICTextureFromFile(
			Device, Path.c_str(),
			nullptr, &LayoutIcons[i]);
	}
}

void FLevelViewportLayout::ReleaseLayoutIcons()
{
	for (int32 i = 0; i < static_cast<int32>(EViewportLayout::MAX); ++i)
	{
		if (LayoutIcons[i])
		{
			LayoutIcons[i]->Release();
			LayoutIcons[i] = nullptr;
		}
	}
}

// ─── Initialize / Release ────────────────────────────────────

void FLevelViewportLayout::Initialize(UEditorEngine* InEditor, FWindowsWindow* InWindow, FRenderer& InRenderer,
	FSelectionManager* InSelectionManager)
{
	Editor = InEditor;
	Window = InWindow;
	RendererPtr = &InRenderer;
	SelectionManager = InSelectionManager;

	// 아이콘 로드
	LoadLayoutIcons(InRenderer.GetFD3DDevice().GetDevice());

	// LevelViewportClient 생성 (단일 뷰포트)
	auto* LevelVC = new FLevelEditorViewportClient();
	LevelVC->SetOverlayStatSystem(&Editor->GetOverlayStatSystem());
	LevelVC->SetSettings(&FEditorSettings::Get());
	LevelVC->Initialize(Window);
	LevelVC->SetViewportSize(Window->GetWidth(), Window->GetHeight());
	LevelVC->SetWorld(Editor->GetWorld());
	LevelVC->SetGizmo(SelectionManager->GetGizmo());
	LevelVC->SetSelectionManager(SelectionManager);

	auto* VP = new FViewport();
	VP->Initialize(InRenderer.GetFD3DDevice().GetDevice(),
		static_cast<uint32>(Window->GetWidth()),
		static_cast<uint32>(Window->GetHeight()));
	VP->SetClient(LevelVC);
	LevelVC->SetViewport(VP);

	LevelVC->CreateCamera();
	LevelVC->ResetCamera();

	AllViewportClients.push_back(LevelVC);
	LevelViewportClients.push_back(LevelVC);
	SetActiveViewport(LevelVC);

	ViewportWindows[0] = new SWindow();
	LevelVC->SetLayoutWindow(ViewportWindows[0]);
	ActiveSlotCount = 1;
	CurrentLayout = EViewportLayout::OnePane;
}

void FLevelViewportLayout::Release()
{
	SSplitter::DestroyTree(RootSplitter);
	RootSplitter = nullptr;
	DraggingSplitter = nullptr;

	for (int32 i = 0; i < MaxViewportSlots; ++i)
	{
		delete ViewportWindows[i];
		ViewportWindows[i] = nullptr;
	}

	ActiveViewportClient = nullptr;
	for (FEditorViewportClient* VC : AllViewportClients)
	{
		if (FViewport* VP = VC->GetViewport())
		{
			VP->Release();
			delete VP;
		}
		delete VC;
	}
	AllViewportClients.clear();
	LevelViewportClients.clear();

	ReleaseLayoutIcons();
}

// ─── 활성 뷰포트 ────────────────────────────────────────────

void FLevelViewportLayout::SetActiveViewport(FLevelEditorViewportClient* InClient)
{
	if (ActiveViewportClient)
	{
		ActiveViewportClient->SetActive(false);
	}
	ActiveViewportClient = InClient;
	if (ActiveViewportClient)
	{
		ActiveViewportClient->SetActive(true);
		UWorld* World = Editor->GetWorld();
		if (World && ActiveViewportClient->GetCamera())
		{
			World->SetActiveCamera(ActiveViewportClient->GetCamera());
		}
	}
}

void FLevelViewportLayout::SetWorld(UWorld* InWorld)
{
	for (FEditorViewportClient* VC : AllViewportClients)
		VC->SetWorld(InWorld);
}

void FLevelViewportLayout::ResetViewport(UWorld* InWorld)
{
	for (FLevelEditorViewportClient* VC : LevelViewportClients)
	{
		VC->CreateCamera();
		VC->SetWorld(InWorld);
		VC->ResetCamera();

		// 카메라 재생성 후 현재 뷰포트 크기로 AspectRatio 동기화
		if (FViewport* VP = VC->GetViewport())
		{
			UCameraComponent* Cam = VC->GetCamera();
			if (Cam && VP->GetWidth() > 0 && VP->GetHeight() > 0)
			{
				Cam->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
			}
		}

		// 기존 뷰포트 타입(Ortho 방향 등)을 새 카메라에 재적용
		VC->SetViewportType(VC->GetRenderOptions().ViewportType);
	}
	if (ActiveViewportClient && InWorld)
		InWorld->SetActiveCamera(ActiveViewportClient->GetCamera());
}

void FLevelViewportLayout::DestroyAllCameras()
{
	for (FEditorViewportClient* VC : AllViewportClients)
	{
		VC->DestroyCamera();
		VC->SetWorld(nullptr);
	}
}

// ─── 뷰포트 슬롯 관리 ───────────────────────────────────────

void FLevelViewportLayout::EnsureViewportSlots(int32 RequiredCount)
{
	// 현재 슬롯보다 더 필요하면 추가 생성
	while (static_cast<int32>(LevelViewportClients.size()) < RequiredCount)
	{
		int32 Idx = static_cast<int32>(LevelViewportClients.size());

		auto* LevelVC = new FLevelEditorViewportClient();
		LevelVC->SetOverlayStatSystem(&Editor->GetOverlayStatSystem());
		LevelVC->SetSettings(&FEditorSettings::Get());
		LevelVC->Initialize(Window);
		LevelVC->SetViewportSize(Window->GetWidth(), Window->GetHeight());
		LevelVC->SetWorld(Editor->GetWorld());
		LevelVC->SetGizmo(SelectionManager->GetGizmo());
		LevelVC->SetSelectionManager(SelectionManager);

		auto* VP = new FViewport();
		VP->Initialize(RendererPtr->GetFD3DDevice().GetDevice(),
			static_cast<uint32>(Window->GetWidth()),
			static_cast<uint32>(Window->GetHeight()));
		VP->SetClient(LevelVC);
		LevelVC->SetViewport(VP);

		LevelVC->CreateCamera();
		LevelVC->ResetCamera();

		AllViewportClients.push_back(LevelVC);
		LevelViewportClients.push_back(LevelVC);

		ViewportWindows[Idx] = new SWindow();
		LevelVC->SetLayoutWindow(ViewportWindows[Idx]);
	}
}

void FLevelViewportLayout::ShrinkViewportSlots(int32 RequiredCount)
{
	while (static_cast<int32>(LevelViewportClients.size()) > RequiredCount)
	{
		FLevelEditorViewportClient* VC = LevelViewportClients.back();
		int32 Idx = static_cast<int32>(LevelViewportClients.size()) - 1;
		LevelViewportClients.pop_back();

		for (auto It = AllViewportClients.begin(); It != AllViewportClients.end(); ++It)
		{
			if (*It == VC) { AllViewportClients.erase(It); break; }
		}

		if (ActiveViewportClient == VC)
			SetActiveViewport(LevelViewportClients[0]);

		if (FViewport* VP = VC->GetViewport())
		{
			VP->Release();
			delete VP;
		}
		VC->DestroyCamera();
		delete VC;

		delete ViewportWindows[Idx];
		ViewportWindows[Idx] = nullptr;
	}
}

// ─── SSplitter 트리 빌드 ─────────────────────────────────────

SSplitter* FLevelViewportLayout::BuildSplitterTree(EViewportLayout Layout)
{
	SWindow** W = ViewportWindows;

	switch (Layout)
	{
	case EViewportLayout::OnePane:
		return nullptr; // 트리 불필요

	case EViewportLayout::TwoPanesHoriz:
	{
		// H → [0] | [1]
		auto* Root = new SSplitterH();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(W[1]);
		return Root;
	}
	case EViewportLayout::TwoPanesVert:
	{
		// V → [0] / [1]
		auto* Root = new SSplitterV();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(W[1]);
		return Root;
	}
	case EViewportLayout::ThreePanesLeft:
	{
		// H → [0] | V([1]/[2])
		auto* RightV = new SSplitterV();
		RightV->SetSideLT(W[1]);
		RightV->SetSideRB(W[2]);
		auto* Root = new SSplitterH();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(RightV);
		return Root;
	}
	case EViewportLayout::ThreePanesRight:
	{
		// H → V([0]/[1]) | [2]
		auto* LeftV = new SSplitterV();
		LeftV->SetSideLT(W[0]);
		LeftV->SetSideRB(W[1]);
		auto* Root = new SSplitterH();
		Root->SetSideLT(LeftV);
		Root->SetSideRB(W[2]);
		return Root;
	}
	case EViewportLayout::ThreePanesTop:
	{
		// V → [0] / H([1]|[2])
		auto* BottomH = new SSplitterH();
		BottomH->SetSideLT(W[1]);
		BottomH->SetSideRB(W[2]);
		auto* Root = new SSplitterV();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(BottomH);
		return Root;
	}
	case EViewportLayout::ThreePanesBottom:
	{
		// V → H([0]|[1]) / [2]
		auto* TopH = new SSplitterH();
		TopH->SetSideLT(W[0]);
		TopH->SetSideRB(W[1]);
		auto* Root = new SSplitterV();
		Root->SetSideLT(TopH);
		Root->SetSideRB(W[2]);
		return Root;
	}
	case EViewportLayout::FourPanes2x2:
	{
		// H → V([0]/[2]) | V([1]/[3])
		auto* LeftV = new SSplitterV();
		LeftV->SetSideLT(W[0]);
		LeftV->SetSideRB(W[2]);
		auto* RightV = new SSplitterV();
		RightV->SetSideLT(W[1]);
		RightV->SetSideRB(W[3]);
		auto* Root = new SSplitterH();
		Root->SetSideLT(LeftV);
		Root->SetSideRB(RightV);
		return Root;
	}
	case EViewportLayout::FourPanesLeft:
	{
		// H → [0] | V([1] / V([2]/[3]))
		auto* InnerV = new SSplitterV();
		InnerV->SetSideLT(W[2]);
		InnerV->SetSideRB(W[3]);
		auto* RightV = new SSplitterV();
		RightV->SetRatio(0.333f);
		RightV->SetSideLT(W[1]);
		RightV->SetSideRB(InnerV);
		auto* Root = new SSplitterH();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(RightV);
		return Root;
	}
	case EViewportLayout::FourPanesRight:
	{
		// H → V([0] / V([1]/[2])) | [3]
		auto* InnerV = new SSplitterV();
		InnerV->SetSideLT(W[1]);
		InnerV->SetSideRB(W[2]);
		auto* LeftV = new SSplitterV();
		LeftV->SetRatio(0.333f);
		LeftV->SetSideLT(W[0]);
		LeftV->SetSideRB(InnerV);
		auto* Root = new SSplitterH();
		Root->SetSideLT(LeftV);
		Root->SetSideRB(W[3]);
		return Root;
	}
	case EViewportLayout::FourPanesTop:
	{
		// V → [0] / H([1] | H([2]|[3]))
		auto* InnerH = new SSplitterH();
		InnerH->SetSideLT(W[2]);
		InnerH->SetSideRB(W[3]);
		auto* BottomH = new SSplitterH();
		BottomH->SetRatio(0.333f);
		BottomH->SetSideLT(W[1]);
		BottomH->SetSideRB(InnerH);
		auto* Root = new SSplitterV();
		Root->SetSideLT(W[0]);
		Root->SetSideRB(BottomH);
		return Root;
	}
	case EViewportLayout::FourPanesBottom:
	{
		// V → H([0] | H([1]|[2])) / [3]
		auto* InnerH = new SSplitterH();
		InnerH->SetSideLT(W[1]);
		InnerH->SetSideRB(W[2]);
		auto* TopH = new SSplitterH();
		TopH->SetRatio(0.333f);
		TopH->SetSideLT(W[0]);
		TopH->SetSideRB(InnerH);
		auto* Root = new SSplitterV();
		Root->SetSideLT(TopH);
		Root->SetSideRB(W[3]);
		return Root;
	}
	default:
		return nullptr;
	}
}

// ─── 레이아웃 전환 ──────────────────────────────────────────

void FLevelViewportLayout::SetLayout(EViewportLayout NewLayout)
{
	if (NewLayout == CurrentLayout) return;

	bool bWasOnePane = (CurrentLayout == EViewportLayout::OnePane);

	// 기존 트리 해제
	SSplitter::DestroyTree(RootSplitter);
	RootSplitter = nullptr;
	DraggingSplitter = nullptr;

	int32 RequiredSlots = GetSlotCount(NewLayout);
	int32 OldSlotCount = static_cast<int32>(LevelViewportClients.size());

	// 슬롯 수 조정
	if (RequiredSlots > OldSlotCount)
		EnsureViewportSlots(RequiredSlots);
	else if (RequiredSlots < OldSlotCount)
		ShrinkViewportSlots(RequiredSlots);

	// 분할 전환 시 새로 추가된 슬롯에 Top, Front, Right 순으로 기본 설정
	if (NewLayout != EViewportLayout::OnePane)
	{
		constexpr ELevelViewportType DefaultTypes[] = {
			ELevelViewportType::Top,
			ELevelViewportType::Front,
			ELevelViewportType::Right
		};
		// 기존 슬롯(또는 슬롯 0)은 유지, 새로 생긴 슬롯에만 적용
		int32 StartIdx = bWasOnePane ? 1 : OldSlotCount;
		for (int32 i = StartIdx; i < RequiredSlots && (i - 1) < 3; ++i)
		{
			LevelViewportClients[i]->SetViewportType(DefaultTypes[i - 1]);
		}
	}

	// 새 트리 빌드
	RootSplitter = BuildSplitterTree(NewLayout);
	ActiveSlotCount = RequiredSlots;
	CurrentLayout = NewLayout;
}

void FLevelViewportLayout::ToggleViewportSplit()
{
	if (CurrentLayout == EViewportLayout::OnePane)
		SetLayout(EViewportLayout::FourPanes2x2);
	else
		SetLayout(EViewportLayout::OnePane);
}

// ─── Viewport UI 렌더링 ─────────────────────────────────────

void FLevelViewportLayout::RenderViewportUI(float DeltaTime)
{
	(void)DeltaTime;
	bMouseOverViewport = false;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_None);

	ImVec2 ContentPos = ImGui::GetCursorScreenPos();
	ImVec2 ContentSize = ImGui::GetContentRegionAvail();

	if (ContentSize.x > 0 && ContentSize.y > 0)
	{
		FRect ContentRect = { ContentPos.x, ContentPos.y, ContentSize.x, ContentSize.y };

		// SSplitter 레이아웃 계산
		if (RootSplitter)
		{
			RootSplitter->ComputeLayout(ContentRect);
		}
		else if (ViewportWindows[0])
		{
			ViewportWindows[0]->SetRect(ContentRect);
		}

		// 각 ViewportClient에 Rect 반영 + 이미지 렌더
		for (int32 i = 0; i < ActiveSlotCount; ++i)
		{
			if (i < static_cast<int32>(LevelViewportClients.size()))
			{
				FLevelEditorViewportClient* VC = LevelViewportClients[i];
				VC->UpdateLayoutRect();
				VC->RenderViewportImage(VC == ActiveViewportClient);
			}
		}

		// 각 뷰포트 패인 상단에 툴바 오버레이 렌더
		for (int32 i = 0; i < ActiveSlotCount; ++i)
		{
			RenderPaneToolbar(i);
		}

		// 분할 바 렌더 (재귀 수집)
		if (RootSplitter)
		{
			TArray<SSplitter*> AllSplitters;
			SSplitter::CollectSplitters(RootSplitter, AllSplitters);

			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			ImU32 BarColor = IM_COL32(80, 80, 80, 255);

			for (SSplitter* S : AllSplitters)
			{
				const FRect& Bar = S->GetSplitBarRect();
				DrawList->AddRectFilled(
					ImVec2(Bar.X, Bar.Y),
					ImVec2(Bar.X + Bar.Width, Bar.Y + Bar.Height),
					BarColor);
			}
		}

		// 입력 처리
		if (ImGui::IsWindowHovered())
		{
			ImVec2 MousePos = ImGui::GetIO().MousePos;
			FPoint MP = { MousePos.x, MousePos.y };

			// 마우스가 어떤 슬롯 위에 있는지
			for (int32 i = 0; i < ActiveSlotCount; ++i)
			{
				if (ViewportWindows[i] && ViewportWindows[i]->IsHover(MP))
				{
					bMouseOverViewport = true;
					break;
				}
			}

			// 분할 바 드래그
			if (RootSplitter)
			{
				if (ImGui::IsMouseClicked(0))
				{
					DraggingSplitter = SSplitter::FindSplitterAtBar(RootSplitter, MP);
				}

				if (ImGui::IsMouseReleased(0))
				{
					DraggingSplitter = nullptr;
				}

				if (DraggingSplitter)
				{
					const FRect& DR = DraggingSplitter->GetRect();
					if (DraggingSplitter->GetOrientation() == ESplitOrientation::Horizontal)
					{
						float NewRatio = (MousePos.x - DR.X) / DR.Width;
						DraggingSplitter->SetRatio(Clamp(NewRatio, 0.15f, 0.85f));
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
					}
					else
					{
						float NewRatio = (MousePos.y - DR.Y) / DR.Height;
						DraggingSplitter->SetRatio(Clamp(NewRatio, 0.15f, 0.85f));
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
					}
				}
				else
				{
					// 호버 커서 변경
					SSplitter* Hovered = SSplitter::FindSplitterAtBar(RootSplitter, MP);
					if (Hovered)
					{
						if (Hovered->GetOrientation() == ESplitOrientation::Horizontal)
							ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
						else
							ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
					}
				}
			}

			// 활성 뷰포트 전환 (분할 바 드래그 중이 아닐 때)
			if (!DraggingSplitter && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
			{
				for (int32 i = 0; i < ActiveSlotCount; ++i)
				{
					if (i < static_cast<int32>(LevelViewportClients.size()) &&
						ViewportWindows[i] && ViewportWindows[i]->IsHover(MP))
					{
						if (LevelViewportClients[i] != ActiveViewportClient)
							SetActiveViewport(LevelViewportClients[i]);
						break;
					}
				}
			}
		}
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

// ─── 각 뷰포트 패인 툴바 오버레이 ──────────────────────────

void FLevelViewportLayout::RenderPaneToolbar(int32 SlotIndex)
{
	if (SlotIndex >= MaxViewportSlots || !ViewportWindows[SlotIndex]) return;

	const FRect& PaneRect = ViewportWindows[SlotIndex]->GetRect();
	if (PaneRect.Width <= 0 || PaneRect.Height <= 0) return;

	// 패인 상단에 오버레이 윈도우
	char OverlayID[64];
	snprintf(OverlayID, sizeof(OverlayID), "##PaneToolbar_%d", SlotIndex);

	ImGui::SetNextWindowPos(ImVec2(PaneRect.X, PaneRect.Y));
	ImGui::SetNextWindowBgAlpha(0.4f);
	ImGui::SetNextWindowSize(ImVec2(0, 0)); // auto-size

	ImGuiWindowFlags OverlayFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove;

	ImGui::Begin(OverlayID, nullptr, OverlayFlags);
	{
		ImGui::PushID(SlotIndex);

		// Layout 드롭다운
		char PopupID[64];
		snprintf(PopupID, sizeof(PopupID), "LayoutPopup_%d", SlotIndex);

		if (ImGui::Button("Layout"))
		{
			ImGui::OpenPopup(PopupID);
		}

		if (ImGui::BeginPopup(PopupID))
		{
			constexpr int32 LayoutCount = static_cast<int32>(EViewportLayout::MAX);
			constexpr int32 Columns = 4;
			constexpr float IconSize = 32.0f;

			for (int32 i = 0; i < LayoutCount; ++i)
			{
				ImGui::PushID(i);

				bool bSelected = (static_cast<EViewportLayout>(i) == CurrentLayout);
				if (bSelected)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
				}

				bool bClicked = false;
				if (LayoutIcons[i])
				{
					bClicked = ImGui::ImageButton("##icon", (ImTextureID)LayoutIcons[i], ImVec2(IconSize, IconSize));
				}
				else
				{
					char Label[4];
					snprintf(Label, sizeof(Label), "%d", i);
					bClicked = ImGui::Button(Label, ImVec2(IconSize + 8, IconSize + 8));
				}

				if (bSelected)
				{
					ImGui::PopStyleColor();
				}

				if (bClicked)
				{
					SetLayout(static_cast<EViewportLayout>(i));
					ImGui::CloseCurrentPopup();
				}

				if ((i + 1) % Columns != 0 && i + 1 < LayoutCount)
					ImGui::SameLine();

				ImGui::PopID();
			}
			ImGui::EndPopup();
		}

		// 토글 버튼 (같은 행)
		ImGui::SameLine();

		constexpr float ToggleIconSize = 16.0f;
		int32 ToggleIdx = (CurrentLayout == EViewportLayout::OnePane)
			? static_cast<int32>(EViewportLayout::FourPanes2x2)
			: static_cast<int32>(EViewportLayout::OnePane);

		if (LayoutIcons[ToggleIdx])
		{
			if (ImGui::ImageButton("##toggle", (ImTextureID)LayoutIcons[ToggleIdx], ImVec2(ToggleIconSize, ToggleIconSize)))
			{
				ToggleViewportSplit();
			}
		}
		else
		{
			const char* ToggleLabel = (CurrentLayout == EViewportLayout::OnePane) ? "Split" : "Merge";
			if (ImGui::Button(ToggleLabel))
			{
				ToggleViewportSplit();
			}
		}

		// ViewportType + Settings 팝업
		if (SlotIndex < static_cast<int32>(LevelViewportClients.size()))
		{
		FLevelEditorViewportClient* VC = LevelViewportClients[SlotIndex];
		FViewportRenderOptions& Opts = VC->GetRenderOptions();

		// ── Viewport Type 드롭다운 (Perspective / Ortho 방향) ──
		ImGui::SameLine();

		static const char* ViewportTypeNames[] = {
			"Perspective", "Top", "Bottom", "Left", "Right", "Front", "Back", "Free Orthographic"
		};
		constexpr int32 ViewportTypeCount = sizeof(ViewportTypeNames) / sizeof(ViewportTypeNames[0]);
		int32 CurrentTypeIdx = static_cast<int32>(Opts.ViewportType);
		const char* CurrentTypeName = ViewportTypeNames[CurrentTypeIdx];

		char VTPopupID[64];
		snprintf(VTPopupID, sizeof(VTPopupID), "ViewportTypePopup_%d", SlotIndex);

		if (ImGui::Button(CurrentTypeName))
		{
			ImGui::OpenPopup(VTPopupID);
		}

		if (ImGui::BeginPopup(VTPopupID))
		{
			for (int32 t = 0; t < ViewportTypeCount; ++t)
			{
				bool bSelected = (t == CurrentTypeIdx);
				if (ImGui::Selectable(ViewportTypeNames[t], bSelected))
				{
					VC->SetViewportType(static_cast<ELevelViewportType>(t));
				}
			}
			ImGui::EndPopup();
		}

		// ── Gizmo Mode 팝업 ──
		UGizmoComponent* Gizmo = Editor->GetGizmo();
		if (Gizmo)
		{
			ImGui::SameLine();

			static const char* GizmoModeNames[] = { "Translate", "Rotate", "Scale" };
			const char* CurrentModeName = GizmoModeNames[static_cast<int32>(Gizmo->GetMode())];

			char GizmoPopupID[64];
			snprintf(GizmoPopupID, sizeof(GizmoPopupID), "GizmoModePopup_%d", SlotIndex);

			if (ImGui::Button(CurrentModeName))
			{
				ImGui::OpenPopup(GizmoPopupID);
			}

			if (ImGui::BeginPopup(GizmoPopupID))
			{
				int32 CurrentGizmoMode = static_cast<int32>(Gizmo->GetMode());
				if (ImGui::RadioButton("Translate", &CurrentGizmoMode, static_cast<int32>(EGizmoMode::Translate)))
					Gizmo->SetTranslateMode();
				if (ImGui::RadioButton("Rotate", &CurrentGizmoMode, static_cast<int32>(EGizmoMode::Rotate)))
					Gizmo->SetRotateMode();
				if (ImGui::RadioButton("Scale", &CurrentGizmoMode, static_cast<int32>(EGizmoMode::Scale)))
					Gizmo->SetScaleMode();
				ImGui::EndPopup();
			}
		}

		// ── Settings 팝업 ──
		ImGui::SameLine();

		char SettingsPopupID[64];
		snprintf(SettingsPopupID, sizeof(SettingsPopupID), "SettingsPopup_%d", SlotIndex);

		if (ImGui::Button("Settings"))
		{
			ImGui::OpenPopup(SettingsPopupID);
		}

		if (ImGui::BeginPopup(SettingsPopupID))
		{
			// View Mode
			ImGui::Text("View Mode");
			int32 CurrentMode = static_cast<int32>(Opts.ViewMode);
			ImGui::RadioButton("Lit", &CurrentMode, static_cast<int32>(EViewMode::Lit));
			ImGui::SameLine();
			ImGui::RadioButton("Unlit", &CurrentMode, static_cast<int32>(EViewMode::Unlit));
			ImGui::SameLine();
			ImGui::RadioButton("Wireframe", &CurrentMode, static_cast<int32>(EViewMode::Wireframe));
			Opts.ViewMode = static_cast<EViewMode>(CurrentMode);

			ImGui::Separator();

			// Show Flags
			ImGui::Text("Show");
			ImGui::Checkbox("Primitives", &Opts.ShowFlags.bPrimitives);
			ImGui::Checkbox("BillboardText", &Opts.ShowFlags.bBillboardText);
			ImGui::Checkbox("Grid", &Opts.ShowFlags.bGrid);
			ImGui::Checkbox("Gizmo", &Opts.ShowFlags.bGizmo);
			ImGui::Checkbox("Bounding Volume", &Opts.ShowFlags.bBoundingVolume);
			ImGui::Checkbox("Debug Draw", &Opts.ShowFlags.bDebugDraw);
			ImGui::Checkbox("Octree", &Opts.ShowFlags.bOctree);
			ImGui::Checkbox("Occlusion BVH", &Opts.ShowFlags.bOcclusionBVH);

			ImGui::Separator();

			// Grid Settings
			ImGui::Text("Grid");
			ImGui::SliderFloat("Spacing", &Opts.GridSpacing, 0.1f, 10.0f, "%.1f");
			ImGui::SliderInt("Half Line Count", &Opts.GridHalfLineCount, 10, 500);

			ImGui::Separator();

			// Camera Sensitivity
			ImGui::Text("Camera");
			ImGui::SliderFloat("Move Sensitivity", &Opts.CameraMoveSensitivity, 0.1f, 5.0f, "%.1f");
			ImGui::SliderFloat("Rotate Sensitivity", &Opts.CameraRotateSensitivity, 0.1f, 5.0f, "%.1f");

			ImGui::EndPopup();
		}
		} // SlotIndex guard

		ImGui::PopID();
	}
	ImGui::End();
}

// ─── FEditorSettings ↔ 뷰포트 상태 동기화 ──────────────────

void FLevelViewportLayout::SaveToSettings()
{
	FEditorSettings& S = FEditorSettings::Get();

	S.LayoutType = static_cast<int32>(CurrentLayout);

	// 뷰포트별 렌더 옵션 저장
	for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
	{
		S.SlotOptions[i] = LevelViewportClients[i]->GetRenderOptions();
	}

	// Splitter 비율 저장
	if (RootSplitter)
	{
		TArray<SSplitter*> AllSplitters;
		SSplitter::CollectSplitters(RootSplitter, AllSplitters);
		S.SplitterCount = static_cast<int32>(AllSplitters.size());
		if (S.SplitterCount > 3) S.SplitterCount = 3;
		for (int32 i = 0; i < S.SplitterCount; ++i)
		{
			S.SplitterRatios[i] = AllSplitters[i]->GetRatio();
		}
	}
	else
	{
		S.SplitterCount = 0;
	}

	// Perspective 카메라 (slot 0) 저장
	if (!LevelViewportClients.empty())
	{
		UCameraComponent* Cam = LevelViewportClients[0]->GetCamera();
		if (Cam)
		{
			S.PerspCamLocation = Cam->GetWorldLocation();
			S.PerspCamRotation = Cam->GetRelativeRotation();
			const FCameraState& CS = Cam->GetCameraState();
			S.PerspCamFOV = CS.FOV * (180.0f / 3.14159265358979f); // rad → deg
			S.PerspCamNearClip = CS.NearZ;
			S.PerspCamFarClip = CS.FarZ;
		}
	}
}

void FLevelViewportLayout::LoadFromSettings()
{
	const FEditorSettings& S = FEditorSettings::Get();

	// 레이아웃 전환 (슬롯 생성 + 트리 빌드)
	EViewportLayout NewLayout = static_cast<EViewportLayout>(S.LayoutType);
	if (NewLayout >= EViewportLayout::MAX)
		NewLayout = EViewportLayout::OnePane;

	// OnePane이 아니면 레이아웃 적용 (Initialize에서 이미 OnePane으로 생성됨)
	if (NewLayout != EViewportLayout::OnePane)
	{
		// SetLayout 내부 bWasOnePane 분기를 피하기 위해 직접 전환
		SSplitter::DestroyTree(RootSplitter);
		RootSplitter = nullptr;
		DraggingSplitter = nullptr;

		int32 RequiredSlots = GetSlotCount(NewLayout);
		EnsureViewportSlots(RequiredSlots);

		RootSplitter = BuildSplitterTree(NewLayout);
		ActiveSlotCount = RequiredSlots;
		CurrentLayout = NewLayout;
	}

	// 뷰포트별 렌더 옵션 적용
	for (int32 i = 0; i < ActiveSlotCount && i < static_cast<int32>(LevelViewportClients.size()); ++i)
	{
		FLevelEditorViewportClient* VC = LevelViewportClients[i];
		VC->GetRenderOptions() = S.SlotOptions[i];

		// ViewportType에 따라 카메라 ortho/방향 설정
		VC->SetViewportType(S.SlotOptions[i].ViewportType);
	}

	// Splitter 비율 복원
	if (RootSplitter)
	{
		TArray<SSplitter*> AllSplitters;
		SSplitter::CollectSplitters(RootSplitter, AllSplitters);
		for (int32 i = 0; i < S.SplitterCount && i < static_cast<int32>(AllSplitters.size()); ++i)
		{
			AllSplitters[i]->SetRatio(S.SplitterRatios[i]);
		}
	}

	// Perspective 카메라 (slot 0) 복원
	if (!LevelViewportClients.empty())
	{
		UCameraComponent* Cam = LevelViewportClients[0]->GetCamera();
		if (Cam)
		{
			Cam->SetRelativeLocation(S.PerspCamLocation);
			Cam->SetRelativeRotation(S.PerspCamRotation);

			FCameraState CS = Cam->GetCameraState();
			CS.FOV = S.PerspCamFOV * (3.14159265358979f / 180.0f); // deg → rad
			CS.NearZ = S.PerspCamNearClip;
			CS.FarZ = S.PerspCamFarClip;
			Cam->SetCameraState(CS);
		}
	}
}
