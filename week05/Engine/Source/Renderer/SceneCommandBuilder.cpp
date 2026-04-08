#include "Renderer/SceneCommandBuilder.h"

#include <algorithm>

#include "Component/BillboardComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Renderer/Material.h"
#include "Renderer/MeshData.h"
#include "Renderer/RenderCommand.h"

namespace
{
	static uint8 ToColorChannel(float Value)
	{
		const float Clamped = (std::max)(0.0f, (std::min)(1.0f, Value));
		return static_cast<uint8>(Clamped * 255.0f + 0.5f);
	}
}

uint32 FSceneCommandBuilder::ToColorKey(const FVector4& Color)
{
	const uint32 A = static_cast<uint32>(ToColorChannel(Color.W));
	const uint32 R = static_cast<uint32>(ToColorChannel(Color.X));
	const uint32 G = static_cast<uint32>(ToColorChannel(Color.Y));
	const uint32 B = static_cast<uint32>(ToColorChannel(Color.Z));
	return (A << 24) | (R << 16) | (G << 8) | B;
}

void FSceneCommandBuilder::UpdateSubUVMaterialParams(
	FMaterial& Material,
	int32 Columns,
	int32 Rows,
	int32 TotalFrames,
	int32 FirstFrame,
	int32 LastFrame,
	float FPS,
	float ElapsedTime,
	bool bLoop)
{
	if (Columns <= 0 || Rows <= 0)
	{
		return;
	}

	const int32 SafeTotalFrames = (std::max)(1, TotalFrames);
	const int32 SafeFirstFrame = (std::max)(0, (std::min)(FirstFrame, SafeTotalFrames - 1));
	const int32 SafeLastFrame = (std::max)(0, (std::min)(LastFrame, SafeTotalFrames - 1));
	const float SafeFPS = (std::max)(0.0f, FPS);

	const int32 FirstRowRaw = SafeFirstFrame / Columns;
	const int32 LastRowRaw = SafeLastFrame / Columns;
	int32 FirstRow = (std::max)(0, (std::min)(FirstRowRaw, Rows - 1));
	int32 LastRow = (std::max)(0, (std::min)(LastRowRaw, Rows - 1));
	if (FirstRow > LastRow)
	{
		std::swap(FirstRow, LastRow);
	}

	const int32 PlayableRowCount = LastRow - FirstRow + 1;
	const int32 AnimationFrame = static_cast<int32>(ElapsedTime * SafeFPS);

	int32 RowIndex = FirstRow;
	if (PlayableRowCount > 0)
	{
		if (bLoop)
		{
			RowIndex = FirstRow + (AnimationFrame % PlayableRowCount);
		}
		else
		{
			RowIndex = FirstRow + (std::min)(AnimationFrame, PlayableRowCount - 1);
		}
	}

	const int32 TargetColumn = 0;
	const int32 FrameIndex = RowIndex * Columns + TargetColumn;
	const int32 Col = FrameIndex % Columns;
	const int32 Row = FrameIndex / Columns;

	const FVector2 CellSize(1.0f / static_cast<float>(Columns), 1.0f / static_cast<float>(Rows));
	const FVector2 UVOffset(static_cast<float>(Col) * CellSize.X, static_cast<float>(Row) * CellSize.Y);

	Material.SetParameterData("CellSize", &CellSize, sizeof(FVector2));
	Material.SetParameterData("UVOffset", &UVOffset, sizeof(FVector2));
}

FMaterial* FSceneCommandBuilder::GetOrCreateTextMaterial(const FSceneCommandBuildContext& BuildContext, const FVector4& TextColor)
{
	if (!BuildContext.TextFeature)
	{
		return nullptr;
	}

	const uint32 ColorKey = ToColorKey(TextColor);
	const auto Found = TextMaterialsByColor.find(ColorKey);
	if (Found != TextMaterialsByColor.end())
	{
		return Found->second.get();
	}

	FMaterial* BaseFontMaterial = BuildContext.TextFeature->GetBaseMaterial();
	if (!BaseFontMaterial)
	{
		return nullptr;
	}

	std::unique_ptr<FDynamicMaterial> OwnedMaterial = BaseFontMaterial->CreateDynamicMaterial();
	if (!OwnedMaterial)
	{
		return BaseFontMaterial;
	}

	std::shared_ptr<FDynamicMaterial> Material(OwnedMaterial.release());
	Material->SetVectorParameter("TextColor", TextColor);

	FDynamicMaterial* RawMaterial = Material.get();
	TextMaterialsByColor[ColorKey] = std::move(Material);
	return RawMaterial;
}

