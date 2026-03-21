#pragma once

#include "Core/Common.h"

class UEditorEngine;

using namespace common::structs;

class FEditorWidget
{
public:
	virtual ~FEditorWidget() = default;

	virtual void Initialize(UEditorEngine* InEditorEngine);
	virtual void Render(float DeltaTime, FViewOutput& ViewOutput) = 0;

protected:
	UEditorEngine* EditorEngine = nullptr;
};
