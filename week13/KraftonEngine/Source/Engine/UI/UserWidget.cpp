#include "UI/UserWidget.h"

#include "Object/ObjectFactory.h"
#include "UI/UIManager.h"



IMPLEMENT_CLASS(UUserWidget, UObject)


void UUserWidget::Initialize(APlayerController* InOwningPlayer, const FString& InDocumentPath)
{
	OwningPlayer = InOwningPlayer;
	DocumentPath = InDocumentPath;
}

void UUserWidget::AddToViewport(int32 InZOrder)
{
	ZOrder = InZOrder;
	bInViewport = true;
	UUIManager::Get().AddToViewport(this, InZOrder);
}

void UUserWidget::RemoveFromParent()
{
	UUIManager::Get().RemoveFromViewport(this);
	bInViewport = false;
}

void UUserWidget::BindClick(const FString& ElementId, sol::protected_function Callback)
{
	PendingClickBindings.push_back({ ElementId, Callback });
	if (IsDocumentLoaded())
	{
		RegisterEventListeners();
	}
}

void UUserWidget::RegisterEventListeners()
{
	if (!Document)
	{
		return;
	}

	ClearEventListeners();

	for (const auto& Binding : PendingClickBindings)
	{
		Rml::Element* Element = Document->GetElementById(Binding.first);
		if (!Element)
		{
			UE_LOG("[RmlUi] Click target not found: %s", Binding.first.c_str());
			continue;
		}

		auto* Listener = new FWidgetClickEventListener(Binding.first, Binding.second);
		Element->AddEventListener("click", Listener);
		ClickListeners.push_back(Listener);
	}
}

void UUserWidget::ClearEventListeners()
{
	if (Document)
	{
		for (FWidgetClickEventListener* Listener : ClickListeners)
		{
			if (!Listener)
			{
				continue;
			}

			Rml::Element* Element = Document->GetElementById(Listener->GetElementId());
			if (Element)
			{
				Element->RemoveEventListener("click", Listener);
			}
		}
	}

	for (FWidgetClickEventListener* Listener : ClickListeners)
	{
		delete Listener;
	}
	ClickListeners.clear();
}
