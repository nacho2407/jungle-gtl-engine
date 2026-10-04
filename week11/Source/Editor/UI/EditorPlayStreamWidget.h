#pragma once
#include "Editor/UI/EditorWidget.h"
#include "ImGui/imgui.h"

class FEditorPlayStreamWidget : public FEditorWidget
{
public:
	virtual void Render(float DeltaTime) override;

private:
	const char* PlayLabel   = "▶ Play";
    const char* ResumeLabel = "▶ Resume";
    const char* PauseLabel  = "❚❚ Pause"; 
    const char* StopLabel   = "■ Stop";
};

