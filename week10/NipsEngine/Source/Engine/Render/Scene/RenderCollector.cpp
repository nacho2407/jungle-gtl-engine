#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Object/ActorIterator.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/ResourceManager.h"
#include "Render/Resource/Material.h"

namespace
{
	FMatrix MakeViewBillboardMatrix(const UPrimitiveComponent* Primitive, const FRenderBus& RenderBus)
	{
		const FMatrix WorldMatrix = Primitive->GetWorldMatrix();
		return UBillboardComponent::MakeBillboardWorldMatrix(
			WorldMatrix.GetOrigin(),
			WorldMatrix.GetScaleVector(),
			RenderBus.GetCameraForward(),
			RenderBus.GetCameraRight(),
			RenderBus.GetCameraUp());
	}

	FMatrix MakeViewSubUVSelectionMatrix(const USubUVComponent* SubUVComp, const FRenderBus& RenderBus)
	{
		const FVector WorldScale = SubUVComp->GetWorldScale();
		return UBillboardComponent::MakeBillboardWorldMatrix(
			SubUVComp->GetWorldLocation(),
			FVector(
				WorldScale.X > 0.01f ? WorldScale.X : 0.01f,
				SubUVComp->GetWidth() * WorldScale.Y,
				SubUVComp->GetHeight() * WorldScale.Z),
			RenderBus.GetCameraForward(),
			RenderBus.GetCameraRight(),
			RenderBus.GetCameraUp());
	}
	/*
	* BillBoardComponent를 상속받은 text, SubUV가 사용하는 AABB 계산함수(의존성 분리)
	*/
	FAABB BuildQuadAABB(const FMatrix& WorldMatrix)
	{
		static constexpr FVector LocalQuadCorners[4] =
		{
			FVector(0.0f, -0.5f,  0.5f),
			FVector(0.0f,  0.5f,  0.5f),
			FVector(0.0f,  0.5f, -0.5f),
			FVector(0.0f, -0.5f, -0.5f)
		};

		FAABB Box;
		Box.Reset();

		for (const FVector& Corner : LocalQuadCorners)
		{
			Box.Expand(WorldMatrix.TransformPosition(Corner));
		}

		return Box;
	}

	FAABB BuildRenderAABB(const UPrimitiveComponent* PrimitiveComponent, const FRenderBus& RenderBus)
	{
		switch (PrimitiveComponent->GetPrimitiveType())
		{
		case EPrimitiveType::EPT_Text:
		{
			const UTextRenderComponent* TextComp = static_cast<const UTextRenderComponent*>(PrimitiveComponent);
			return BuildQuadAABB(TextComp->GetTextMatrix());
		}
		case EPrimitiveType::EPT_SubUV:
		{
			const USubUVComponent* SubUVComp = static_cast<const USubUVComponent*>(PrimitiveComponent);
			return BuildQuadAABB(MakeViewSubUVSelectionMatrix(SubUVComp, RenderBus));
		}
		default:
			return PrimitiveComponent->GetWorldAABB();
		}
	}
}

void FRenderCollector::CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	if (!World) return;

	for (TActorIterator<AActor> Iter(World); Iter; ++Iter)
	{
		AActor* Actor = *Iter;
		if (!Actor) continue;
		CollectFromActor(Actor, ShowFlags, ViewMode, RenderBus);
	}
}

void FRenderCollector::CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	bool bHasSelectionMask = false;
	for (AActor* Actor : SelectedActors)
	{
		bHasSelectionMask |= CollectFromSelectedActor(Actor, ShowFlags, ViewMode, RenderBus);
	}

	if (bHasSelectionMask)
	{
		FRenderCommand PostProcessCmd = {};
		PostProcessCmd.Type = ERenderCommandType::PostProcessOutline;
		PostProcessCmd.Constants.Outline.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f);
		PostProcessCmd.Constants.Outline.OutlineThicknessPixels = 5.0f;
		RenderBus.AddCommand(ERenderPass::PostProcessOutline, PostProcessCmd);
	}
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus, bool bOrthographic)
{
	FRenderCommand Cmd = {};
	Cmd.Type = ERenderCommandType::Grid;
	Cmd.Constants.Grid.GridSpacing = GridSpacing;
	Cmd.Constants.Grid.GridHalfLineCount = GridHalfLineCount;
	Cmd.Constants.Grid.bOrthographic = bOrthographic;
	RenderBus.AddCommand(ERenderPass::Grid, Cmd);
}

