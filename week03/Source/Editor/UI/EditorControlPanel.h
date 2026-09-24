#pragma once
#include "Editor/Editor.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "PrimitiveFactory.h"
#include "EngineCore/TimerManager.h"
#include "EngineCore/EngineTypes.h"
#include "World.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include <string>
#include "Renderer/Renderer.h"
#include <filesystem>
#include "Component/GridComp.h"
#include "EngineCore/ResourceManager.h"

class UCameraComponent;
class Editor;
class UGridComp;
class URenderer;
struct EditorControlPanel
{
	EditorControlPanel(Editor* parent, URenderer* Renderer);
	~EditorControlPanel();

	void Draw(UCameraComponent* camera, UGridComp* Grid);

	Editor* editor;
	URenderer* Renderer;
};
