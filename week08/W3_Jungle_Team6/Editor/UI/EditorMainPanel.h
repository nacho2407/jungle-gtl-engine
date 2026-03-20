#pragma once

#include <windows.h>

#include "Math/Vector.h"
#include "Editor/Core/EditorConsole.h"
#include "Core/Common.h"
#include "Object/Object.h"

class FRenderer;
class FEditorEngine;

enum class EPrimitiveType;

using namespace common::structs;

class FEditorMainPanel
{
private:
	const char* PrimitiveTypes[3] =
	{
		"Cube",
		"Sphere",
		"Plane"
	};

	float NewSceneNotificationTimer = 0;
	float SceneSaveNotificationTimer = 0;
	float SceneLoadNotificationTimer = 0;
	FEditorEngine* EditorEngine = nullptr;

	int SelectedPrimitiveType = 0;
	int NumberOfSpawnedActors = 1;
	FVector CurSpawnPoint = { 0.f,0.f,0.f };
	bool bShowConsole = true;

	char SceneName[128] = "Default";
	char LoadPath[128] = "Saves/";

	FEditorConsole ConsoleInstance;
	
public:
	void Create(HWND InHWindow, FRenderer& InRenderer, FEditorEngine* InEditorEngine);
	void Release();
	void Render(float DeltaTime, FViewOutput& ViewOutput);
	void RenderObjectWindow(UObject*& Object);
	void Update();
};
