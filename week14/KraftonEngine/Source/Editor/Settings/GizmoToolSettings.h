#pragma once

#include "Core/CoreTypes.h"

enum class EEditorCoordSystem : uint8
{
	World = 0,
	Local = 1
};

struct FGizmoToolSettings
{
	EEditorCoordSystem CoordSystem = EEditorCoordSystem::World;

	bool bEnableTranslationSnap = false;
	float TranslationSnapSize = 0.1f;

	bool bEnableRotationSnap = false;
	float RotationSnapSize = 15.0f;
	
	bool bEnableScaleSnap = false;
	float ScaleSnapSize = 0.1f;
};