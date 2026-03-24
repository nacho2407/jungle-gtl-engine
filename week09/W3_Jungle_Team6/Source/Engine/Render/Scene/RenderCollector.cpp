#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"

void FRenderCollector::CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	if (!World) return;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor) continue;
		CollectFromActor(Actor, ShowFlags, ViewMode, RenderBus);
	}
}

void FRenderCollector::CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	for (AActor* Actor : SelectedActors)
	{
		CollectFromSelectedActor(Actor, ShowFlags, ViewMode, RenderBus);
	}
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus)
{
	FRenderCommand Cmd = {};
	Cmd.Type = ERenderCommandType::Grid;
	Cmd.Constants.Grid.GridSpacing = GridSpacing;
	Cmd.Constants.Grid.GridHalfLineCount = GridHalfLineCount;
	RenderBus.AddCommand(ERenderPass::Grid, Cmd);
}

void FRenderCollector::CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus)
{
	if (ShowFlags.bGizmo == false) return;
	if (!Gizmo || !Gizmo->IsVisible()) return;

	FMeshBuffer* GizmoMesh = &MeshBufferManager.GetMeshBuffer(Gizmo->GetPrimitiveType());
	FMatrix WorldMatrix = Gizmo->GetWorldMatrix();
	bool bHolding = Gizmo->IsHolding();
	int32 SelectedAxis = Gizmo->GetSelectedAxis();

	auto CreateGizmoCmd = [&](bool bInner) {
		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Gizmo;
		Cmd.MeshBuffer = GizmoMesh;
		Cmd.PerObjectConstants = FPerObjectConstants{ WorldMatrix };

		if (bInner)
		{
			Cmd.DepthStencilState = EDepthStencilState::GizmoInside;
			Cmd.BlendState = EBlendState::AlphaBlend;
		}
		else
		{
			Cmd.DepthStencilState = EDepthStencilState::GizmoOutside;
			Cmd.BlendState = EBlendState::Opaque;
		}
		Cmd.Constants.Gizmo.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Cmd.Constants.Gizmo.bIsInnerGizmo = bInner ? 1 : 0;
		Cmd.Constants.Gizmo.bClicking = bHolding ? 1 : 0;
		Cmd.Constants.Gizmo.SelectedAxis = SelectedAxis >= 0 ? (uint32)SelectedAxis : 0xffffffffu;
		Cmd.Constants.Gizmo.HoveredAxisOpacity = 0.3f;
		return Cmd;
		};

	RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(false));

	if (!bHolding)
	{
		RenderBus.AddCommand(ERenderPass::DepthLess, CreateGizmoCmd(true));
	}
}

void FRenderCollector::CollectFromActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	if (!Actor->IsVisible()) return;

	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		CollectFromComponent(Primitive, ShowFlags, ViewMode, RenderBus);
	}
}

