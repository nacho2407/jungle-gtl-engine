#include "UI/UserWidget.h"

#include "Object/Reflection/ObjectFactory.h"
#include "UI/UIManager.h"

static FString EscapeRmlText(const FString& Text)
{
	FString Escaped;
	Escaped.reserve(Text.size());
	for (const char Ch : Text)
	{
		switch (Ch)
		{
		case '&': Escaped += "&amp;"; break;
		case '<': Escaped += "&lt;"; break;
		case '>': Escaped += "&gt;"; break;
		case '"': Escaped += "&quot;"; break;
		case '\'': Escaped += "&#39;"; break;
		default: Escaped += Ch; break;
		}
	}
	return Escaped;
}

void FWidgetEventListener::ProcessEvent(Rml::Event& Event)
{
	if (!Callback.valid())
	{
		return;
	}

	sol::state_view Lua(Callback.lua_state());
	sol::table EventTable = Lua.create_table();

	EventTable["type"] = Event.GetType();
	EventTable["mouse_x"] = Event.GetParameter<float>("mouse_x", -1.0f);
	EventTable["mouse_y"] = Event.GetParameter<float>("mouse_y", -1.0f);

	Rml::Element* Element = Event.GetCurrentElement();
	if (!Element)
	{
		Element = Event.GetTargetElement();
	}

	if (Element)
	{
		EventTable["element_id"] = Element->GetId();
		EventTable["element_left"] = Element->GetAbsoluteLeft();
		EventTable["element_top"] = Element->GetAbsoluteTop();
		EventTable["element_width"] = Element->GetOffsetWidth();
		EventTable["element_height"] = Element->GetOffsetHeight();
	}

	if (Rml::Element* Target = Event.GetTargetElement())
	{
		EventTable["target_id"] = Target->GetId();
	}

	sol::protected_function_result Result = Callback(EventTable);
	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("[Lua] UI event callback error: %s", Err.what());
	}
}



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

void UUserWidget::BindEvent(const FString& ElementId, const FString& EventName, sol::protected_function Callback)
{
	PendingEventBindings.push_back({ ElementId, EventName, Callback });
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

	for (const FWidgetEventBinding& Binding : PendingEventBindings)
	{
		Rml::Element* Element = Document->GetElementById(Binding.ElementId);
		if (!Element)
		{
			UE_LOG("[RmlUi] Event target not found: %s", Binding.ElementId.c_str());
			continue;
		}

		auto* Listener = new FWidgetEventListener(Binding.ElementId, Binding.EventName, Binding.Callback);
		Element->AddEventListener(Binding.EventName.c_str(), Listener);
		EventListeners.push_back(Listener);
	}
}

void UUserWidget::ClearEventListeners()
{
	if (Document)
	{
		for (FWidgetEventListener* Listener : EventListeners)
		{
			if (!Listener)
			{
				continue;
			}

			Rml::Element* Element = Document->GetElementById(Listener->GetElementId());
			if (Element)
			{
				Element->RemoveEventListener(Listener->GetEventName().c_str(), Listener);
			}
		}

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

	for (FWidgetEventListener* Listener : EventListeners)
	{
		delete Listener;
	}
	EventListeners.clear();

	for (FWidgetClickEventListener* Listener : ClickListeners)
	{
		delete Listener;
	}
	ClickListeners.clear();
}

void UUserWidget::SetText(const FString& ElementId, const FString& Text)
{
	if (!Document)
	{
		return;
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Text target not found: %s", ElementId.c_str());
		return;
	}

	const FString EscapedText = EscapeRmlText(Text);
	Element->SetInnerRML(EscapedText.c_str());
}

bool UUserWidget::SetProperty(const FString& ElementId, const FString& PropertyName, const FString& Value)
{
	if (!Document)
	{
		return false;
	}

	Rml::Element* Element = Document->GetElementById(ElementId);
	if (!Element)
	{
		UE_LOG("[RmlUi] Property target not found: %s", ElementId.c_str());
		return false;
	}

	return Element->SetProperty(PropertyName.c_str(), Value.c_str());
}
