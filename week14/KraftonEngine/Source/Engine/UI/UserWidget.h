#pragma once

#include "Object/Object.h"
#include "Core/Log.h"
#include <sol/sol.hpp>
#include <utility>

#ifdef GetNextSibling
#undef GetNextSibling
#endif
#ifdef GetFirstChild
#undef GetFirstChild
#endif
#include <RmlUi/Core.h>

class APlayerController;
class FWidgetClickEventListener;
namespace Rml { class ElementDocument; }

class FWidgetClickEventListener final : public Rml::EventListener
{
public:
	FWidgetClickEventListener(FString InElementId, sol::protected_function InCallback)
		: ElementId(std::move(InElementId))
		, Callback(std::move(InCallback))
	{
	}

	void ProcessEvent(Rml::Event& /*Event*/) override
	{
		if (!Callback.valid())
		{
			return;
		}

		sol::protected_function_result Result = Callback();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("[Lua] UI click callback error: %s", Err.what());
		}
	}

	const FString& GetElementId() const { return ElementId; }

private:
	FString ElementId;
	sol::protected_function Callback;
};

class UUserWidget : public UObject
{
public:
	DECLARE_CLASS(UUserWidget, UObject)

	UUserWidget() = default;
	~UUserWidget() override = default;

	void Initialize(APlayerController* InOwningPlayer, const FString& InDocumentPath);
	void AddToViewport(int32 InZOrder = 0);
	void RemoveFromParent();
	void BindClick(const FString& ElementId, sol::protected_function Callback);
	void RegisterEventListeners();
	void ClearEventListeners();
	void SetText(const FString& ElementId, const FString& Text);
	bool SetProperty(const FString& ElementId, const FString& PropertyName, const FString& Value);

	APlayerController* GetOwningPlayer() const { return OwningPlayer; }
	const FString& GetDocumentPath() const { return DocumentPath; }
	int32 GetZOrder() const { return ZOrder; }
	bool IsInViewport() const { return bInViewport; }
	bool IsDocumentLoaded() const { return bDocumentLoaded; }
	Rml::ElementDocument* GetDocument() const { return Document; }

	// 메뉴/대화창처럼 사용자가 클릭/포인팅을 해야 하는 widget 은 true 로 설정.
	// UUIManager 가 viewport 에 올라온 widget 중 하나라도 이 값이 true 면 GameViewportClient
	// 에 알려 시스템 커서를 보이고 raw mouse / clip 을 해제하도록 한다. HUD 처럼 비대화형
	// 오버레이는 false 유지.
	void SetWantsMouse(bool bInWantsMouse) { bWantsMouse = bInWantsMouse; }
	bool WantsMouse() const { return bWantsMouse; }

	void MarkDocumentLoaded(Rml::ElementDocument* InDocument) { Document = InDocument; bDocumentLoaded = Document != nullptr; }
	void MarkRemovedFromViewport() { bInViewport = false; }
	void ClearDocument() { Document = nullptr; bDocumentLoaded = false; }

private:
	APlayerController* OwningPlayer = nullptr;
	Rml::ElementDocument* Document = nullptr;
	FString DocumentPath;
	TArray<std::pair<FString, sol::protected_function>> PendingClickBindings;
	TArray<FWidgetClickEventListener*> ClickListeners;
	int32 ZOrder = 0;
	bool bInViewport = false;
	bool bDocumentLoaded = false;
	bool bWantsMouse = false;
};
