#pragma once

#include "Core/CoreTypes.h"

class AActor;
class FEditorViewportClient;
class FSceneViewport;
class FViewportCamera;
class UEditorEngine;
class UWorld;

class FEditorPickingService
{
public:
	static bool ResolveActorForSelection(
		UWorld* World,
		const FViewportCamera* Camera,
		FSceneViewport* Viewport,
		UEditorEngine* Editor,
		float LocalX,
		float LocalY,
		AActor*& OutActor);

private:
	static bool PickActorByIdAtViewportLocalPoint(
		FSceneViewport* Viewport,
		UEditorEngine* Editor,
		float LocalX,
		float LocalY,
		AActor*& OutActor);

	static bool PickActorByRayAtViewportLocalPoint(
		UWorld* World,
		const FViewportCamera* Camera,
		FSceneViewport* Viewport,
		float LocalX,
		float LocalY,
		AActor*& OutActor);
};
