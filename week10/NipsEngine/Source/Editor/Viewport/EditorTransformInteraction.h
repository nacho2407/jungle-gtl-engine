#pragma once

#include "Editor/Viewport/EditorViewportClient.h"

class FViewportCamera;
class UGizmoComponent;

class FEditorTransformInteraction
{
public:
	using ETransformMode = FEditorViewportClient::ETransformMode;

	static ETransformMode GetNextTransformMode(ETransformMode CurrentMode);
	static ETransformMode GetNextGizmoTransformMode(ETransformMode CurrentMode);
	static void ApplyTransformModeToGizmo(ETransformMode TransformMode, UGizmoComponent* Gizmo);
	static void SyncGizmoVisualState(
		ETransformMode TransformMode,
		UGizmoComponent* Gizmo,
		const FViewportCamera& Camera,
		bool bHasCamera,
		bool bIsPIEWorld,
		bool bIsViewportHovered,
		bool bRoutedInputProcessedThisFrame);
};
