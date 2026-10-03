#include "Editor/Notification/EditorNotificationService.h"

#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Core/Logging/Log.h"

void FEditorNotificationService::Initialize(UEditorEngine* InEditorEngine)
{
	EditorEngine = InEditorEngine;
}

void FEditorNotificationService::Info(const FString& Message) const
{
	Notify(EEditorNotificationType::Info, Message);
}

void FEditorNotificationService::Warning(const FString& Message) const
{
	Notify(EEditorNotificationType::Warning, Message);
}

void FEditorNotificationService::Error(const FString& Message) const
{
	Notify(EEditorNotificationType::Error, Message);
}

void FEditorNotificationService::Notify(EEditorNotificationType Type, const FString& Message) const
{
	if (Message.empty())
	{
		return;
	}

	switch (Type)
	{
	case EEditorNotificationType::Warning:
		UE_LOG_WARNING("[EditorNotification] %s", Message.c_str());
		break;
	case EEditorNotificationType::Error:
		UE_LOG_ERROR("[EditorNotification] %s", Message.c_str());
		break;
	case EEditorNotificationType::Info:
	default:
		UE_LOG("[EditorNotification] %s", Message.c_str());
		break;
	}

	if (EditorEngine)
	{
		EditorEngine->GetMainPanel().PushFooterLog(Message);
	}
}
