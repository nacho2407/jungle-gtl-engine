#pragma once

#include "Core/CoreMinimal.h"

class UEditorEngine;

class FEditorWidget
{
public:
	virtual ~FEditorWidget() = default;

	virtual void Initialize(UEditorEngine* InEditorEngine);
	virtual void Render(float DeltaTime) = 0;

protected:
	UEditorEngine* EditorEngine = nullptr;
};
