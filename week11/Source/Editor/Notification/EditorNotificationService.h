#pragma once

#include "Core/CoreMinimal.h"

class UEditorEngine;

enum class EEditorNotificationType : uint8
{
	Info,
	Warning,
	Error,
};

class FEditorNotificationService
{
public:
	void Initialize(UEditorEngine* InEditorEngine);

	void Info(const FString& Message) const;
	void Warning(const FString& Message) const;
	void Error(const FString& Message) const;
	void Notify(EEditorNotificationType Type, const FString& Message) const;

private:
	UEditorEngine* EditorEngine = nullptr;
};
