#include "SlateApplication.h"

#include "Input/InputSystem.h"

// 의도적으로 ImGui 를 의존하지 않는다. 입력 소유권 정책은 ImGui 비의존이며,
// ImGui 사실(hover, 텍스트 입력)은 UI 계층이 setter 로 주입한다.

void FSlateApplication::RegisterViewport(FViewportClient* Client)
{
	if (!Client) return;

	for (FViewportInfo& Info : RegisteredViewports)
	{
		if (Info.Client == Client)
		{
			return;
		}
	}

	RegisteredViewports.push_back({ Client });
}

void FSlateApplication::UnregisterViewport(FViewportClient* Client)
{
	if (!Client) return;

	RegisteredViewports.erase(
		std::remove_if(RegisteredViewports.begin(), RegisteredViewports.end(),
			[Client](const FViewportInfo& Info) { return Info.Client == Client; }),
		RegisteredViewports.end());

	if (FocusedClient == Client)
	{
		FocusedClient = nullptr;
	}
	if (CapturedClient == Client)
	{
		CapturedClient = nullptr;
	}
	if (HoveredClient == Client)
	{
		HoveredClient = nullptr;
	}
}

void FSlateApplication::SetViewportImGuiHovered(FViewportClient* Client, bool bHovered)
{
	if (!Client) return;

	for (FViewportInfo& Info : RegisteredViewports)
	{
		if (Info.Client == Client)
		{
			Info.bImGuiHovered = bHovered;
			return;
		}
	}
}

void FSlateApplication::UpdateInputOwner()
{
	InputSystem& Input = InputSystem::Get();
	const FInputSystemSnapshot Snapshot = Input.MakeSnapshot();

	if (!Snapshot.bWindowFocused)
	{
		HoveredClient = nullptr;
		FocusedClient = nullptr;
		CapturedClient = nullptr;
		return;
	}

	// 소유권은 기하 hit-test가 아니라, 각 뷰포트 위젯이 이미지 렌더 시 보고한
	// ImGui 인지 hover를 신뢰한다. ImGui hover는 창 z-order/팝업/캡처를 이미
	// 반영하므로, 떠 있는 패널이 뷰포트를 가리면 자동으로 false가 된다.
	HoveredClient = nullptr;
	for (auto It = RegisteredViewports.rbegin(); It != RegisteredViewports.rend(); ++It)
	{
		if (It->Client && It->bImGuiHovered)
		{
			HoveredClient = It->Client;
			break;
		}
	}

	const bool bMousePressed =
		Snapshot.bLeftMousePressed ||
		Snapshot.bRightMousePressed ||
		Snapshot.bMiddleMousePressed;

	if (bMousePressed)
	{
		if (HoveredClient)
		{
			FocusedClient = HoveredClient;
			CapturedClient = HoveredClient;
		}
		else
		{
			// 뷰포트 밖(ImGui 패널/다른 창)을 클릭 → 키보드 소유권 해제.
			// 클릭이 키보드 포커스를 옮긴다는 모델을 ImGui와 일치시킨다.
			FocusedClient = nullptr;
		}
	}

	const bool bAnyMouseDown =
		Snapshot.bLeftMouseDown ||
		Snapshot.bRightMouseDown ||
		Snapshot.bMiddleMouseDown;

	if (!bAnyMouseDown)
	{
		CapturedClient = nullptr;
	}

	// 텍스트 입력 중에는 어떤 뷰포트도 키보드를 소유하지 않는다.
	if (bTextInputActive)
	{
		FocusedClient = nullptr;
	}

	if (FocusedClient && !IsViewportRegistered(FocusedClient))
	{
		FocusedClient = nullptr;
	}

	if (CapturedClient && !IsViewportRegistered(CapturedClient))
	{
		CapturedClient = nullptr;
	}

	for (FViewportInfo& Info : RegisteredViewports)
	{
		Info.bImGuiHovered = false;
	}
}

void FSlateApplication::BringViewportToFront(FViewportClient* Client)
{
	if (!Client) return;

	auto It = std::find_if(RegisteredViewports.begin(), RegisteredViewports.end(),
		[Client](const FViewportInfo& Info) { return Info.Client == Client; });

	if (It == RegisteredViewports.end()) return;

	FViewportInfo Info = *It;
	RegisteredViewports.erase(It);
	RegisteredViewports.push_back(Info);
}

bool FSlateApplication::DoesClientOwnMouseInput(FViewportClient* Client) const
{
	return Client && (Client == CapturedClient || Client == HoveredClient);
}

bool FSlateApplication::DoesClientOwnKeyboardInput(FViewportClient* Client) const
{
	return Client && (Client == FocusedClient);
}

void FSlateApplication::CaptureMouse(FViewportClient* Client)
{
	CapturedClient = Client;
}

void FSlateApplication::ReleaseMouse(FViewportClient* Client)
{
	if (CapturedClient == Client)
	{
		CapturedClient = nullptr;
	}
}

bool FSlateApplication::IsViewportRegistered(FViewportClient* Client) const
{
	for (const FViewportInfo& Info : RegisteredViewports)
	{
		if (Info.Client == Client)
		{
			return true;
		}
	}
	return false;
}
