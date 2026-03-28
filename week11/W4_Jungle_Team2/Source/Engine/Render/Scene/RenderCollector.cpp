#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"

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

		if (!primitiveComponent->IsVisible()) continue;

		FMeshBuffer* MeshBuffer = nullptr;
		if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_StaticMesh)
		{
			auto* StaticMeshComp = static_cast<UStaticMeshComponent*>(primitiveComponent);
			MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMeshComp->GetStaticMesh());
		}
		else
		{
			MeshBuffer = &MeshBufferManager.GetMeshBuffer(primitiveComponent->GetPrimitiveType());
		}

		if (!MeshBuffer)
		{
			continue;
		}

		FRenderCommand BaseCmd{};
		BaseCmd.MeshBuffer = MeshBuffer;
		BaseCmd.PerObjectConstants = FPerObjectConstants{ primitiveComponent->GetWorldMatrix() };
		FVector WorldScale = primitiveComponent->GetWorldScale();

		if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Text)
		{
			UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(primitiveComponent);
			const FFontResource* Font = TextComp->GetFont();
			if (!Font || !Font->IsLoaded()) continue;
			const FString& Text = TextComp->GetText();
			if (Text.empty()) continue;

			FMatrix outlineMatrix = TextComp->CalculateOutlineMatrix();
			WorldScale = outlineMatrix.GetScale();

			FRenderCommand TextCmd = BaseCmd;
			BaseCmd.PerObjectConstants.Model = outlineMatrix;

			if (ShowFlags.bBillboardText)
			{
				TextCmd.PerObjectConstants = FPerObjectConstants{ primitiveComponent->GetWorldMatrix() };
				TextCmd.Type = ERenderCommandType::Font;
				TextCmd.PerObjectConstants.Color = TextComp->GetColor();
				TextCmd.Constants.Font.Text = &Text;
				TextCmd.Constants.Font.Font = Font;
				TextCmd.Constants.Font.Scale = TextComp->GetFontSize();
				TextCmd.BlendState = EBlendState::AlphaBlend;
				TextCmd.DepthStencilState = EDepthStencilState::Default;
				RenderBus.AddCommand(ERenderPass::Font, TextCmd);
			}
		}

		if (!primitiveComponent->SupportsOutline()) continue;


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
		OutlineCmd.Constants.Outline.OutlineInvScale = FVector(1.0f / WorldScale.X,
			1.0f / WorldScale.Y, 1.0f / WorldScale.Z);
		OutlineCmd.Constants.Outline.OutlineOffset = 0.03f;
		if (ViewMode == EViewMode::Wireframe)
		{
			OutlineCmd.PerObjectConstants.Color = FColor(255, 153, 0, 255).ToVector4();
		}
		CollectAABBCommand(primitiveComponent, ShowFlags, RenderBus);
		EPrimitiveType PrimType = primitiveComponent->GetPrimitiveType();
		OutlineCmd.Constants.Outline.PrimitiveType = (PrimType == EPrimitiveType::EPT_Billboard ||
			PrimType == EPrimitiveType::EPT_SubUV ||
			PrimType == EPrimitiveType::EPT_Text) ? 0u : 1u;
		RenderBus.AddCommand(ERenderPass::Outline, OutlineCmd);
	}
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	if (!Primitive->IsVisible()) return;

	EPrimitiveType PrimType = Primitive->GetPrimitiveType();

	switch (PrimType)
	{
	case EPrimitiveType::EPT_StaticMesh:
	{
		if (!ShowFlags.bPrimitives) return;

		UStaticMeshComponent* StaticMeshComp = static_cast<UStaticMeshComponent*>(Primitive);
		const UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh();
		FMeshBuffer* MeshBuffer = MeshBufferManager.GetStaticMeshBuffer(StaticMesh);

		if (!MeshBuffer) return;

		for (const auto& Section : StaticMesh->GetSections())
		{
			FRenderCommand Cmd = {};
			Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
			Cmd.Type = ERenderCommandType::StaticMesh;
			Cmd.MeshBuffer = MeshBuffer;
			Cmd.DepthStencilState = EDepthStencilState::Default;

			Cmd.SectionIndexStart = Section.StartIndex;
			Cmd.SectionIndexCount = Section.IndexCount;

			Cmd.Constants.StaticMesh.CameraWorldPos = RenderBus.GetCameraPosition();

			static const FMaterial DefaultMaterial{};

			const FMaterial* MtlData = StaticMeshComp->GetMaterial(Section.MaterialSlotIndex);
			if (!MtlData) MtlData = &DefaultMaterial;

			// 빛 방향은 일정하다고 가정합니다
			if (MtlData)
			{
				Cmd.Constants.StaticMesh.AmbientColor = MtlData->AmbientColor;
				Cmd.Constants.StaticMesh.DiffuseColor = MtlData->DiffuseColor;
				Cmd.Constants.StaticMesh.SpecularColor = MtlData->SpecularColor;
				Cmd.Constants.StaticMesh.Shininess = MtlData->Shininess;

				FMaterialResource* DiffuseTexture = FResourceManager::Get().FindTexture(MtlData->DiffuseTexPath);
				Cmd.Constants.StaticMesh.bHasDiffuseMap = bool(DiffuseTexture);
				if (DiffuseTexture)
				{
					Cmd.Constants.StaticMesh.DiffuseSRV = DiffuseTexture->SRV;
				}

				FMaterialResource* AmbientTexture = FResourceManager::Get().FindTexture(MtlData->AmbientTexPath);
				Cmd.Constants.StaticMesh.bHasAmbientMap = bool(AmbientTexture);
				if (AmbientTexture)
				{
					Cmd.Constants.StaticMesh.AmbientSRV = AmbientTexture->SRV;
				}

				FMaterialResource* SpecularTexture = FResourceManager::Get().FindTexture(MtlData->SpecularTexPath);
				Cmd.Constants.StaticMesh.bHasSpecularMap = bool(SpecularTexture);
				if (SpecularTexture)
				{
					Cmd.Constants.StaticMesh.SpecularSRV = SpecularTexture->SRV;
				}

				FMaterialResource* BumpTexture = FResourceManager::Get().FindTexture(MtlData->BumpTexPath);
				Cmd.Constants.StaticMesh.bHasBumpMap = bool(BumpTexture);
				if (BumpTexture)
				{
					Cmd.Constants.StaticMesh.BumpSRV = BumpTexture->SRV;
				}

			}
			// 미리 준비한 디폴트 텍스쳐 붙이기
			else
			{

			}
			RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
		}

		break;
	}

	case EPrimitiveType::EPT_Text:
	{
		if (!ShowFlags.bBillboardText) return;

		UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(Primitive);
		const FFontResource* Font = TextComp->GetFont();
		if (!Font || !Font->IsLoaded()) return;

		const FString& Text = TextComp->GetText();
		if (Text.empty()) return;

		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Font;
		Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), TextComp->GetColor() };
		Cmd.Constants.Font.Text = &Text;
		Cmd.Constants.Font.Font = Font;
		Cmd.Constants.Font.Scale = TextComp->GetFontSize();
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;
		RenderBus.AddCommand(ERenderPass::Font, Cmd);
		break;
	}

	case EPrimitiveType::EPT_SubUV:
	{
		USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(Primitive);
		const FParticleResource* Particle = SubUVComp->GetParticle();
		if (!Particle || !Particle->IsLoaded()) return;

		FRenderCommand Cmd = {};
		Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
		Cmd.Type = ERenderCommandType::SubUV;
		Cmd.Constants.SubUV.Particle = Particle;
		Cmd.Constants.SubUV.FrameIndex = SubUVComp->GetFrameIndex();
		Cmd.Constants.SubUV.Width = SubUVComp->GetWidth();
		Cmd.Constants.SubUV.Height = SubUVComp->GetHeight();
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;

		RenderBus.AddCommand(ERenderPass::SubUV, Cmd);
		break;
	}
	default:
		if (PrimType == EPrimitiveType::EPT_TransGizmo || PrimType == EPrimitiveType::EPT_RotGizmo || PrimType == EPrimitiveType::EPT_ScaleGizmo)
		{
			return;
		}
		return;
	}
}

void FRenderCollector::CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus)
{
	if (!ShowFlags.bBoundingVolume) return;

	FRenderCommand AABBCmd = {};
	AABBCmd.Type = ERenderCommandType::DebugBox;

	const FAABB& Box = PrimitiveComponent->GetWorldAABB();

	// 이전에 정의한 union 구조체의 AABB 영역에 데이터를 채웁니다.
	AABBCmd.Constants.AABB.Min = Box.Min;
	AABBCmd.Constants.AABB.Max = Box.Max;
	AABBCmd.Constants.AABB.Color = FColor::White();

	// 렌더러가 마지막에 몰아서 그릴 수 있게 특정 패스(예: Editor/Overlay)에 푸시합니다.
	RenderBus.AddCommand(ERenderPass::Editor, AABBCmd);
}