FMaterial* FSceneCommandBuilder::GetOrCreateSubUVMaterial(
	const FSceneCommandBuildContext& BuildContext,
	const USubUVComponent* Component)
{
	if (!Component || !BuildContext.SubUVFeature)
	{
		return nullptr;
	}

	FMaterial* BaseSubUVMaterial = BuildContext.SubUVFeature->GetBaseMaterial();
	if (!BaseSubUVMaterial)
	{
		return nullptr;
	}

	auto Found = SubUVMaterialsByComponent.find(Component);
	if (Found == SubUVMaterialsByComponent.end())
	{
		std::unique_ptr<FDynamicMaterial> OwnedMaterial = BaseSubUVMaterial->CreateDynamicMaterial();
		if (!OwnedMaterial)
		{
			return BaseSubUVMaterial;
		}

		std::shared_ptr<FDynamicMaterial> Material(OwnedMaterial.release());
		Found = SubUVMaterialsByComponent.emplace(Component, std::move(Material)).first;
	}

	FDynamicMaterial* Material = Found->second.get();
	if (!Material)
	{
		return BaseSubUVMaterial;
	}

	UpdateSubUVMaterialParams(
		*Material,
		Component->GetColumns(),
		Component->GetRows(),
		Component->GetTotalFrames(),
		Component->GetFirstFrame(),
		Component->GetLastFrame(),
		Component->GetFPS(),
		BuildContext.TotalTimeSeconds,
		Component->IsLoop());

	return Material;
}

void FSceneCommandBuilder::PruneStaleSubUVMaterials(const TArray<const USubUVComponent*>& ActiveComponents)
{
	for (auto It = SubUVMaterialsByComponent.begin(); It != SubUVMaterialsByComponent.end();)
	{
		if (std::find(ActiveComponents.begin(), ActiveComponents.end(), It->first) == ActiveComponents.end())
		{
			It = SubUVMaterialsByComponent.erase(It);
			continue;
		}

		++It;
	}
}

