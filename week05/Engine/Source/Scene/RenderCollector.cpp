#include "RenderCollector.h"

#include "Actor/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Core/Engine.h"
#include "Renderer/Material.h"
#include "Renderer/MeshData.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer.h"
#include "Renderer/SubUVRenderer.h"
#include "Renderer/TextMeshBuilder.h"
#include "Scene/Scene.h"

void FSceneRenderCollector::CollectRenderCommands(
	UScene* Scene,
	const FFrustum& Frustum,
	const FShowFlags& ShowFlags,
	const FVector& CameraPosition,
	FRenderCommandQueue& OutQueue)
{
	if (!Scene)
	{
		return;
	}

	TArray<UPrimitiveComponent*> VisiblePrimitives;
	FrustrumCull(Scene, Frustum, ShowFlags, VisiblePrimitives);

	FRenderer* Renderer = GEngine ? GEngine->GetRenderer() : nullptr;
	if (!Renderer)
	{
		return;
	}

	FTextMeshBuilder& TextRenderer = Renderer->GetTextRenderer();
	FSubUVRenderer& SubUVRenderer = Renderer->GetSubUVRenderer();
	OutQueue.Commands.reserve(OutQueue.Commands.size() + VisiblePrimitives.size());

	for (UPrimitiveComponent* Comp : VisiblePrimitives)
	{
		if (!Comp)
		{
			continue;
		}

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
						Command.RenderLayer =
							Comp->IsA(UUUIDBillboardComponent::StaticClass()) ? ERenderLayer::Overlay : ERenderLayer::Default;

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
			continue;
		}

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
					SubUVComponent->GetFPS(), TotalTime, SubUVComponent->IsLoop());

				FMaterial* SubUVMat = SubUVRenderer.GetSubUVMaterial();
				if (SubUVMat)
				{
					FRenderCommand Command;
					Command.RenderMesh = SubUVMesh;
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

		if (Comp->IsA(UStaticMeshComponent::StaticClass()))
		{
			UStaticMeshComponent* SMC = static_cast<UStaticMeshComponent*>(Comp);
			FRenderMesh* TargetMesh = SMC->GetRenderMesh();

			if (TargetMesh)
			{
				const int32 NumSections = TargetMesh->GetNumSection();
				if (NumSections <= 0)
				{
					FRenderCommand Command;
					Command.RenderMesh = TargetMesh;
					Command.WorldMatrix = SMC->GetWorldTransform();
					std::shared_ptr<FMaterial> MatPtr = SMC->GetMaterial(0);
					Command.Material = MatPtr ? MatPtr.get() : Renderer->GetDefaultMaterial();
					OutQueue.AddCommand(Command);
				}
				else
				{
					for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
					{
						const FMeshSection& Section = TargetMesh->Sections[SectionIndex];

						FRenderCommand Command;
						Command.RenderMesh = TargetMesh;
						Command.WorldMatrix = SMC->GetWorldTransform();
						Command.IndexStart = Section.StartIndex;
						Command.IndexCount = Section.IndexCount;

						std::shared_ptr<FMaterial> MatPtr = SMC->GetMaterial(SectionIndex);
						Command.Material = MatPtr ? MatPtr.get() : Renderer->GetDefaultMaterial();
						OutQueue.AddCommand(Command);
					}
				}
			}
			continue;
		}
	}
}

void FSceneRenderCollector::FrustrumCull(
	UScene* Scene,
	const FFrustum& Frustum,
	const FShowFlags& ShowFlags,
	TArray<UPrimitiveComponent*>& OutVisible)
{
	if (!Scene)
	{
		return;
	}

	const bool bShowUUID = ShowFlags.HasFlag(EEngineShowFlags::SF_UUID);
	const bool bShowBillboard = ShowFlags.HasFlag(EEngineShowFlags::SF_Billboard);
	const bool bShowText = ShowFlags.HasFlag(EEngineShowFlags::SF_Text);
	const bool bShowPrimitives = ShowFlags.HasFlag(EEngineShowFlags::SF_Primitives);
	if (!bShowUUID && !bShowBillboard && !bShowText && !bShowPrimitives)
	{
		return;
	}

	const auto* UUIDBillboardClass = UUUIDBillboardComponent::StaticClass();
	const auto* SubUVClass = USubUVComponent::StaticClass();
	const auto* TextClass = UTextComponent::StaticClass();

	TArray<UPrimitiveComponent*> CandidatePrimitives;
	Scene->QueryPrimitivesByFrustum(Frustum, CandidatePrimitives);
	OutVisible.reserve(OutVisible.size() + CandidatePrimitives.size());

	for (UPrimitiveComponent* PrimitiveComponent : CandidatePrimitives)
	{
		if (!PrimitiveComponent || PrimitiveComponent->IsPendingKill())
		{
			continue;
		}

		AActor* OwnerActor = PrimitiveComponent->GetTypedOuter<AActor>();
		if (!OwnerActor)
		{
			OwnerActor = PrimitiveComponent->GetOwner();
		}
		if (!OwnerActor || OwnerActor->IsPendingDestroy() || !OwnerActor->IsVisible())
		{
			continue;
		}

		const bool bIsUUID = PrimitiveComponent->IsA(UUIDBillboardClass);
		const bool bIsSubUV = PrimitiveComponent->IsA(SubUVClass);
		const bool bIsText = PrimitiveComponent->IsA(TextClass);

		if (bIsUUID)
		{
			if (!bShowUUID)
			{
				continue;
			}
		}
		else if (bIsSubUV)
		{
			if (!bShowBillboard)
			{
				continue;
			}
		}
		else if (bIsText)
		{
			if (!bShowText)
			{
				continue;
			}
		}
		else
		{
			if (!bShowPrimitives)
			{
				continue;
			}

			if (!PrimitiveComponent->GetRenderMesh())
			{
				continue;
			}
		}

		OutVisible.push_back(PrimitiveComponent);
	}
}
