#include "RenderCollector.h"
#include "Component/UUIDBillboardComponent.h"
#include "Renderer/RenderCommand.h"
#include "Actor/Actor.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Core/Engine.h"
#include "Component/TextComponent.h"
#include "Debug/EngineLog.h"
#include "Renderer/Renderer.h"
#include "Renderer/TextMeshBuilder.h"
#include "Renderer/SubUVRenderer.h"
#include "Renderer/Material.h"
#include "Renderer/MeshData.h"

void FSceneRenderCollector::CollectRenderCommands(const TArray<AActor*>& Actors, const FFrustum& Frustum,
	const FShowFlags& ShowFlags, const FVector& CameraPosition, FRenderCommandQueue& OutQueue)
{
	// ? UActorComponent가 아니라 UPrimitiveComponent로 바로 받습니다!
	TArray<UPrimitiveComponent*> VisiblePrimitives;
	FrustrumCull(Actors, Frustum, ShowFlags, VisiblePrimitives);

	FRenderer* Renderer = GEngine ? GEngine->GetRenderer() : nullptr;
	if (!Renderer) return;

	FTextMeshBuilder& TextRenderer = Renderer->GetTextRenderer();
	FSubUVRenderer& SubUVRenderer = Renderer->GetSubUVRenderer();

	for (UPrimitiveComponent* Comp : VisiblePrimitives)
	{
		if (!Comp) continue;

		// ─── 1. 텍스트 컴포넌트 ───
		if (Comp->IsA(UTextComponent::StaticClass()))
		{
			UTextComponent* TextComp = static_cast<UTextComponent*>(Comp);
			FRenderMesh* TextMesh = TextComp->GetRenderMesh();

			if (TextMesh)
			{
				bool bBuilt = false;
				if (TextComp->IsTextMeshDirty())
				{
					bBuilt = TextRenderer.BuildTextMesh(TextComp->GetDisplayText(), *TextMesh);
					if (bBuilt)
					{
						TextMesh->bIsDirty = true;
						TextComp->ClearTextMeshDirty();
					}
				}

				if (!TextMesh->Vertices.empty())
				{
					FMaterial* FontMat = TextRenderer.GetFontMaterial();
					if (FontMat)
					{
						FVector4 Color = TextComp->GetTextColor();
						FontMat->SetParameterData("TextColor", &Color, 16);

						FRenderCommand Command;
						Command.RenderMesh = TextMesh;
						Command.Material = FontMat;

						if (!Comp->IsA(UUUIDBillboardComponent::StaticClass()))
						{
							Command.RenderLayer = ERenderLayer::Default;
						}
						else
						{
							Command.RenderLayer = ERenderLayer::Overlay;
						}

						const FVector WorldPos = TextComp->GetRenderWorldPosition();
						const FVector Scale = TextComp->GetRenderWorldScale();

						if (TextComp->IsBillboard())
						{
							const FVector CameraPos = Renderer->GetCameraPosition();
							Command.WorldMatrix = FMatrix::MakeScale(Scale) * FMatrix::MakeBillboard(WorldPos, CameraPos);
						}
						else
						{
							const float TextScale = TextComp->GetTextScale();
							Command.WorldMatrix =
								FMatrix::MakeScale(FVector(TextScale, TextScale, TextScale)) *
								TextComp->GetWorldTransform();
						}

						OutQueue.AddCommand(Command);
					}
				}
			}
			continue;
		}

		// ─── 2. SubUV 스프라이트 컴포넌트 ───
		if (Comp->IsA(USubUVComponent::StaticClass()))
		{
			USubUVComponent* SubUVComponent = static_cast<USubUVComponent*>(Comp);
			FRenderMesh* SubUVMesh = SubUVComponent->GetSubUVMesh();
			if (SubUVMesh && SubUVRenderer.BuildSubUVMesh(SubUVComponent->GetSize(), *SubUVMesh))
			{
				SubUVMesh->bIsDirty = true;
				float TotalTime = GEngine ? static_cast<float>(GEngine->GetTimer().GetTotalTime()) : 0.0f;
				SubUVRenderer.UpdateAnimationParams(
					SubUVComponent->GetColumns(), SubUVComponent->GetRows(), SubUVComponent->GetTotalFrames(),
					SubUVComponent->GetFirstFrame(), SubUVComponent->GetLastFrame(),
					SubUVComponent->GetFPS(), TotalTime, SubUVComponent->IsLoop()
				);

				FMaterial* SubUVMat = SubUVRenderer.GetSubUVMaterial();
				if (SubUVMat)
				{
					FRenderCommand Command;
					Command.RenderMesh = SubUVMesh;
					Command.Material = SubUVMat;
					Command.WorldMatrix = SubUVComponent->GetWorldTransform();

					if (SubUVComponent->IsBillboard())
					{
						const FVector CameraPos = Renderer->GetCameraPosition();
						const FVector WorldPos = Command.WorldMatrix.GetTranslation();
						const FVector Scale = Command.WorldMatrix.GetScaleVector();
						Command.WorldMatrix = FMatrix::MakeScale(Scale) * FMatrix::MakeBillboard(WorldPos, CameraPos);
					}

					OutQueue.AddCommand(Command);
				}
			}
			continue;
		}

		if (Comp->IsA(UStaticMeshComponent::StaticClass()))
		{
			UStaticMeshComponent* SMC = static_cast<UStaticMeshComponent*>(Comp);
			FRenderMesh* TargetMesh = SMC->GetRenderMesh();

			if (TargetMesh)
			{
				int32 NumSections = TargetMesh->GetNumSection();

				if (NumSections <= 0)
				{
					// 섹션이 없으면 예전처럼 통째로 1번 그립니다.
					FRenderCommand Command;
					Command.RenderMesh = TargetMesh;
					Command.WorldMatrix = SMC->GetWorldTransform();

					std::shared_ptr<FMaterial> MatPtr = SMC->GetMaterial(0);
					Command.Material = MatPtr ? MatPtr.get() : Renderer->GetDefaultMaterial();

					OutQueue.AddCommand(Command);
				}
				else
				{
					// ? 핵심: 메쉬의 섹션 개수만큼 루프를 돌며 주문서를 여러 장 쪼개서 발급!
					for (int32 i = 0; i < NumSections; ++i)
					{
						const FMeshSection& Section = TargetMesh->Sections[i];

						FRenderCommand Command;
						Command.RenderMesh = TargetMesh;
						Command.WorldMatrix = SMC->GetWorldTransform();

						// 1. 주문서에 이번에 그릴 인덱스 구간 적기
						Command.IndexStart = Section.StartIndex;
						Command.IndexCount = Section.IndexCount;

						// 2. 인덱스(i)에 맞는 머티리얼을 꺼내서 주문서에 붙이기
						std::shared_ptr<FMaterial> MatPtr = SMC->GetMaterial(i);
						Command.Material = MatPtr ? MatPtr.get() : Renderer->GetDefaultMaterial();
	for (UPrimitiveComponent* PrimitiveComponent : VisiblePrimitives)
	{
		if (!PrimitiveComponent) continue;

		// ─── 텍스트 컴포넌트 ───
		if (PrimitiveComponent->IsA(UTextComponent::StaticClass()))
		{
			UTextComponent* TextComp = static_cast<UTextComponent*>(PrimitiveComponent);
			FMeshData* TextMesh = TextComp->GetTextMesh();
			
			if (TextMesh && TextRenderer.BuildTextMesh(TextComp->GetDisplayText(), *TextMesh))
			{
				FMaterial* FontMat = TextRenderer.GetFontMaterial();
				if (FontMat)
				{
					FVector4 Color = TextComp->GetTextColor();
					FontMat->SetParameterData("TextColor", &Color, 16);

					FRenderCommand Command;
					Command.MeshData = TextMesh;
					Command.Material = FontMat;
					// TODO: UUID 렌더링 기능 재구현되면 아래 1줄 삭제
					if (!PrimitiveComponent->IsA(UUUIDBillboardComponent::StaticClass()))
					{
						Command.RenderLayer = ERenderLayer::Default;  // ← Overlay → Default
					}
					else
					{
						Command.RenderLayer = ERenderLayer::Overlay;
					}
				
					
					const FVector WorldPos = TextComp->GetRenderWorldPosition();
					const FVector Scale = TextComp->GetRenderWorldScale();

					if (TextComp->IsBillboard())
					{
						Command.WorldMatrix = FMatrix::MakeScale(Scale) * FMatrix::MakeBillboard(WorldPos, CameraPosition);
					}
					else
					{
						const float TextScale = TextComp->GetTextScale();
						Command.WorldMatrix =
							FMatrix::MakeScale(FVector(TextScale, TextScale, TextScale)) *
							TextComp->GetWorldTransform();
					}

						OutQueue.AddCommand(Command);
					}
				}
			}
					OutQueue.AddCommand(Command);
				}
			}
			continue;
		}

		// ─── SubUV 스프라이트 통합 ───
		// TODO: 일반적인 프리미티브와 RenderCommand build 경로 통합
		if (PrimitiveComponent->IsA(USubUVComponent::StaticClass()))
		{
			USubUVComponent* SubUVComponent = static_cast<USubUVComponent*>(PrimitiveComponent);
			FMeshData* SubUVMesh = SubUVComponent->GetSubUVMesh();
			if (SubUVMesh && SubUVRenderer.BuildSubUVMesh(SubUVComponent->GetSize(), *SubUVMesh))
			{
				float TotalTime = GEngine ? static_cast<float>(GEngine->GetTimer().GetTotalTime()) : 0.0f;
				SubUVRenderer.UpdateAnimationParams(
					SubUVComponent->GetColumns(), SubUVComponent->GetRows(), SubUVComponent->GetTotalFrames(),
					SubUVComponent->GetFirstFrame(), SubUVComponent->GetLastFrame(),
					SubUVComponent->GetFPS(), TotalTime, SubUVComponent->IsLoop()
				);

				FMaterial* SubUVMat = SubUVRenderer.GetSubUVMaterial();
				if (SubUVMat)
				{
					FRenderCommand Command;
					Command.MeshData = SubUVMesh;
					Command.Material = SubUVMat;
					Command.WorldMatrix = SubUVComponent->GetWorldTransform();

					if (SubUVComponent->IsBillboard())
					{
						const FVector WorldPos = Command.WorldMatrix.GetTranslation();
						const FVector Scale = Command.WorldMatrix.GetScaleVector();
						Command.WorldMatrix = FMatrix::MakeScale(Scale) * FMatrix::MakeBillboard(WorldPos, CameraPosition);
					}

					OutQueue.AddCommand(Command);
				}
			}
			continue;
		}

		// ─── 일반 프리미티브 ───
		if (!PrimitiveComponent->GetPrimitive() || !PrimitiveComponent->GetPrimitive()->GetMeshData())
		{
			continue;
		}
	}
}

void FSceneRenderCollector::FrustrumCull(const TArray<AActor*>& Actors, const FFrustum& Frustum,
	const FShowFlags& ShowFlags, TArray<UPrimitiveComponent*>& OutVisible)
{
	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor->IsPendingDestroy() || !Actor->IsVisible()) continue;
		if (!Actor->IsVisible()) continue;

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component->IsA(UPrimitiveComponent::StaticClass())) continue;

			UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);

			const bool bIsUUID = PrimitiveComponent->IsA(UUUIDBillboardComponent::StaticClass());
			const bool bIsSubUV = PrimitiveComponent->IsA(USubUVComponent::StaticClass());
			const bool bIsText = PrimitiveComponent->IsA(UTextComponent::StaticClass());
			// ─── ShowFlags에 따른 필터링 ───
			if (bIsUUID)
			{
				if (!ShowFlags.HasFlag(EEngineShowFlags::SF_UUID)) continue;
			}
			else if (bIsSubUV)
			{
				if (!ShowFlags.HasFlag(EEngineShowFlags::SF_Billboard))
				{
					continue;
				}
			}
			else if (bIsText)
			{
				if (!ShowFlags.HasFlag(EEngineShowFlags::SF_Text))
				{
					continue;
				}
			}
			else
			{
				if (!ShowFlags.HasFlag(EEngineShowFlags::SF_Primitives)) continue;
				if (!PrimitiveComponent->GetRenderMesh()) continue;
			}

			if (Frustum.IsVisible(PrimitiveComponent->GetWorldBounds()))
			{
				OutVisible.push_back(PrimitiveComponent);
			}
		}
	}
}