void FRenderCollector::CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus, bool bIsActiveOperation)
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

		Cmd.SectionIndexStart = 0;
		Cmd.SectionIndexCount = GizmoMesh->GetIndexBuffer().GetIndexCount();

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
		Cmd.Constants.Gizmo.SelectedAxis = (SelectedAxis >= 0 && bIsActiveOperation) ? (uint32)SelectedAxis : 0xffffffffu;
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

bool FRenderCollector::CollectFromSelectedActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	(void)ViewMode;
	if (!Actor->IsVisible()) return false;

	bool bHasSelectionMask = false;

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
		BaseCmd.SectionIndexStart = 0;
		BaseCmd.SectionIndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();

		if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Text)
		{
			UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(primitiveComponent);
			const FFontResource* Font = TextComp->GetFont();
			if (!Font || !Font->IsLoaded()) continue;
			const FString& Text = TextComp->GetText();
			if (Text.empty()) continue;

			FMatrix WorldMatrix = TextComp->GetTextMatrix();

			FRenderCommand TextCmd = BaseCmd;
			BaseCmd.PerObjectConstants.Model = WorldMatrix;
			TextCmd.PerObjectConstants = FPerObjectConstants{TextComp->GetWorldMatrix()};
			TextCmd.Type = ERenderCommandType::Font;
			TextCmd.PerObjectConstants.Color = TextComp->GetColor();
			TextCmd.Constants.Font.Text = &Text;
			TextCmd.Constants.Font.Font = Font;
			TextCmd.Constants.Font.Scale = TextComp->GetFontSize();
			TextCmd.BlendState = EBlendState::AlphaBlend;
			TextCmd.DepthStencilState = EDepthStencilState::Default;
			RenderBus.AddCommand(ERenderPass::Font, TextCmd);
		}
		else if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_SubUV)
		{
			BaseCmd.PerObjectConstants.Model = MakeViewSubUVSelectionMatrix(
				static_cast<USubUVComponent*>(primitiveComponent),
				RenderBus);
		}

		if (!primitiveComponent->SupportsOutline()) continue;

		// Selection Mask
		FRenderCommand MaskCmd = BaseCmd;
		MaskCmd.Type = ERenderCommandType::SelectionMask;
		RenderBus.AddCommand(ERenderPass::SelectionMask, MaskCmd);
		bHasSelectionMask = true;
		CollectAABBCommand(primitiveComponent, ShowFlags, RenderBus);
	}

	return bHasSelectionMask;
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

		const TArray<FStaticMeshSection>& Sections = StaticMesh->GetSections();
		for (int32 SectionIdx = 0; SectionIdx < static_cast<int32>(Sections.size()); ++SectionIdx)
		{
			const FStaticMeshSection& Section = Sections[SectionIdx];

			FRenderCommand Cmd = {};
			Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
			Cmd.Type = ERenderCommandType::StaticMesh;
			Cmd.MeshBuffer = MeshBuffer;
			Cmd.DepthStencilState = EDepthStencilState::Default;
			Cmd.BlendState = EBlendState::Opaque;

			Cmd.SectionIndexStart = Section.StartIndex;
			Cmd.SectionIndexCount = Section.IndexCount;

			Cmd.Constants.StaticMesh.CameraWorldPos = RenderBus.GetCameraPosition();

			// 메테리얼 정보가 없을 시 디폴트 메테리얼을 사용합니다.
			static const FMaterial EngineDefaultMaterial{};

			const FMaterial* MtlData = StaticMeshComp->GetMaterial(SectionIdx);

			if (!MtlData) MtlData = &EngineDefaultMaterial;
	
			Cmd.Constants.StaticMesh.AmbientColor = MtlData->AmbientColor;
			Cmd.Constants.StaticMesh.DiffuseColor = MtlData->DiffuseColor;
			Cmd.Constants.StaticMesh.SpecularColor = MtlData->SpecularColor;
			Cmd.Constants.StaticMesh.Shininess = MtlData->Shininess;

			Cmd.Constants.StaticMesh.ScrollX = StaticMeshComp->GetScroll().first;
			Cmd.Constants.StaticMesh.ScrollY = StaticMeshComp->GetScroll().second;

			ID3D11ShaderResourceView* DefaultSRV = FResourceManager::Get().GetDefaultWhiteSRV();

			auto ResolveSRV = [&](const FString& Path) -> ID3D11ShaderResourceView*
			{
				FMaterialResource* Res = FResourceManager::Get().FindTexture(Path);
				return (Res && Res->SRV) ? Res->SRV.Get() : DefaultSRV;
			};

			// 와이어 프레임이 있는 경우 텍스쳐를 사용하지 않는 메테리얼에게 기본 텍스쳐를 강제 주입
			if (ViewMode == EViewMode::Wireframe)
			{
				Cmd.Constants.StaticMesh.bHasDiffuseMap =  1u;
				Cmd.Constants.StaticMesh.bHasSpecularMap = 1u;
				Cmd.Constants.StaticMesh.DiffuseSRV = DefaultSRV;
				Cmd.Constants.StaticMesh.AmbientSRV = DefaultSRV;
				Cmd.Constants.StaticMesh.SpecularSRV = DefaultSRV;
				Cmd.Constants.StaticMesh.BumpSRV = DefaultSRV;
			}
			else
			{
				Cmd.Constants.StaticMesh.bHasDiffuseMap = MtlData->bHasDiffuseTexture ? 1u : 0u;
				Cmd.Constants.StaticMesh.bHasSpecularMap = MtlData->bHasSpecularTexture ? 1u : 0u;
				Cmd.Constants.StaticMesh.DiffuseSRV = MtlData->bHasDiffuseTexture ? ResolveSRV(MtlData->DiffuseTexPath) : DefaultSRV;
				Cmd.Constants.StaticMesh.AmbientSRV = MtlData->bHasAmbientTexture ? ResolveSRV(MtlData->AmbientTexPath) : DefaultSRV;
				Cmd.Constants.StaticMesh.SpecularSRV = MtlData->bHasSpecularTexture ? ResolveSRV(MtlData->SpecularTexPath) : DefaultSRV;
				Cmd.Constants.StaticMesh.BumpSRV = MtlData->bHasBumpTexture ? ResolveSRV(MtlData->BumpTexPath) : DefaultSRV;
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
		Cmd.PerObjectConstants = FPerObjectConstants{TextComp->GetWorldMatrix(), TextComp->GetColor()};
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
		Cmd.PerObjectConstants = FPerObjectConstants{
			MakeViewBillboardMatrix(Primitive, RenderBus),
			FColor::White().ToVector4() };
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

	const FAABB Box = BuildRenderAABB(PrimitiveComponent, RenderBus);

	// 이전에 정의한 union 구조체의 AABB 영역에 데이터를 채웁니다.
	AABBCmd.Constants.AABB.Min = Box.Min;
	AABBCmd.Constants.AABB.Max = Box.Max;
	AABBCmd.Constants.AABB.Color = FColor::White();

	// 렌더러가 마지막에 몰아서 그릴 수 있게 특정 패스(예: Editor/Overlay)에 푸시합니다.
	RenderBus.AddCommand(ERenderPass::Editor, AABBCmd);
}
