#pragma once
#include "Editor/Editor.h"
#include "EngineCore/EngineStatics.h"
#include "ImGuiLayout.h"

struct EditorStatPanel
{
	EditorStatPanel();
	~EditorStatPanel();

	void Draw(ImGuiWindowLayout& layout);
};