void FRenderCollector::CollectFromSelectedActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	if (!Actor->IsVisible()) return;

	for (UPrimitiveComponent* primitiveComponent : Actor->GetPrimitiveComponents())
	{
		// MeshBuffer가 없는 Batcher 처리 타입은 아웃라인 렌더에서 제외
		EPrimitiveType PrimType = primitiveComponent->GetPrimitiveType();
		if (primitiveComponent->IsA<USubUVComponent>()) continue;
		if (!primitiveComponent->IsVisible()) continue;

		if (PrimType == EPrimitiveType::EPT_Text)
		{
			if (ShowFlags.bBillboardText == false) continue;
			UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(primitiveComponent);
			const FFontResource* Font = TextComp->GetFont();
			if (!Font || !Font->IsLoaded()) continue;
			const FString& Text = TextComp->GetText();
			if (Text.empty()) continue;

			FRenderCommand TextCmd{};
			TextCmd.PerObjectConstants = FPerObjectConstants{ primitiveComponent->GetWorldMatrix() };
			TextCmd.Type = ERenderCommandType::Font;
			TextCmd.PerObjectConstants.Color = TextComp->GetColor();
			TextCmd.TextData = Text;
			TextCmd.AtlasResource = Font;
			TextCmd.SpriteSize.X = TextComp->GetFontSize();
			TextCmd.BlendState = EBlendState::AlphaBlend;
			TextCmd.DepthStencilState = EDepthStencilState::Default;
			RenderBus.AddCommand(ERenderPass::Font, std::move(TextCmd));
			continue;
		}

		FRenderCommand BaseCmd{};
		BaseCmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());
		BaseCmd.PerObjectConstants = FPerObjectConstants{ primitiveComponent->GetWorldMatrix() };

		// StencilBuffer Mask
		FRenderCommand MaskCmd = BaseCmd;
		MaskCmd.Type = ERenderCommandType::SelectionOutline;
		MaskCmd.DepthStencilState = EDepthStencilState::StencilWrite; //스텐실 버퍼만 작성하는 타입
		MaskCmd.BlendState = EBlendState::NoColor;
		RenderBus.AddCommand(ERenderPass::StencilMask, MaskCmd);

		// Outline
		FRenderCommand OutlineCmd = BaseCmd;
		OutlineCmd.Type = ERenderCommandType::SelectionOutline;
		OutlineCmd.DepthStencilState = EDepthStencilState::StencilOutline;
		OutlineCmd.Constants.Outline.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // RGBA
		OutlineCmd.Constants.Outline.OutlineInvScale = FVector(1.0f / primitiveComponent->GetRelativeScale().X,
			1.0f / primitiveComponent->GetRelativeScale().Y, 1.0f / primitiveComponent->GetRelativeScale().Z);

		OutlineCmd.Constants.Outline.OutlineOffset = 0.03f;
		if (ViewMode == EViewMode::Wireframe)
		{
			OutlineCmd.PerObjectConstants.Color = FColor(255, 153, 0, 255).ToVector4();
		}
		CollectAABBCommand(primitiveComponent, RenderBus);
		OutlineCmd.Constants.Outline.PrimitiveType = (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Plane) ? 0u : 1u;
		RenderBus.AddCommand(ERenderPass::Outline, OutlineCmd);
	}
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	if (!Primitive->IsVisible()) return;

	FRenderCommand Cmd = {};
	Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
	ERenderPass TargetPass = ERenderPass::Opaque;

	switch (Primitive->GetPrimitiveType())
	{
	case EPrimitiveType::EPT_Cube:
	case EPrimitiveType::EPT_Sphere:
	case EPrimitiveType::EPT_Plane:
	{
		if (!ShowFlags.bPrimitives) return;
		Cmd.Type = ERenderCommandType::Primitive;
		Cmd.MeshBuffer = &MeshBufferManager.GetMeshBuffer(Primitive->GetPrimitiveType());
		Cmd.DepthStencilState = EDepthStencilState::Default;
		TargetPass = ERenderPass::Opaque;
		break;
	}

	case EPrimitiveType::EPT_SubUV:
	{
		USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(Primitive);
		const FParticleResource* Particle = SubUVComp->GetParticle();
		if (!Particle || !Particle->IsLoaded()) return;

		Cmd.Type = ERenderCommandType::SubUV;
		Cmd.AtlasResource = Particle;
		Cmd.FrameIndex = SubUVComp->GetFrameIndex();
		Cmd.SpriteSize = { SubUVComp->GetWidth(), SubUVComp->GetHeight() };
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

void FRenderCollector::CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, FRenderBus& RenderBus)
{
	FRenderCommand AABBCmd = {};
	AABBCmd.Type = ERenderCommandType::DebugBox;

	FBoundingBox Box = PrimitiveComponent->GetWorldBoundingBox();

	// 이전에 정의한 union 구조체의 AABB 영역에 데이터를 채웁니다.
	AABBCmd.Constants.AABB.Min = Box.Min;
	AABBCmd.Constants.AABB.Max = Box.Max;
	AABBCmd.Constants.AABB.Color = FColor(255, 153, 0, 255); // 선택 강조용 주황색

	// 렌더러가 마지막에 몰아서 그릴 수 있게 특정 패스(예: Editor/Overlay)에 푸시합니다.
	RenderBus.AddCommand(ERenderPass::Editor, AABBCmd);
}
