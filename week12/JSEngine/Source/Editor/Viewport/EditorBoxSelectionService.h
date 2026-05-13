#pragma once

#include "Core/CoreTypes.h"
#include "Spatial/WorldSpatialIndex.h"

#include <Windows.h>

class FSelectionManager;
class FViewportCamera;
class UWorld;

class FEditorBoxSelectionService
{
public:
	static void SelectActorsInBox(
		UWorld* World,
		FSelectionManager* SelectionManager,
		const FViewportCamera* Camera,
		const POINT& BoxSelectStart,
		const POINT& BoxSelectEnd,
		float ViewportWidth,
		float ViewportHeight,
		FWorldSpatialIndex::FPrimitiveFrustumQueryScratch& FrustumQueryScratch);

private:
	static bool TryProjectWorldToViewport(
		const FViewportCamera& Camera,
		const FVector& WorldPos,
		float ViewportWidth,
		float ViewportHeight,
		float& OutViewportX,
		float& OutViewportY,
		float& OutDepth);
};
