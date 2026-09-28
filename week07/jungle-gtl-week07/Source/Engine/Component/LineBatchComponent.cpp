#include "LineBatchComponent.h"
#include "Core/ResourceManager.h"

ULineBatchComponent::ULineBatchComponent()
{
	UMaterial* Mat = FResourceManager::Get().GetOrCreateMaterial("LineMaterial", "Shaders/ShaderLine.hlsl");
	Mat->DepthStencilType = EDepthStencilType::Default;
	Mat->BlendType = EBlendType::AlphaBlend;
	Mat->RasterizerType = ERasterizerType::SolidBackCull;

	Material = Mat;
}

void ULineBatchComponent::AddLine(const FVector& Start, const FVector& End, const FColor& Color)
{
	LineVertices.emplace_back(Start, Color);
	LineVertices.emplace_back(End, Color);
	const uint32 StartIndex = static_cast<uint32>(LineVertices.size() - 2);
	Indices.push_back(StartIndex);
	Indices.push_back(StartIndex + 1);
}

void ULineBatchComponent::AddAABB(const FAABB& Box, const FColor& Color)
{
	const FVector Min = Box.Min;
	const FVector Max = Box.Max;

	FVector Vertices[8] = {
		{Min.X, Min.Y, Min.Z},
		{Max.X, Min.Y, Min.Z},
		{Max.X, Max.Y, Min.Z},
		{Min.X, Max.Y, Min.Z},
		{Min.X, Min.Y, Max.Z},
		{Max.X, Min.Y, Max.Z},
		{Max.X, Max.Y, Max.Z},
		{Min.X, Max.Y, Max.Z}
	};
	uint32 Indices[24] = {
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	};
	for (const auto& Index : Indices)
	{
		AddLine(Vertices[Index], Vertices[Index ^ 1], Color);
	}
}

void ULineBatchComponent::AddOBB(const FOBB& Box, const FColor& Color)
{
	FAABB AABB(Box.Center - Box.Extents, Box.Center + Box.Extents);
	FMatrix WorldMatrix = FMatrix::Identity;
	WorldMatrix.MakeRotationEuler(Box.Rotation.Euler());
	WorldMatrix.SetTranslation(Box.Center);

	FVector TransformedVertices[8];
	for (int i = 0; i < 8; ++i)
	{
		FVector LocalVertex = (i & 1) ? FVector(AABB.Max.X, AABB.Min.Y, AABB.Min.Z) : FVector(AABB.Min.X, AABB.Min.Y, AABB.Min.Z);
		if (i & 2) LocalVertex.Y = AABB.Max.Y;
		if (i & 4) LocalVertex.Z = AABB.Max.Z;
		TransformedVertices[i] = WorldMatrix.TransformPosition(LocalVertex);
	}
	uint32 Indices[24] = {
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	};

	for (const auto& Index : Indices)
	{
		AddLine(TransformedVertices[Index], TransformedVertices[Index ^ 1], Color);
	}
}
