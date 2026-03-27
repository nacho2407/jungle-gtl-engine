#include "RenderCollector.h"
#include "Component/UUIDBillboardComponent.h"
#include "Renderer/RenderCommand.h"
#include "Actor/Actor.h"
#include "Component/SubUVComponent.h"
#include "Core/FEngine.h"
#include "Component/TextComponent.h"
#include "Renderer/Renderer.h"
#include "Renderer/TextMeshBuilder.h"
#include "Renderer/SubUVRenderer.h"
#include "Renderer/Material.h"

void FSceneRenderCollector::CollectRenderCommands(const TArray<AActor*>& Actors, const FFrustum& Frustum,
	const FShowFlags& ShowFlags, FRenderCommandQueue& OutQueue)
{
	TArray<UPrimitiveComponent*> VisiblePrimitives;
	FrustrumCull(Actors, Frustum, ShowFlags, VisiblePrimitives);

	CRenderer* Renderer = GEngine->GetCore()->GetRenderer();
	if (!Renderer) return;

	CTextMeshBuilder& TextRenderer = Renderer->GetTextRenderer();
	CSubUVRenderer& SubUVRenderer = Renderer->GetSubUVRenderer();

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
				float TotalTime = static_cast<float>(GEngine->GetCore()->GetTimer().GetTotalTime());
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

		// ─── 일반 프리미티브 ───
		if (!PrimitiveComponent->GetPrimitive() || !PrimitiveComponent->GetPrimitive()->GetMeshData())
		{
			continue;
		}

		FRenderCommand Command;
		Command.MeshData = PrimitiveComponent->GetPrimitive()->GetMeshData();
		Command.WorldMatrix = PrimitiveComponent->GetWorldTransform();
		Command.Material = PrimitiveComponent->GetMaterial();
		OutQueue.AddCommand(Command);
	}
}

void FSceneRenderCollector::FrustrumCull(const TArray<AActor*>& Actors, const FFrustum& Frustum,
	const FShowFlags& ShowFlags, TArray<UPrimitiveComponent*>& OutVisible)
{
	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor->IsPendingDestroy()) continue;
		if (!Actor->IsVisible()) continue;

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component->IsA(UPrimitiveComponent::StaticClass())) continue;

			UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);

			const bool bIsUUID = PrimitiveComponent->IsA(UUUIDBillboardComponent::StaticClass());
			const bool bIsSubUV = PrimitiveComponent->IsA(USubUVComponent::StaticClass());
			const bool bIsText = PrimitiveComponent->IsA(UTextComponent::StaticClass());

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
				if (!PrimitiveComponent->GetPrimitive() || !PrimitiveComponent->GetPrimitive()->GetMeshData()) continue;
			}

			if (Frustum.IsVisible(PrimitiveComponent->GetWorldBounds()))
			{
				OutVisible.push_back(PrimitiveComponent);
			}
		}
	}
}
