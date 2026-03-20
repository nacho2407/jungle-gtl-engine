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
	//	Iterate through GUObjects
	for (auto* Object : GUObjectArray) 
	{
		if (!Object) continue;

		if (Object->IsA<AActor>() && !Object->bPendingKill)
		{
			auto* Actor = Object->Cast<AActor>();
			if (Actor->GetWorld() == Context.World)
			{
				CollectFromActor(Actor, Context, RenderBus);
			}
		}
	}
}

void FRenderCollector::CollectFromActor(AActor* Actor, const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	// Iterate through the components of the actor and retrieve their render properties
	for (auto* Comp : Actor->GetComponents()) 
	{
		if (!Comp || Comp->bPendingKill) continue;
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
		RenderBus.AddCommand(ERenderPass::Component,Cmd);

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

			RenderBus.AddCommand(ERenderPass::Outline,OutlineCmd);
		}
	}

}

void FRenderCollector::CollectFromEditor(const FRenderCollectorContext& Context, const FMatrix& ViewMat, const FMatrix& ProjMat, FRenderBus& RenderBus)
{
	CollectGizmo(Context, ViewMat, ProjMat, RenderBus);
	CollectGridAndAxis(Context, ViewMat, ProjMat, RenderBus);
	CollectMouseOverlay(Context, ViewMat, ProjMat, RenderBus);
}


void FRenderCollector::CollectGizmo(const FRenderCollectorContext& Context, const FMatrix& ViewMat, const FMatrix& ProjMat, FRenderBus& RenderBus)
{
	UGizmoComponent* Gizmo = Context.Gizmo;
	if (!Gizmo || !Gizmo->IsVisible()) return;

	auto CreateGizmoCmd = [&](bool bInner) {
		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Gizmo;
		Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
		Cmd.TransformConstants = FTransformConstants{ Gizmo->GetWorldMatrix(), ViewMat, ProjMat };
		if (bInner)
		{
			Cmd.DepthStencilState = EDepthStencilState::None;
			Cmd.BlendState = EBlendState::AlphaBlend;
			Cmd.GizmoConstants.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		}
		else
		{
			Cmd.DepthStencilState = EDepthStencilState::Default;
			Cmd.BlendState = EBlendState::Opaque;
			Cmd.GizmoConstants.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		}
		Cmd.GizmoConstants.bIsInnerGizmo = bInner ? 1 : 0;
		Cmd.GizmoConstants.bClicking = Gizmo->IsHolding() ? 1 : 0;
		Cmd.GizmoConstants.SelectedAxis = Gizmo->GetSelectedAxis() >= 0 ? (uint32)Gizmo->GetSelectedAxis() : 0xffffffffu;
		Cmd.GizmoConstants.HoveredAxisOpacity = 0.3f;
		return Cmd;
		};

	// Inner Gizmo
	RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(false));

	if (!Gizmo->IsHolding())
	{
		RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(true));
	}
}

void FRenderCollector::CollectGridAndAxis(const FRenderCollectorContext& Context, const FMatrix& ViewMat, const FMatrix& ProjMat, FRenderBus& RenderBus)
{
	if (Context.bGridVisible == false)
	{
		return;
	}

	FVector CamPos = Context.Camera->GetWorldLocation();
	FTransformConstants StaticTransform = { FMatrix::Identity, ViewMat, ProjMat };

	// Axis
	FRenderCommand AxisCmd = {};
	AxisCmd.Type = ERenderCommandType::Axis;
	AxisCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_Axis);
	AxisCmd.TransformConstants = StaticTransform;
	AxisCmd.EditorConstants.CameraPosition = FVector4{ CamPos.X, CamPos.Y, CamPos.Z, 0.0f };
	AxisCmd.EditorConstants.Flag = 0;
	RenderBus.AddCommand(ERenderPass::Editor, AxisCmd);

	// Grid
	FRenderCommand GridCmd = AxisCmd; // ���� �ʵ� ����
	GridCmd.Type = ERenderCommandType::Grid;
	GridCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_Grid);
	GridCmd.EditorConstants.Flag = 1;
	RenderBus.AddCommand(ERenderPass::Grid,GridCmd);
}

void FRenderCollector::CollectMouseOverlay(const FRenderCollectorContext& Context, const FMatrix& ViewMat, const FMatrix& ProjMat, FRenderBus& RenderBus)
{
	//	Cursor Overlay (null checking +)
	if (Context.CursorOverlayState == nullptr || Context.CursorOverlayState->bVisible == false)
	{
		return;
	}

	FRenderCommand OverlayCmd = {};
	OverlayCmd.Type = ERenderCommandType::Overlay;
	OverlayCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_MouseOverlay);

	OverlayCmd.OverlayConstants.CenterScreen.X = Context.CursorOverlayState->ScreenX;
	OverlayCmd.OverlayConstants.CenterScreen.Y = Context.CursorOverlayState->ScreenY;
	OverlayCmd.OverlayConstants.ViewportSize.X = static_cast<float>(Context.ViewportWidth);
	OverlayCmd.OverlayConstants.ViewportSize.Y = static_cast<float>(Context.ViewportHeight);
	OverlayCmd.OverlayConstants.Radius = Context.CursorOverlayState->CurrentRadius;
	OverlayCmd.OverlayConstants.Color = Context.CursorOverlayState->Color;

	RenderBus.AddCommand(ERenderPass::Overlay, OverlayCmd);

}
