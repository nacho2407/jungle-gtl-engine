#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/CameraComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"

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

	// Picking Actors
	for (AActor* Actor : Context.SelectedActors)
	{
		CollectFromSelectedActor(Actor, Context, RenderBus);

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

void FRenderCollector::CollectFromSelectedActor(AActor* Actor, const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	for (UPrimitiveComponent* primitiveComponent : Actor->GetPrimitiveComponents())
	{
		// MeshBuffer가 없는 Batcher 처리 타입은 아웃라인 렌더에서 제외
		EPrimitiveType PrimType = primitiveComponent->GetPrimitiveType();
		if (PrimType == EPrimitiveType::EPT_Text || PrimType == EPrimitiveType::EPT_SubUV) continue;

		FRenderCommand BaseCmd{};
		BaseCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());
		BaseCmd.PerObjectConstants = FPerObjectConstants{ primitiveComponent->GetWorldMatrix() };

		// StencilBuffer Mask
		FRenderCommand MaskCmd = BaseCmd;
		MaskCmd.Type = ERenderCommandType::SelectionOutline;
		MaskCmd.DepthStencilState = EDepthStencilState::StencilWrite; //스텐실 버퍼만 작성하는 타입
		MaskCmd.BlendState = EBlendState::NoColor;
		RenderBus.AddCommand(ERenderPass::Outline, MaskCmd);

		// Outline
		FRenderCommand OutlineCmd = BaseCmd;
		OutlineCmd.Type = ERenderCommandType::SelectionOutline;
		OutlineCmd.DepthStencilState = EDepthStencilState::StencilOutline;
		OutlineCmd.Constants.Outline.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // RGBA
		OutlineCmd.Constants.Outline.OutlineInvScale = FVector(1.0f / primitiveComponent->GetRelativeScale().X,
			1.0f / primitiveComponent->GetRelativeScale().Y, 1.0f / primitiveComponent->GetRelativeScale().Z);

		if (Context.ViewMode == EViewMode::Wireframe)
		{
			OutlineCmd.PerObjectConstants.Color = FColor(255, 153, 0, 255).ToVector4();
			OutlineCmd.Constants.Outline.OutlineOffset = 0.003f;
		}
		else
		{
			OutlineCmd.Constants.Outline.OutlineOffset = 0.03f;
		}
		CollectAABBCommand(primitiveComponent, RenderBus);
		OutlineCmd.Constants.Outline.PrimitiveType = (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Plane) ? 0u : 1u;
		RenderBus.AddCommand(ERenderPass::Outline, OutlineCmd);
	}
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* Primitive, const FRenderCollectorContext& Context, FRenderBus& RenderBus)
{
	if (!Primitive->IsVisible()) return;

	FRenderCommand Cmd = {};
	Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4(), 0.f };
	ERenderPass TargetPass = ERenderPass::Opaque;

	switch (Primitive->GetPrimitiveType())
	{
	case EPrimitiveType::EPT_Cube:
	case EPrimitiveType::EPT_Sphere:
	case EPrimitiveType::EPT_Plane:
	{
		if (!Context.ShowFlags.bPrimitives) return;
		Cmd.Type = ERenderCommandType::Primitive;
		Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(Primitive->GetPrimitiveType());
		Cmd.DepthStencilState = EDepthStencilState::Default;
		TargetPass = ERenderPass::Opaque;
		break;
	}

	case EPrimitiveType::EPT_Quad: // Billboard
	{
		if (!Context.ShowFlags.bBillboardText) return;
		Cmd.Type = ERenderCommandType::Billboard;
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;
		Cmd.TextData = "Hello Jungle";
		TargetPass = ERenderPass::Translucent;
		break;
	}

	case EPrimitiveType::EPT_Text:
	{
		if (!Context.ShowFlags.bBillboardText) return;

		// 선택된 액터의 컴포넌트만 UUID 텍스트 표시
		AActor* Owner = Primitive->GetOwner();
		bool bOwnerSelected = false;
		for (AActor* Selected : Context.SelectedActors)
		{
			if (Selected == Owner) { bOwnerSelected = true; break; }
		}
		if (!bOwnerSelected) return;

		UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(Primitive);
		const FFontResource* Font = TextComp->GetFont();
		if (!Font || !Font->IsLoaded()) return;
		const FString& Text = TextComp->GetText();
		if (Text.empty()) return;

		Cmd.Type = ERenderCommandType::Font;
		Cmd.PerObjectConstants.Color = TextComp->GetColor();
		Cmd.TextData     = Text;
		Cmd.AtlasResource = Font;
		Cmd.SpriteSize.X = TextComp->GetFontSize();
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;
		TargetPass = ERenderPass::Font;
		break;
	}

	case EPrimitiveType::EPT_SubUV:
	{
		USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(Primitive);
		const FParticleResource* Particle = SubUVComp->GetParticle();
		if (!Particle || !Particle->IsLoaded()) return;

		Cmd.Type = ERenderCommandType::SubUV;
		Cmd.AtlasResource = Particle;
		Cmd.FrameIndex    = SubUVComp->GetFrameIndex();
		Cmd.SpriteSize    = { SubUVComp->GetWidth(), SubUVComp->GetHeight() };
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;
		TargetPass = ERenderPass::SubUV;
		break;
	}

	default:
		return;
	}

	RenderBus.AddCommand(TargetPass, Cmd);
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

