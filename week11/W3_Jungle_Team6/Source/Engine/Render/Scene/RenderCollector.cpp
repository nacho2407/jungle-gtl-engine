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
	
	UCamera* Camera = Context.Camera;
	RenderBus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix());

	//	Draw from Editor (Gizmo, Axis, etc.)
	CollectFromEditor(Context,RenderBus);

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
	Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());
	Cmd.TransformConstants = FTransformConstants{ primitiveComponent->GetWorldMatrix(), Context.Camera->GetViewMatrix(), Context.Camera->GetProjectionMatrix() };

	if (primitiveComponent->GetRenderCommand(Context.Camera->GetViewMatrix(), Context.Camera->GetProjectionMatrix(), Cmd))
	{
		ERenderPass selectedRenderPass = ERenderPass::Component;
		switch (Cmd.Type)
		{

		case ERenderCommandType::Primitive:
			selectedRenderPass = ERenderPass::Component;
			if (Context.SelectedComponent == primitiveComponent)
			{
				CollectComponentOutline(primitiveComponent, Context, RenderBus);
				CollectAABBCommand(primitiveComponent, RenderBus);
			}

			break;

		case ERenderCommandType::Billboard:
			Cmd.BlendState = EBlendState::AlphaBlend;
			Cmd.DepthStencilState = EDepthStencilState::Default;
			selectedRenderPass = ERenderPass::Component;
			break;
		}

		RenderBus.AddCommand(selectedRenderPass,Cmd);
	}

}

void FRenderCollector::CollectFromEditor(const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{

	CollectGizmo(Context, RenderBus);
	CollectMouseOverlay(Context, RenderBus);
}


void FRenderCollector::CollectGizmo(const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	UGizmoComponent* Gizmo = Context.Gizmo;
	if (!Gizmo || !Gizmo->IsVisible()) return;

	auto CreateGizmoCmd = [&](bool bInner) {
		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Gizmo;
		Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
		Cmd.TransformConstants = FTransformConstants{ Gizmo->GetWorldMatrix(), 
													RenderBus.GetView(), 
													RenderBus.GetProj()};
		if (bInner)
		{
			Cmd.DepthStencilState = EDepthStencilState::None;
			Cmd.BlendState = EBlendState::AlphaBlend;
			Cmd.Constants.Gizmo.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		}
		else
		{
			Cmd.DepthStencilState = EDepthStencilState::Default;
			Cmd.BlendState = EBlendState::Opaque;
			Cmd.Constants.Gizmo.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		}
		Cmd.Constants.Gizmo.bIsInnerGizmo = bInner ? 1 : 0;
		Cmd.Constants.Gizmo.bClicking = Gizmo->IsHolding() ? 1 : 0;
		Cmd.Constants.Gizmo.SelectedAxis = Gizmo->GetSelectedAxis() >= 0 ? (uint32)Gizmo->GetSelectedAxis() : 0xffffffffu;
		Cmd.Constants.Gizmo.HoveredAxisOpacity = 0.3f;
		return Cmd;
		};

	// Inner Gizmo
	RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(false));

	if (!Gizmo->IsHolding())
	{
		RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(true));
	}
}

void FRenderCollector::CollectMouseOverlay(const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	//	Cursor Overlay (null checking +)
	if (Context.CursorOverlayState == nullptr || Context.CursorOverlayState->bVisible == false)
	{
		return;
	}

	FRenderCommand OverlayCmd = {};
	OverlayCmd.Type = ERenderCommandType::Overlay;
	OverlayCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(EPrimitiveType::EPT_MouseOverlay);

	OverlayCmd.Constants.Overlay.CenterScreen.X = Context.CursorOverlayState->ScreenX;
	OverlayCmd.Constants.Overlay.CenterScreen.Y = Context.CursorOverlayState->ScreenY;
	OverlayCmd.Constants.Overlay.ViewportSize.X = static_cast<float>(Context.ViewportWidth);
	OverlayCmd.Constants.Overlay.ViewportSize.Y = static_cast<float>(Context.ViewportHeight);
	OverlayCmd.Constants.Overlay.Radius = Context.CursorOverlayState->CurrentRadius;
	OverlayCmd.Constants.Overlay.Color = Context.CursorOverlayState->Color;

	RenderBus.AddCommand(ERenderPass::Overlay, OverlayCmd);

}

void FRenderCollector::CollectComponentOutline(UPrimitiveComponent* primitiveComponent, const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	FRenderCommand OutlineCmd{};
	OutlineCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());
	OutlineCmd.TransformConstants = FTransformConstants{ primitiveComponent->GetWorldMatrix(), Context.Camera->GetViewMatrix(), Context.Camera->GetProjectionMatrix() };
	OutlineCmd.Type = ERenderCommandType::SelectionOutline;

	OutlineCmd.Constants.Outline.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // RGBA
	OutlineCmd.Constants.Outline.OutlineInvScale = FVector(1.0f / primitiveComponent->GetRelativeScale().X,
		1.0f / primitiveComponent->GetRelativeScale().Y, 1.0f / primitiveComponent->GetRelativeScale().Z);
	OutlineCmd.Constants.Outline.OutlineOffset = 0.03f;

	if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Plane)
	{
		OutlineCmd.Constants.Outline.PrimitiveType = 0u;
	}
	else
	{
		//	Plane은 Outline이 제대로 안나오는 이슈가 있어서, 일단 Cube로 대체하여 그립니다.
		OutlineCmd.Constants.Outline.PrimitiveType = 1u;
	}

	RenderBus.AddCommand(ERenderPass::Outline, OutlineCmd);
}

void FRenderCollector::CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, FRenderBus& RenderBus)
{
	FRenderCommand AABBCmd = {};
	AABBCmd.Type = ERenderCommandType::DebugBox; 

	FBoundingBox Box = PrimitiveComponent->GetWorldBoundingBox();

	// 이전에 정의한 union 구조체의 AABB 영역에 데이터를 채웁니다.
	AABBCmd.Constants.AABB.Min = Box.Min;
	AABBCmd.Constants.AABB.Max = Box.Max;
	AABBCmd.Constants.AABB.Color = FColor(1.0f, 0.6f, 0.0f, 1.0f); // 선택 강조용 주황색

	// 렌더러가 마지막에 몰아서 그릴 수 있게 특정 패스(예: Editor/Overlay)에 푸시합니다.
	RenderBus.AddCommand(ERenderPass::Editor, AABBCmd);
}
