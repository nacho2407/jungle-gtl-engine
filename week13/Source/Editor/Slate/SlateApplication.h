#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"

class FViewportClient;

struct FViewportInfo
{
	FViewportClient* Client = nullptr;
	// 이미지 렌더 시 위젯이 보고하는 ImGui 인지 hover (z-order/팝업/캡처 반영).
	bool bImGuiHovered = false;
};

/**
* UE의 FSlateApplication 대응
* 실제 Slate UI 시스템과는 별개로, 뷰포트의 입력 소유권과 포커스 관리를 담당하는 클래스입니다.
*/
class FSlateApplication : public TSingleton<FSlateApplication>
{
	friend TSingleton<FSlateApplication>;
public:
	void RegisterViewport(FViewportClient* Client);
	void UnregisterViewport(FViewportClient* Client);

	void UpdateInputOwner();
	void BringViewportToFront(FViewportClient* Client);

	// 뷰포트 이미지를 그린 위젯이 매 프레임 자신의 ImGui hover 여부를 보고한다.
	void SetViewportImGuiHovered(FViewportClient* Client, bool bHovered);

	// ImGui 의존을 이 클래스 밖으로 밀어낸다 — UI 계층(EditorMainPanel)이
	// 매 프레임 ImGui IO 의 텍스트 입력 상태를 주입한다.
	void SetTextInputActive(bool bActive) { bTextInputActive = bActive; }

	FViewportClient* GetHoveredViewportClient() const { return HoveredClient; }
	FViewportClient* GetFocusedViewportClient() const { return FocusedClient; }
	FViewportClient* GetCapturedViewportClient() const { return CapturedClient; }

	bool DoesClientOwnMouseInput(FViewportClient* Client) const;
	bool DoesClientOwnKeyboardInput(FViewportClient* Client) const;

	void CaptureMouse(FViewportClient* Client);
	void ReleaseMouse(FViewportClient* Client);

	bool IsViewportRegistered(FViewportClient* Client) const;

private:
	TArray<FViewportInfo> RegisteredViewports;

	FViewportClient* HoveredClient = nullptr;
	FViewportClient* FocusedClient = nullptr;
	FViewportClient* CapturedClient = nullptr;

	bool bTextInputActive = false;
};
