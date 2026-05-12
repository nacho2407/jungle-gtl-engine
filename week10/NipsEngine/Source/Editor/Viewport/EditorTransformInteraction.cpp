#include "Editor/Viewport/EditorTransformInteraction.h"

#include "Camera/ViewportCamera.h"
#include "Component/GizmoComponent.h"

FEditorTransformInteraction::ETransformMode FEditorTransformInteraction::GetNextTransformMode(ETransformMode CurrentMode)
{
	switch (CurrentMode)
	{
	case ETransformMode::Select:
		return ETransformMode::Translate;
	case ETransformMode::Translate:
		return ETransformMode::Rotate;
	case ETransformMode::Rotate:
		return ETransformMode::Scale;
	case ETransformMode::Scale:
	default:
		return ETransformMode::Select;
	}
}

FEditorTransformInteraction::ETransformMode FEditorTransformInteraction::GetNextGizmoTransformMode(ETransformMode CurrentMode)
{
	switch (CurrentMode)
	{
	case ETransformMode::Translate:
		return ETransformMode::Rotate;
	case ETransformMode::Rotate:
		return ETransformMode::Scale;
	case ETransformMode::Select:
	case ETransformMode::Scale:
	default:
		return ETransformMode::Translate;
	}
}

void FEditorTransformInteraction::ApplyTransformModeToGizmo(ETransformMode TransformMode, UGizmoComponent* Gizmo)
{
	if (!Gizmo)
	{
		return;
	}

	if (TransformMode == ETransformMode::Select)
	{
		Gizmo->DragEnd();
		Gizmo->SetVisibility(false);
		return;
	}

	Gizmo->SetVisibility(Gizmo->HasTarget());

	switch (TransformMode)
	{
	case ETransformMode::Translate:
		Gizmo->SetTranslateMode();
		break;
	case ETransformMode::Rotate:
		Gizmo->SetRotateMode();
		break;
	case ETransformMode::Scale:
		Gizmo->SetScaleMode();
		break;
	case ETransformMode::Select:
	default:
		break;
	}
}

void FEditorTransformInteraction::SyncGizmoVisualState(
	ETransformMode TransformMode,
	UGizmoComponent* Gizmo,
	const FViewportCamera& Camera,
	bool bHasCamera,
	bool bIsPIEWorld,
	bool bIsViewportHovered,
	bool bRoutedInputProcessedThisFrame)
{
	if (!bHasCamera || !Gizmo)
	{
		return;
	}

	if (bIsPIEWorld)
	{
		return;
	}

	if (!bIsViewportHovered && !bRoutedInputProcessedThisFrame)
	{
		return;
	}

	ApplyTransformModeToGizmo(TransformMode, Gizmo);

	if (TransformMode == ETransformMode::Select || !Gizmo->IsVisible())
	{
		return;
	}

	if (Camera.IsOrthographic())
	{
		Gizmo->ApplyScreenSpaceScalingOrtho(Camera.GetOrthoHeight());
	}
	else
	{
		Gizmo->ApplyScreenSpaceScaling(Camera.GetLocation());
	}
}
