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

#include <algorithm>

// Shader 선택 헬퍼 — StaticMesh vs Primitive
static FShader* GetPrimitiveShader(UPrimitiveComponent* Comp)
{
	return FShaderManager::Get().GetShader(
		Comp->IsA<UStaticMeshComp>() ? EShaderType::StaticMesh : EShaderType::Primitive);
}

void FRenderCollector::CollectWorld(UWorld* World, const TArray<AActor*>& SelectedActors, FRenderBus& RenderBus)
{
	if (!World) return;

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor) continue;
		bool bSelected = std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end();
		CollectFromActor(Actor, bSelected, RenderBus);
	}
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus)
{
	FGridEntry Entry = {};
	Entry.Grid.GridSpacing = GridSpacing;
	Entry.Grid.GridHalfLineCount = GridHalfLineCount;
	RenderBus.AddGridEntry(std::move(Entry));
}

void FRenderCollector::CollectGizmo(UGizmoComponent* Gizmo, FRenderBus& RenderBus)
{
	const FShowFlags& ShowFlags = RenderBus.GetShowFlags();

	if (ShowFlags.bGizmo == false) return;
	if (!Gizmo || !Gizmo->IsVisible()) return;

	FMeshBuffer* GizmoMesh = Gizmo->GetMeshBuffer();
	FMatrix WorldMatrix = Gizmo->GetWorldMatrix();
	bool bHolding = Gizmo->IsHolding();
	int32 SelectedAxis = Gizmo->GetSelectedAxis();

	auto CreateGizmoCmd = [&](bool bInner) {
		FRenderCommand Cmd = {};
		Cmd.Shader = FShaderManager::Get().GetShader(EShaderType::Gizmo);
		Cmd.MeshBuffer = GizmoMesh;
		Cmd.PerObjectConstants = FPerObjectConstants{ WorldMatrix };

		auto& G = Cmd.ExtraCB.Bind<FGizmoConstants>(
			FConstantBufferPool::Get().GetBuffer(ECBSlot::Gizmo, sizeof(FGizmoConstants)), ECBSlot::Gizmo);
		G.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		G.bIsInnerGizmo = bInner ? 1 : 0;
		G.bClicking = bHolding ? 1 : 0;
		G.SelectedAxis = SelectedAxis >= 0 ? (uint32)SelectedAxis : 0xffffffffu;
		G.HoveredAxisOpacity = 0.3f;

		return Cmd;
		};

	RenderBus.AddCommand(ERenderPass::GizmoOuter, CreateGizmoCmd(false));
	RenderBus.AddCommand(ERenderPass::GizmoInner, CreateGizmoCmd(true));
}

void FRenderCollector::CollectFromActor(AActor* Actor, bool bSelected, FRenderBus& RenderBus)
{
	if (!Actor->IsVisible()) return;

	for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
	{
		CollectFromComponent(Primitive, RenderBus);

		if (bSelected)
		{
			CollectSelectionEffects(Primitive, RenderBus);
		}
	}
}

void FRenderCollector::CollectFromComponent(UPrimitiveComponent* Primitive, FRenderBus& RenderBus)
{
	if (!Primitive->IsVisible()) return;

	const FShowFlags& ShowFlags = RenderBus.GetShowFlags();

	// SubUV — SubUVBatcher 경유
	if (USubUVComponent* SubUVComp = Cast<USubUVComponent>(Primitive))
	{
		const FParticleResource* Particle = SubUVComp->GetParticle();
		if (!Particle || !Particle->IsLoaded()) return;

		FSubUVEntry Entry = {};
		Entry.PerObject = FPerObjectConstants::FromWorldMatrix(Primitive->GetWorldMatrix());
		Entry.SubUV.Particle = Particle;
		Entry.SubUV.FrameIndex = SubUVComp->GetFrameIndex();
		Entry.SubUV.Width = SubUVComp->GetWidth();
		Entry.SubUV.Height = SubUVComp->GetHeight();
		RenderBus.AddSubUVEntry(std::move(Entry));
		return;
	}

	// MeshBuffer가 없는 컴포넌트(Text, Billboard 등)는 이 경로에서 렌더링하지 않음
	if (!ShowFlags.bPrimitives) return;

	FMeshBuffer* Buffer = Primitive->GetMeshBuffer();
	if (!Buffer || !Buffer->IsValid()) return;

	FRenderCommand Cmd = {};
	Cmd.PerObjectConstants = FPerObjectConstants::FromWorldMatrix(Primitive->GetWorldMatrix());

	Cmd.Shader = GetPrimitiveShader(Primitive);

	if (UStaticMeshComp* SMComp = Cast<UStaticMeshComp>(Primitive))
	{
		// 섹션별 드로우 정보 수집
		UStaticMesh* SM = SMComp->GetStaticMesh();
		if (SM && SM->GetStaticMeshAsset())
		{
			const auto& Sections = SM->GetStaticMeshAsset()->Sections;
			const auto& Slots = SM->GetStaticMaterials(); // 기본 슬롯 정보
			const auto& Overrides = SMComp->GetOverrideMaterials(); // 인스턴스별 오버라이드 정보

			for (const FStaticMeshSection& Section : Sections)
			{
				FMeshSectionDraw Draw;
				Draw.FirstIndex = Section.FirstIndex;
				Draw.IndexCount = Section.NumTriangles * 3;

				// 머티리얼 슬롯 이름으로 매칭
				for (int32 i = 0; i < Slots.size(); ++i)
				{
					if (Slots[i].MaterialSlotName == Section.MaterialSlotName)
					{
						if (i < Overrides.size() && Overrides[i])
						{
							auto& Mat = Overrides[i];
							if (Mat->DiffuseTexture)
								Draw.DiffuseSRV = Mat->DiffuseTexture->GetSRV();
							Draw.DiffuseColor = Mat->DiffuseColor;
						}
						break;
					}
				}

				Cmd.SectionDraws.push_back(Draw);
			}
		}
	}
	Cmd.MeshBuffer = Buffer;
	RenderBus.AddCommand(ERenderPass::Opaque, Cmd);
}

