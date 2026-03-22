#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"

void FRenderCollector::Collect(const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	if (!Context.Camera || !Context.World)
	{
		return;
	}

	//	Must be the active camera

	UCameraComponent* Camera = Context.Camera;
	RenderBus.SetViewProjection(Camera->GetViewMatrix(), Camera->GetProjectionMatrix(),
		Camera->GetRightVector(), Camera->GetUpVector());
	RenderBus.SetRenderSettings(Context.ViewMode, Context.ShowFlags);


	//	Draw from Editor (Gizmo, Axis, etc.)
	CollectFromEditor(Context, RenderBus);

	//	Draw from World
	for (AActor* Actor : Context.World->GetActors())
	{
		if (!Actor) continue;
		CollectFromActor(Actor, Context, RenderBus);
	}
}

void FRenderCollector::CollectFromActor(AActor* Actor, const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	// Iterate through the components of the actor and retrieve their render properties
	for (auto* Comp : Actor->GetComponents())
	{
		if (!Comp) continue;
		if (!Comp->IsA<UPrimitiveComponent>()) continue;
		UPrimitiveComponent* Primitive = static_cast<UPrimitiveComponent*>(Comp);
		CollectFromComponent(Primitive, Context, RenderBus);

	}
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* primitiveComponent, const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	FRenderCommand Cmd = {};
	Cmd.PerObjectConstants = FPerObjectConstants{ primitiveComponent->GetWorldMatrix(), FColor::White().ToVector4(), 0.f};
	if (primitiveComponent->GetRenderCommand(Cmd))
	{
		ERenderPass selectedRenderPass = ERenderPass::Opaque;
		switch (Cmd.Type)
		{
		case ERenderCommandType::Primitive:
			if (Context.ShowFlags.bPrimitives == false) return;
			selectedRenderPass = ERenderPass::Opaque;
			Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());
			if (Context.SelectedComponent == primitiveComponent)
			{

				if (Context.ViewMode == EViewMode::Wireframe)
				{
					Cmd.PerObjectConstants.IsSelected = 1.0f;
					Cmd.PerObjectConstants.Color = FColor(255,153,0,255).ToVector4();
				}
				else
				{
					CollectAABBCommand(primitiveComponent, RenderBus);
				}

				Cmd.DepthStencilState = EDepthStencilState::StencilWrite;
				CollectComponentOutline(primitiveComponent, Context, RenderBus);
			}
			else
			{
				// 선택되지 않은 객체는 기본값(Default) 사용
				Cmd.DepthStencilState = EDepthStencilState::Default;
			}

			break;

		case ERenderCommandType::Billboard:

			if (Context.ShowFlags.bBillboardText == false) return;
			Cmd.BlendState = EBlendState::AlphaBlend;
			Cmd.DepthStencilState = EDepthStencilState::Default;
			Cmd.TextData = "Hello Jungle";
			selectedRenderPass = ERenderPass::Translucent;
			break;
		}

		RenderBus.AddCommand(selectedRenderPass, Cmd);
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
	if (Context.ShowFlags.bGizmo == false) return;
	if (!Gizmo || !Gizmo->IsVisible()) return;

	auto CreateGizmoCmd = [&](bool bInner) {
		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Gizmo;
		Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
		Cmd.PerObjectConstants = FPerObjectConstants{ Gizmo->GetWorldMatrix() };

		if (bInner)
		{
			Cmd.DepthStencilState = EDepthStencilState::GizmoInside;
			Cmd.BlendState = EBlendState::AlphaBlend;
			Cmd.Constants.Gizmo.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		}
		else
		{
			Cmd.DepthStencilState = EDepthStencilState::GizmoOutside;
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
	OutlineCmd.PerObjectConstants = FPerObjectConstants{ primitiveComponent->GetWorldMatrix() };
	OutlineCmd.Type = ERenderCommandType::SelectionOutline;
	OutlineCmd.DepthStencilState = EDepthStencilState::StencilOutline;
	OutlineCmd.Constants.Outline.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // RGBA
	OutlineCmd.Constants.Outline.OutlineInvScale = FVector(1.0f / primitiveComponent->GetRelativeScale().X,
		1.0f / primitiveComponent->GetRelativeScale().Y, 1.0f / primitiveComponent->GetRelativeScale().Z);

	if (Context.ViewMode == EViewMode::Wireframe)
	{
		OutlineCmd.Constants.Outline.OutlineOffset = 0.003f;
	}
	else
	{
		OutlineCmd.Constants.Outline.OutlineOffset = 0.03f;
	}

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

