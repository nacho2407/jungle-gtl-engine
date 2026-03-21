#pragma once
#include "Editor/Editor.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "PrimitiveFactory.h"
#include "EngineCore/TimerManager.h"
#include "World.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "ImGui/imgui.h"
#include <string>

class UCameraComponent;
class Editor;

struct EditorControlPanel
{
	EditorControlPanel(Editor* parent);
	~EditorControlPanel();

	void Draw(UCameraComponent* camera);

	Editor* editor;
};
