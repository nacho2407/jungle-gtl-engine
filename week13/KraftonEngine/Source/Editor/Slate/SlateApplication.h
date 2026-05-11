#pragma once

#include "Core/Singleton.h"
#include "Core/CoreTypes.h"

class SWindow;
class FViewportClient;

struct FViewportInfo
{
	SWindow* Window = nullptr;
	FViewportClient* Client;
};

/**
* UE의 FSlateApplication 대응
* 실제 Slate UI 시스템과는 별개로, 뷰포트의 입력 소유권과 포커스 관리를 담당하는 클래스입니다.
*/
class FSlateApplication : public TSingleton<FSlateApplication>
{
	friend TSingleton<FSlateApplication>;
public:
	void RegisterViewport(SWindow* Window, FViewportClient* Client);
	void UnregisterViewport(FViewportClient* Client);

	void UpdateInputOwner();

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
};
