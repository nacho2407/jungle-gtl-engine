#include "RenderCollector.h"
#include "Render/Resource/ConstantBufferPool.h"

#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Texture/Texture2D.h"
#include "Component/GizmoComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/BillboardComponent.h"

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
	Cmd.Params.Grid.GridSpacing = GridSpacing;
	Cmd.Params.Grid.GridHalfLineCount = GridHalfLineCount;
	RenderBus.AddCommand(ERenderPass::Grid, Cmd);
}

void FRenderCollector::CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus)
{
	if (ShowFlags.bGizmo == false) return;
	if (!Gizmo || !Gizmo->IsVisible()) return;

	FMeshBuffer* GizmoMesh = &FMeshBufferManager::Get().GetMeshBuffer(Gizmo->GetPrimitiveType());
	FMatrix WorldMatrix = Gizmo->GetWorldMatrix();
	bool bHolding = Gizmo->IsHolding();
	int32 SelectedAxis = Gizmo->GetSelectedAxis();

	auto CreateGizmoCmd = [&](bool bInner) {
		FRenderCommand Cmd = {};
		Cmd.Type = ERenderCommandType::Gizmo;
		Cmd.Shader = FShaderManager::Get().GetShader(EShaderType::Gizmo);
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
		Cmd.Params.Gizmo.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Cmd.Params.Gizmo.bIsInnerGizmo = bInner ? 1 : 0;
		Cmd.Params.Gizmo.bClicking = bHolding ? 1 : 0;
		Cmd.Params.Gizmo.SelectedAxis = SelectedAxis >= 0 ? (uint32)SelectedAxis : 0xffffffffu;
		Cmd.Params.Gizmo.HoveredAxisOpacity = 0.3f;

		// Gizmo CB
		Cmd.ExtraCB = { FConstantBufferPool::Get().GetBuffer(ECBSlot::Gizmo, sizeof(FGizmoConstants)), sizeof(FGizmoConstants), ECBSlot::Gizmo };

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
		FRenderCommand BaseCmd{};
		FMeshBuffer* Buffer = primitiveComponent->GetMeshBuffer();
		if (!Buffer) Buffer = &FMeshBufferManager::Get().GetMeshBuffer(primitiveComponent->GetPrimitiveType());
		BaseCmd.MeshBuffer = Buffer;
		BaseCmd.PerObjectConstants = FPerObjectConstants{ primitiveComponent->GetWorldMatrix() };
		FVector WorldScale = primitiveComponent->GetWorldScale();

		if (primitiveComponent->GetPrimitiveType() == EPrimitiveType::EPT_Text)
		{
			UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(primitiveComponent);
			const FFontResource* Font = TextComp->GetFont();
			if (!Font || !Font->IsLoaded()) continue;
			const FString& Text = TextComp->GetText();
			if (Text.empty()) continue;

			// 카메라 축을 직접 사용하여 빌보드 행렬 구성 (FontBatcher와 동일한 축 보장)
			// Right를 반전: CalculateOutlineMatrix의 CenterY(-0.5)와 결합해 올바른 방향이 됨
			FVector BillboardForward = (RenderBus.GetCameraForward() * -1.0f);
			FMatrix RotMatrix;
			RotMatrix.SetAxes(BillboardForward, RenderBus.GetCameraRight() * -1.0f, RenderBus.GetCameraUp());
			FMatrix PerViewBillboard = FMatrix::MakeScaleMatrix(TextComp->GetWorldScale())
				* RotMatrix * FMatrix::MakeTranslationMatrix(TextComp->GetWorldLocation());
			FMatrix outlineMatrix = TextComp->CalculateOutlineMatrix(PerViewBillboard);
			WorldScale = outlineMatrix.GetScale();

			FRenderCommand TextCmd = BaseCmd;
			BaseCmd.PerObjectConstants.Model = outlineMatrix;

			if (ShowFlags.bBillboardText)
			{
				TextCmd.PerObjectConstants = FPerObjectConstants{ PerViewBillboard };
				TextCmd.Type = ERenderCommandType::Font;
				TextCmd.PerObjectConstants.Color = TextComp->GetColor();
				TextCmd.Params.Font.Text = &Text;
				TextCmd.Params.Font.Font = Font;
				TextCmd.Params.Font.Scale = TextComp->GetFontSize();
				TextCmd.BlendState = EBlendState::AlphaBlend;
				TextCmd.DepthStencilState = EDepthStencilState::Default;
				RenderBus.AddCommand(ERenderPass::Font, TextCmd);
			}
		}

		if (!primitiveComponent->SupportsOutline()) continue;

		EPrimitiveType PrimType = primitiveComponent->GetPrimitiveType();
		bool bIsPNCT = (PrimType == EPrimitiveType::EPT_StaticMesh);

		// StencilBuffer Mask
		FRenderCommand MaskCmd = BaseCmd;
		MaskCmd.Type = ERenderCommandType::SelectionOutline;
		MaskCmd.Shader = bIsPNCT
			? FShaderManager::Get().GetShader(EShaderType::StaticMesh)
			: FShaderManager::Get().GetShader(EShaderType::Primitive);
		MaskCmd.DepthStencilState = EDepthStencilState::StencilWrite; //스텐실 버퍼만 작성하는 타입
		MaskCmd.BlendState = EBlendState::NoColor;
		RenderBus.AddCommand(ERenderPass::StencilMask, MaskCmd);

		// Outline
		FRenderCommand OutlineCmd = BaseCmd;
		OutlineCmd.Type = ERenderCommandType::SelectionOutline;
		OutlineCmd.Shader = bIsPNCT
			? FShaderManager::Get().GetShader(EShaderType::OutlinePNCT)
			: FShaderManager::Get().GetShader(EShaderType::Outline);
		OutlineCmd.DepthStencilState = EDepthStencilState::StencilOutline;
		OutlineCmd.Params.Outline.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 0.7f); // RGBA (반투명)
		OutlineCmd.Params.Outline.OutlineInvScale = FVector(1.0f / WorldScale.X,
			1.0f / WorldScale.Y, 1.0f / WorldScale.Z);
		OutlineCmd.Params.Outline.OutlineOffset = 0.03f;
		if (ViewMode == EViewMode::Wireframe)
		{
			OutlineCmd.PerObjectConstants.Color = FColor(255, 153, 0, 255).ToVector4();
		}

		OutlineCmd.Params.Outline.PrimitiveType = (PrimType == EPrimitiveType::EPT_Plane ||
			PrimType == EPrimitiveType::EPT_SubUV ||
			PrimType == EPrimitiveType::EPT_Text) ? 0u : 1u;

		// Outline CB
		OutlineCmd.ExtraCB = { FConstantBufferPool::Get().GetBuffer(ECBSlot::Outline, sizeof(FOutlineConstants)), sizeof(FOutlineConstants), ECBSlot::Outline };

		RenderBus.AddCommand(ERenderPass::Outline, OutlineCmd);

		// 보조 컴포넌트(텍스트/빌보드/SubUV)는 AABB 제외 — 메인 메시만 표시
		if (PrimType != EPrimitiveType::EPT_Text &&
			PrimType != EPrimitiveType::EPT_SubUV &&
			!primitiveComponent->IsA<UBillboardComponent>())
		{
			CollectAABBCommand(primitiveComponent, ShowFlags, RenderBus);
		}
	}
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus)
{
	if (!Primitive->IsVisible()) return;

	EPrimitiveType PrimType = Primitive->GetPrimitiveType();

	switch (PrimType)
	{
	case EPrimitiveType::EPT_Cube:
	case EPrimitiveType::EPT_Sphere:
	case EPrimitiveType::EPT_Plane:
	case EPrimitiveType::EPT_StaticMesh:
	{
		if (!ShowFlags.bPrimitives) return;

		// 컴포넌트가 자체 버퍼를 가지면 사용, 아니면 MeshBufferManager에서 조회
		FMeshBuffer* Buffer = Primitive->GetMeshBuffer();
		if (!Buffer) Buffer = &FMeshBufferManager::Get().GetMeshBuffer(PrimType);
		if (!Buffer || !Buffer->IsValid()) return;

		FRenderCommand Cmd = {};
		Cmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix(), FColor::White().ToVector4() };
		if (PrimType == EPrimitiveType::EPT_StaticMesh)
		{
			Cmd.Type = ERenderCommandType::StaticMesh;
			Cmd.Shader = FShaderManager::Get().GetShader(EShaderType::StaticMesh);

			// 섹션별 드로우 정보 수집
			UStaticMeshComp* SMComp = static_cast<UStaticMeshComp*>(Primitive);
			UStaticMesh* SM = SMComp->GetStaticMesh();
			if (SM && SM->GetStaticMeshAsset())
			{
				const auto& Sections = SM->GetStaticMeshAsset()->Sections;
				const auto& Materials = SM->GetStaticMaterials();

				for (const FStaticMeshSection& Section : Sections)
				{
					FMeshSectionDraw Draw;
					Draw.FirstIndex = Section.FirstIndex;
					Draw.IndexCount = Section.NumTriangles * 3;

					// 머티리얼 슬롯 이름으로 매칭
					for (const FStaticMaterial& Mat : Materials)
					{
						if (Mat.MaterialSlotName == Section.MaterialSlotName && Mat.MaterialInterface)
						{
							if (Mat.MaterialInterface->DiffuseTexture)
								Draw.DiffuseSRV = Mat.MaterialInterface->DiffuseTexture->GetSRV();
							Draw.DiffuseColor = Mat.MaterialInterface->DiffuseColor;
							break;
						}
					}

					Cmd.SectionDraws.push_back(Draw);
				}
			}
		}
		else
		{
			Cmd.Type = ERenderCommandType::Primitive;
			Cmd.Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
		}
		Cmd.MeshBuffer = Buffer;
		Cmd.DepthStencilState = EDepthStencilState::Default;
		RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
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
		Cmd.Params.SubUV.Particle = Particle;
		Cmd.Params.SubUV.FrameIndex = SubUVComp->GetFrameIndex();
		Cmd.Params.SubUV.Width = SubUVComp->GetWidth();
		Cmd.Params.SubUV.Height = SubUVComp->GetHeight();
		Cmd.BlendState = EBlendState::AlphaBlend;
		Cmd.DepthStencilState = EDepthStencilState::Default;
		RenderBus.AddCommand(ERenderPass::SubUV, Cmd);
		break;
	}

	default:
		return;
	}
}

void FRenderCollector::CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus)
{
	if (!ShowFlags.bBoundingVolume) return;

	FRenderCommand AABBCmd = {};
	AABBCmd.Type = ERenderCommandType::DebugBox;

	FBoundingBox Box = PrimitiveComponent->GetWorldBoundingBox();

	AABBCmd.Params.AABB.Min = Box.Min;
	AABBCmd.Params.AABB.Max = Box.Max;
	AABBCmd.Params.AABB.Color = FColor::White();

	RenderBus.AddCommand(ERenderPass::Editor, AABBCmd);
}
