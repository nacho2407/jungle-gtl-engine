#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Object/Object.h"

class FEditorPropertyWidget : public FEditorWidget
{
public:
	virtual void Render(float DeltaTime, FViewOutput& ViewOutput) override;
};
