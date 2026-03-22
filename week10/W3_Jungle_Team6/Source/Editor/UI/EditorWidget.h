#pragma once

#include "Core/Common.h"

class FEditorEngine;

using namespace common::structs;

class FEditorWidget
{
public:
	virtual ~FEditorWidget() = default;

	virtual void Initialize(FEditorEngine* InEditorEngine);
	virtual void Render(float DeltaTime, FViewOutput& ViewOutput) = 0;

protected:
	FEditorEngine* EditorEngine = nullptr;
};
