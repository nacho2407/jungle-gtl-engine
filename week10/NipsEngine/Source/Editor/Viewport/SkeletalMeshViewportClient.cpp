#include "SkeletalMeshViewportClient.h"

#include "Component/GizmoComponent.h"
#include "Engine/Input/InputTypes.h"
#include "Viewport/FSceneViewport.h"

#include <utility>

void FSkeletalMeshViewportClient::SetBonePickHandler(FBonePickHandler InHandler)
{
	BonePickHandler = std::move(InHandler);
}

bool FSkeletalMeshViewportClient::ProcessInput(FViewportInputContext& Context)
{
	if (ShouldTryBonePick(Context) && !IsGizmoUnderCursor(Context))
	{
		const float LocalX = static_cast<float>(Context.MouseLocalPos.x);
		const float LocalY = static_cast<float>(Context.MouseLocalPos.y);
		if (BonePickHandler(LocalX, LocalY))
		{
			return true;
		}
	}
	return FEditorViewportClient::ProcessInput(Context);
}

bool FSkeletalMeshViewportClient::ShouldTryBonePick(const FViewportInputContext& Context) const
{
	if (!ShowFlags.bShowBones || !BonePickHandler)
	{
		return false;
	}

	if (Context.Frame.IsAltDown() || Context.Frame.IsCtrlDown() || Context.Frame.IsShiftDown())
	{
		return false;
	}

	if (Context.Frame.bLeftDragging || Context.WasPointerDragStarted(EPointerButton::Left))
	{
		return false;
	}

	return Context.WasPressed(VK_LBUTTON) ||
		(Context.WasReleased(VK_LBUTTON) && !Context.WasPointerDragEnded(EPointerButton::Left));
}

bool FSkeletalMeshViewportClient::IsGizmoUnderCursor(const FViewportInputContext& Context) const
{
	UGizmoComponent* Gizmo = const_cast<FSkeletalMeshViewportClient*>(this)->GetGizmo();
	const FViewportCamera* Camera = GetCamera();
	const FSceneViewport* SceneViewport = GetViewport();
	if (!Gizmo || !Gizmo->IsVisible() || !Camera || !SceneViewport)
	{
		return false;
	}

	const FViewportRect& Rect = SceneViewport->GetRect();
	if (Rect.Width <= 0 || Rect.Height <= 0)
	{
		return false;
	}

	const float LocalX = static_cast<float>(Context.MouseLocalPos.x);
	const float LocalY = static_cast<float>(Context.MouseLocalPos.y);
	const FRay MouseRay = Camera->DeprojectScreenToWorld(
		LocalX,
		LocalY,
		static_cast<float>(Rect.Width),
		static_cast<float>(Rect.Height));

	FHitResult HitResult{};
	return Gizmo->HitTestMesh(MouseRay, HitResult);
}
