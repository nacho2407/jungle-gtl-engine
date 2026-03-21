#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "Component/Camera.h"
#include "Component/GizmoComponent.h"

FMeshBufferManager FRenderCollector::MeshBufferManager;

void FRenderCollector::Collect(const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	if (!Context.Camera || !Context.World)
	{
		return;
	}

	//	Must be the active camera
	
	FMatrix View = Context.Camera->GetViewMatrix();
	FMatrix Projection = Context.Camera->GetProjectionMatrix();

	//	Draw from Editor (Gizmo, Axis, etc.)
	CollectFromEditor(Context, View, Projection, RenderBus);

	//	Draw from World
	if (Context.ShowFlags.bPrimitives)
	{
		for (auto* Object : GUObjectArray)
		{
			if (!Object) continue;

			if (Object->IsA<AActor>())
			{
				auto* Actor = Object->Cast<AActor>();
				if (Actor->GetWorld() == Context.World)
				{
					CollectFromActor(Actor, Context, RenderBus);
				}
			}
		}
	}
}

void FRenderCollector::CollectFromActor(AActor* Actor, const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	// Iterate through the components of the actor and retrieve their render properties
	for (auto* Comp : Actor->GetComponents()) 
	{
		if (!Comp) continue;
		if (!Comp->IsA<UPrimitiveComponent>()) continue;

		UPrimitiveComponent* Primitive = dynamic_cast<UPrimitiveComponent*>(Comp);
		CollectFromComponent(Primitive, Context, RenderBus);

	}
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* primitiveComponent, const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	FRenderCommand Cmd = {};
	Cmd.Type = ERenderCommandType::Primitive;
	Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());
	Cmd.TransformConstants = FTransformConstants{ primitiveComponent->GetWorldMatrix(), Context.Camera->GetViewMatrix(), Context.Camera->GetProjectionMatrix()};

	if (primitiveComponent->GetRenderCommand(Context.Camera->GetViewMatrix(), Context.Camera->GetProjectionMatrix(), Cmd))
	{
		RenderBus.AddComponentCommand(Cmd);

		if(Context.SelectedComponent == primitiveComponent)
		{
			FRenderCommand OutlineCmd = Cmd;
			OutlineCmd.Type = ERenderCommandType::SelectionOutline;
			OutlineCmd.OutlineConstants.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // RGBA
			FVector scale = primitiveComponent->GetRelativeScale();
			const float kMinScale = 0.001f;
			if (std::abs(scale.X) < kMinScale) scale.X = kMinScale;
			if (std::abs(scale.Y) < kMinScale) scale.Y = kMinScale;
			if (std::abs(scale.Z) < kMinScale) scale.Z = kMinScale;
			OutlineCmd.OutlineConstants.OutlineInvScale = FVector(1.0f / scale.X, 1.0f / scale.Y, 1.0f / scale.Z);
			OutlineCmd.OutlineConstants.OutlineOffset = 0.03f;

			if(primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Plane)
			{
				OutlineCmd.OutlineConstants.PrimitiveType = 0u;
			}
			else
			{
				//	Plane은 Outline이 제대로 안나오는 이슈가 있어서, 일단 Cube로 대체하여 그립니다.
				OutlineCmd.OutlineConstants.PrimitiveType = 1u;
			}

			RenderBus.AddOutlineCommand(OutlineCmd);
		}
	}

}