void FRenderCollector::CollectSelectionEffects(UPrimitiveComponent* Primitive, FRenderBus& RenderBus)
{
	if (!Primitive->IsVisible()) return;

	const FShowFlags& ShowFlags = RenderBus.GetShowFlags();
	EViewMode ViewMode = RenderBus.GetViewMode();

	FRenderCommand BaseCmd{};
	BaseCmd.MeshBuffer = Primitive->GetMeshBuffer();
	BaseCmd.PerObjectConstants = FPerObjectConstants{ Primitive->GetWorldMatrix() };
	FVector WorldScale = Primitive->GetWorldScale();

	// Text — 빌보드 방식의 폰트 렌더링
	if (UTextRenderComponent* TextComp = Cast<UTextRenderComponent>(Primitive))
	{
		const FFontResource* Font = TextComp->GetFont();
		if (!Font || !Font->IsLoaded()) return;
		const FString& Text = TextComp->GetText();
		if (Text.empty()) return;

		// 카메라 축을 직접 사용하여 빌보드 행렬 구성 (FontBatcher와 동일한 축 보장)
		// Right를 반전: CalculateOutlineMatrix의 CenterY(-0.5)와 결합해 올바른 방향이 됨
		FVector BillboardForward = (RenderBus.GetCameraForward() * -1.0f);
		FMatrix RotMatrix;
		RotMatrix.SetAxes(BillboardForward, RenderBus.GetCameraRight() * -1.0f, RenderBus.GetCameraUp());
		FMatrix PerViewBillboard = FMatrix::MakeScaleMatrix(TextComp->GetWorldScale())
			* RotMatrix * FMatrix::MakeTranslationMatrix(TextComp->GetWorldLocation());
		FMatrix outlineMatrix = TextComp->CalculateOutlineMatrix(PerViewBillboard);
		WorldScale = outlineMatrix.GetScale();

		BaseCmd.PerObjectConstants.Model = outlineMatrix;

		if (ShowFlags.bBillboardText)
		{
			FFontEntry Entry = {};
			Entry.PerObject = FPerObjectConstants{ PerViewBillboard };
			Entry.PerObject.Color = TextComp->GetColor();
			Entry.Font.Text = &Text;
			Entry.Font.Font = Font;
			Entry.Font.Scale = TextComp->GetFontSize();
			RenderBus.AddFontEntry(std::move(Entry));
		}
	}

	if (!Primitive->SupportsOutline()) return;

	// StencilBuffer Mask
	FRenderCommand MaskCmd = BaseCmd;
	MaskCmd.Shader = GetPrimitiveShader(Primitive);
	RenderBus.AddCommand(ERenderPass::StencilMask, MaskCmd);

	// Outline
	FRenderCommand OutlineCmd = BaseCmd;
	OutlineCmd.Shader = FShaderManager::Get().GetShader(
		Primitive->IsA<UStaticMeshComp>() ? EShaderType::OutlinePNCT : EShaderType::Outline);

	auto& Outline = OutlineCmd.ExtraCB.Bind<FOutlineConstants>(
		FConstantBufferPool::Get().GetBuffer(ECBSlot::Outline, sizeof(FOutlineConstants)), ECBSlot::Outline);
	Outline.OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 0.7f); // RGBA (반투명)
	Outline.OutlineInvScale = FVector(1.0f / WorldScale.X,
		1.0f / WorldScale.Y, 1.0f / WorldScale.Z);
	Outline.OutlineOffset = 0.03f;
	if (ViewMode == EViewMode::Wireframe)
	{
		OutlineCmd.PerObjectConstants.Color = FColor(255, 153, 0, 255).ToVector4();
	}
	Outline.bIs3D = Primitive->IsFlat() ? 0u : 1u;

	RenderBus.AddCommand(ERenderPass::Outline, OutlineCmd);

	// 보조 컴포넌트(텍스트/빌보드/SubUV)는 AABB 제외 — 메인 메시만 표시
	if (!Primitive->IsA<UBillboardComponent>())
	{
		CollectAABBCommand(Primitive, RenderBus);
	}
}

void FRenderCollector::CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, FRenderBus& RenderBus)
{
	if (!RenderBus.GetShowFlags().bBoundingVolume) return;

	FAABBEntry Entry = {};
	FBoundingBox Box = PrimitiveComponent->GetWorldBoundingBox();
	Entry.AABB.Min = Box.Min;
	Entry.AABB.Max = Box.Max;
	Entry.AABB.Color = FColor::White();

	RenderBus.AddAABBEntry(std::move(Entry));
}