void FSceneCommandBuilder::BuildQueue(
	const FSceneCommandBuildContext& BuildContext,
	const FSceneRenderPacket& Packet,
	const FVector& CameraPosition,
	FRenderCommandQueue& OutQueue)
{
	for (const FSceneMeshPrimitive& Primitive : Packet.MeshPrimitives)
	{
		UStaticMeshComponent* MeshComponent = Primitive.Component;
		if (!MeshComponent)
		{
			continue;
		}

		FRenderMesh* TargetMesh = MeshComponent->GetRenderMesh();
		if (!TargetMesh)
		{
			continue;
		}

		const int32 SectionCount = TargetMesh->GetNumSection();
		if (SectionCount <= 0)
		{
			FRenderCommand Command;
			Command.RenderMesh = TargetMesh;
			Command.WorldMatrix = MeshComponent->GetWorldTransform();
			std::shared_ptr<FMaterial> Material = MeshComponent->GetMaterial(0);
			Command.Material = Material ? Material.get() : BuildContext.DefaultMaterial;
			OutQueue.AddCommand(Command);
			continue;
		}

		for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
		{
			const FMeshSection& Section = TargetMesh->Sections[SectionIndex];

			FRenderCommand Command;
			Command.RenderMesh = TargetMesh;
			Command.WorldMatrix = MeshComponent->GetWorldTransform();
			Command.IndexStart = Section.StartIndex;
			Command.IndexCount = Section.IndexCount;

			std::shared_ptr<FMaterial> Material = MeshComponent->GetMaterial(SectionIndex);
			Command.Material = Material ? Material.get() : BuildContext.DefaultMaterial;
			OutQueue.AddCommand(Command);
		}
	}

	for (const FSceneTextPrimitive& Primitive : Packet.TextPrimitives)
	{
		UTextRenderComponent* TextComponent = Primitive.Component;
		if (!TextComponent)
		{
			continue;
		}

		FRenderMesh* TextMesh = TextComponent->GetRenderMesh();
		if (!TextMesh)
		{
			continue;
		}

		if (TextComponent->IsTextMeshDirty())
		{
			if (BuildContext.TextFeature && BuildContext.TextFeature->BuildMesh(
				TextComponent->GetDisplayText(),
				*TextMesh,
				1.0f,
				TextComponent->GetHorizontalAlignment(),
				TextComponent->GetVerticalAlignment()))
			{
				TextMesh->bIsDirty = true;
				TextComponent->ClearTextMeshDirty();
			}
		}

		if (TextMesh->Vertices.empty())
		{
			continue;
		}

		FMaterial* TextMaterial = GetOrCreateTextMaterial(BuildContext, TextComponent->GetTextColor());
		if (!TextMaterial && BuildContext.TextFeature)
		{
			TextMaterial = BuildContext.TextFeature->GetBaseMaterial();
		}
		if (!TextMaterial)
		{
			continue;
		}

		FRenderCommand Command;
		Command.RenderMesh = TextMesh;
		Command.Material = TextMaterial;
		Command.RenderLayer = TextComponent->IsA(UUUIDBillboardComponent::StaticClass()) ? ERenderLayer::Overlay : ERenderLayer::Default;

		const FVector WorldPosition = TextComponent->GetRenderWorldPosition();
		const FVector WorldScale = TextComponent->GetRenderWorldScale();
		if (TextComponent->IsBillboard())
		{
			Command.WorldMatrix = FMatrix::MakeScale(WorldScale) * FMatrix::MakeBillboard(WorldPosition, CameraPosition);
		}
		else
		{
			const float TextScale = TextComponent->GetTextScale();
			Command.WorldMatrix = FMatrix::MakeScale(FVector(TextScale, TextScale, TextScale)) * TextComponent->GetWorldTransform();
		}

		OutQueue.AddCommand(Command);
	}

	TArray<const USubUVComponent*> ActiveSubUVComponents;
	ActiveSubUVComponents.reserve(Packet.SubUVPrimitives.size());

	for (const FSceneSubUVPrimitive& Primitive : Packet.SubUVPrimitives)
	{
		USubUVComponent* SubUVComponent = Primitive.Component;
		if (!SubUVComponent)
		{
			continue;
		}

		FRenderMesh* SubUVMesh = SubUVComponent->GetSubUVMesh();
		if (!SubUVMesh)
		{
			continue;
		}

		if (!BuildContext.SubUVFeature || !BuildContext.SubUVFeature->BuildMesh(SubUVComponent->GetSize(), *SubUVMesh))
		{
			continue;
		}

		SubUVMesh->bIsDirty = true;

		FMaterial* SubUVMaterial = GetOrCreateSubUVMaterial(BuildContext, SubUVComponent);
		if (!SubUVMaterial && BuildContext.SubUVFeature)
		{
			SubUVMaterial = BuildContext.SubUVFeature->GetBaseMaterial();
		}
		if (!SubUVMaterial)
		{
			continue;
		}

		FRenderCommand Command;
		Command.RenderMesh = SubUVMesh;
		Command.Material = SubUVMaterial;
		Command.WorldMatrix = SubUVComponent->GetWorldTransform();

		if (SubUVComponent->IsBillboard())
		{
			const FVector WorldPosition = Command.WorldMatrix.GetTranslation();
			const FVector Scale = Command.WorldMatrix.GetScaleVector();
			Command.WorldMatrix = FMatrix::MakeScale(Scale) * FMatrix::MakeBillboard(WorldPosition, CameraPosition);
		}

		OutQueue.AddCommand(Command);
		ActiveSubUVComponents.push_back(SubUVComponent);
	}

	PruneStaleSubUVMaterials(ActiveSubUVComponents);

	TArray<const UBillboardComponent*> ActiveBillboardComponents;
	ActiveBillboardComponents.reserve(Packet.BillboardPrimitives.size());

	for (const FSceneBillboardPrimitive& Primitive : Packet.BillboardPrimitives)
	{
		UBillboardComponent* BillboardComponent = Primitive.Component;
		if (!BillboardComponent)
		{
			continue;
		}

		FRenderMesh* BillboardMesh = BillboardComponent->GetBillboardMesh();
		if (!BillboardMesh || !BuildContext.BillboardFeature)
		{
			continue;
		}

		if (!BuildContext.BillboardFeature->BuildMesh(BillboardComponent->GetSize(), *BillboardMesh))
		{
			continue;
		}

		BillboardMesh->bIsDirty = true;

		FMaterial* BillboardMaterial = BuildContext.BillboardFeature->GetOrCreateMaterial(*BillboardComponent);
		if (!BillboardMaterial)
		{
			continue;
		}

		FRenderCommand Command;
		Command.RenderMesh = BillboardMesh;
		Command.Material = BillboardMaterial;

		const FVector WorldPosition = BillboardComponent->GetWorldTransform().GetTranslation();
		const FVector Scale = BillboardComponent->GetWorldTransform().GetScaleVector();
		Command.WorldMatrix = FMatrix::MakeScale(Scale) * FMatrix::MakeBillboard(WorldPosition, CameraPosition);

		OutQueue.AddCommand(Command);
		ActiveBillboardComponents.push_back(BillboardComponent);
	}

	if (BuildContext.BillboardFeature)
	{
		BuildContext.BillboardFeature->PruneMaterials(ActiveBillboardComponents);
	}
}