void FRenderCollector::CollectFromEditor(const FRenderCollectorContext& Context, const FMatrix& ViewMat, const FMatrix& ProjMat, FRenderBus& RenderBus)
{
	//	Gizmo
	UGizmoComponent* Gizmo = Context.Gizmo;
	if (Context.ShowFlags.bGizmo && Gizmo && Gizmo->IsVisible())
	{
		FRenderCommand Cmd1 = {};
		Cmd1.Type = ERenderCommandType::Gizmo;
		Cmd1.MeshBuffer = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
		Cmd1.TransformConstants = FTransformConstants{ Gizmo->GetWorldMatrix(), ViewMat, ProjMat };
		Cmd1.GizmoConstants.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Cmd1.GizmoConstants.bIsInnerGizmo = 1;
		Cmd1.GizmoConstants.bClicking = Gizmo->IsHolding() ? 1 : 0;
		Cmd1.GizmoConstants.SelectedAxis = Gizmo->GetSelectedAxis() >= 0 ? static_cast<uint32>(Gizmo->GetSelectedAxis()) : 0xffffffffu;
		Cmd1.GizmoConstants.HoveredAxisOpacity = 0.55f;

		RenderBus.AddDepthLessCommand(Cmd1);

		//	선택되지 않은 경우에 Outer를 그림
		if (!Gizmo->IsHolding())
		{
			FRenderCommand Cmd2 = {};
			Cmd2.Type = ERenderCommandType::Gizmo;
			Cmd2.MeshBuffer = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
			Cmd2.TransformConstants = FTransformConstants{ Gizmo->GetWorldMatrix(), ViewMat, ProjMat };
			Cmd2.GizmoConstants.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
			Cmd2.GizmoConstants.bIsInnerGizmo = 0;
			Cmd2.GizmoConstants.bClicking = Gizmo->IsHolding() ? 1 : 0;
			Cmd2.GizmoConstants.SelectedAxis = Gizmo->GetSelectedAxis() >= 0 ? static_cast<uint32>(Gizmo->GetSelectedAxis()) : 0xffffffffu;
			Cmd2.GizmoConstants.HoveredAxisOpacity = 0.55f;

			RenderBus.AddDepthLessCommand(Cmd2);
		}
	}

	if (Context.ShowFlags.bGrid)
	{
		//	Axis 추가
		FRenderCommand AxisCmd = {};
		AxisCmd.Type = ERenderCommandType::Axis;
		AxisCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_Axis);
		AxisCmd.TransformConstants = FTransformConstants{ FMatrix::Identity, ViewMat, ProjMat };	//	Model은 고정

		FVector camPos = Context.Camera->GetWorldLocation();
		AxisCmd.EditorConstants.CameraPosition = FVector4{ camPos.X,camPos.Y,camPos.Z,0.0f };
		AxisCmd.EditorConstants.Flag = 0; // Axis : 0

		RenderBus.AddEditorCommand(AxisCmd);

		FRenderCommand GridCmd = {};
		GridCmd.Type = ERenderCommandType::Grid;
		GridCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_Grid);
		GridCmd.TransformConstants = FTransformConstants{ FMatrix::Identity, ViewMat, ProjMat };

		GridCmd.EditorConstants.CameraPosition = FVector4{ camPos.X,camPos.Y,camPos.Z,0.0f };
		GridCmd.EditorConstants.Flag = 1; // Grid : 1

		RenderBus.AddGridEditorCommand(GridCmd);
	}
    

	//	Cursor Overlay (null checking +)
	if (Context.CursorOverlayState && Context.CursorOverlayState->bVisible)
	{
		FRenderCommand OverlayCmd = {};
		OverlayCmd.Type = ERenderCommandType::Overlay;
		OverlayCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_MouseOverlay);

		OverlayCmd.OverlayConstants.CenterScreen.X = Context.CursorOverlayState->ScreenX;
		OverlayCmd.OverlayConstants.CenterScreen.Y = Context.CursorOverlayState->ScreenY;
		OverlayCmd.OverlayConstants.ViewportSize.X = static_cast<float>(Context.ViewportWidth);
		OverlayCmd.OverlayConstants.ViewportSize.Y = static_cast<float>(Context.ViewportHeight);
		OverlayCmd.OverlayConstants.Radius = Context.CursorOverlayState->CurrentRadius;
		OverlayCmd.OverlayConstants.Color = Context.CursorOverlayState->Color;

		RenderBus.AddOverlayCommand(OverlayCmd);
	}
}
