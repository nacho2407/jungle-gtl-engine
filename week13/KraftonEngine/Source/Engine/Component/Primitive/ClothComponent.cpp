#include "Component/Primitive/ClothComponent.h"

#include "Engine/Runtime/Engine.h"
#include "Materials/MaterialManager.h"
#include "Object/GarbageCollection.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Proxy/ClothSceneProxy.h"
#include "Core/Logging/Log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	constexpr int32 GMinClothParticlesPerAxis = 2;
	constexpr int32 GMaxClothParticlesPerAxis = 256;
	constexpr float GDefaultParticleSpacing = 10.0f;
	constexpr float GNormalTolerance = 1.0e-6f;
	constexpr float GTangentTolerance = 1.0e-6f;

	/**
	 * @brief 정수 값을 지정된 범위 안으로 보정합니다
	 *
	 * @param Value 보정할 정수 값
	 *
	 * @param MinValue 허용 최소값
	 *
	 * @param MaxValue 허용 최대값
	 *
	 * @return 보정된 정수 값
	 */
	int32 ClampInt(int32 Value, int32 MinValue, int32 MaxValue)
	{
		return (std::max)(MinValue, (std::min)(Value, MaxValue));
	}

	/**
	 * @brief 유한한 양수 spacing 값을 반환합니다
	 *
	 * @param Value 보정할 spacing 값
	 *
	 * @return 보정된 spacing 값
	 */
	float SanitizeSpacing(float Value)
	{
		if (!std::isfinite(Value) || Value <= 0.0f)
		{
			return GDefaultParticleSpacing;
		}

		return Value;
	}
}

UClothComponent::UClothComponent()
{
	PrimaryComponentTick.SetTickGroup(TG_PostPhysics);
	PrimaryComponentTick.SetEndTickGroup(TG_PostPhysics);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEnabled = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	bClothRebuildDirty = true;
}

FPrimitiveSceneProxy* UClothComponent::CreateSceneProxy()
{
	// editor viewport에서 tick 전에 proxy가 만들어지는 초기 표시 경로 보장
	RebuildClothIfNeeded();
	return new FClothSceneProxy(this);
}

void UClothComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMeshComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	LogBackendStatusOnce();
	RebuildClothIfNeeded();
}

void UClothComponent::PostEditProperty(const char* PropertyName)
{
	UMeshComponent::PostEditProperty(PropertyName);

	if (!PropertyName)
	{
		return;
	}

	if (std::strcmp(PropertyName, "NumParticlesX") == 0
		|| std::strcmp(PropertyName, "Num Particles X") == 0
		|| std::strcmp(PropertyName, "NumParticlesY") == 0
		|| std::strcmp(PropertyName, "Num Particles Y") == 0
		|| std::strcmp(PropertyName, "ParticleSpacing") == 0
		|| std::strcmp(PropertyName, "Particle Spacing") == 0)
	{
		// 에디터 입력값 보정과 rebuild 병합
		const FClothConfig Config = MakeClothConfig();
		NumParticlesX = Config.NumParticlesX;
		NumParticlesY = Config.NumParticlesY;
		ParticleSpacing = Config.ParticleSpacing;
		MarkClothRebuildDirty();
		return;
	}

	if (std::strcmp(PropertyName, "MaterialSlot") == 0 || std::strcmp(PropertyName, "Material") == 0)
	{
		LoadMaterialFromSlot();
		MarkProxyDirty(EDirtyFlag::Material);
	}
}

void UClothComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	LoadMaterialFromSlot();
	MarkClothRebuildDirty();
}

void UClothComponent::AddReferencedObjects(FReferenceCollector& Collector)
{
	UMeshComponent::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(Material, "UClothComponent.Material");
}

FMeshDataView UClothComponent::GetMeshDataView() const
{
	if (!RenderData.IsValid())
	{
		return {};
	}

	FMeshDataView View;
	View.VertexData = RenderData.Vertices.data();
	View.VertexCount = static_cast<uint32>(RenderData.Vertices.size());
	View.Stride = sizeof(FVertexPNCTT);
	View.IndexData = RenderData.Indices.data();
	View.IndexCount = static_cast<uint32>(RenderData.Indices.size());
	return View;
}

void UClothComponent::UpdateWorldAABB() const
{
	if (!bHasValidLocalBounds)
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	const FMatrix& WorldMatrix = GetWorldMatrix();
	const FVector WorldCenter = WorldMatrix.TransformPositionWithW(CachedLocalCenter);

	const float Ex = std::abs(WorldMatrix.M[0][0]) * CachedLocalExtent.X
		+ std::abs(WorldMatrix.M[1][0]) * CachedLocalExtent.Y
		+ std::abs(WorldMatrix.M[2][0]) * CachedLocalExtent.Z;
	const float Ey = std::abs(WorldMatrix.M[0][1]) * CachedLocalExtent.X
		+ std::abs(WorldMatrix.M[1][1]) * CachedLocalExtent.Y
		+ std::abs(WorldMatrix.M[2][1]) * CachedLocalExtent.Z;
	const float Ez = std::abs(WorldMatrix.M[0][2]) * CachedLocalExtent.X
		+ std::abs(WorldMatrix.M[1][2]) * CachedLocalExtent.Y
		+ std::abs(WorldMatrix.M[2][2]) * CachedLocalExtent.Z;

	WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
	WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void UClothComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
	if (ElementIndex != 0)
	{
		return;
	}

	Material = InMaterial;
	MaterialSlot = Material ? Material->GetAssetPathFileName() : "None";

	// 단일 material slot만 바뀌므로 geometry rebuild 없이 material dirty만 전파
	MarkProxyDirty(EDirtyFlag::Material);
}

UMaterial* UClothComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex != 0)
	{
		return nullptr;
	}

	return Material.Get();
}

void UClothComponent::MarkClothRebuildDirty()
{
	bClothRebuildDirty = true;
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UClothComponent::RebuildClothIfNeeded()
{
	if (!bClothRebuildDirty)
	{
		return;
	}

	BuildGrid(MakeClothConfig());
}

FClothConfig UClothComponent::MakeClothConfig() const
{
	FClothConfig Config;
	Config.NumParticlesX = ClampInt(NumParticlesX, GMinClothParticlesPerAxis, GMaxClothParticlesPerAxis);
	Config.NumParticlesY = ClampInt(NumParticlesY, GMinClothParticlesPerAxis, GMaxClothParticlesPerAxis);
	Config.ParticleSpacing = SanitizeSpacing(ParticleSpacing);
	return Config;
}

void UClothComponent::BuildGrid(const FClothConfig& Config)
{
	// 현재 property에도 보정값 반영
	NumParticlesX = Config.NumParticlesX;
	NumParticlesY = Config.NumParticlesY;
	ParticleSpacing = Config.ParticleSpacing;

	RenderData.Vertices.clear();
	RenderData.Indices.clear();
	RenderData.Sections.clear();

	const uint32 NumX = static_cast<uint32>(Config.NumParticlesX);
	const uint32 NumY = static_cast<uint32>(Config.NumParticlesY);
	const uint32 VertexCount = NumX * NumY;
	const uint32 QuadCount = (NumX - 1) * (NumY - 1);

	RenderData.Vertices.resize(VertexCount);
	RenderData.Indices.reserve(static_cast<size_t>(QuadCount) * 6);

	const float Width = static_cast<float>(NumX - 1) * Config.ParticleSpacing;
	const float Height = static_cast<float>(NumY - 1) * Config.ParticleSpacing;
	const float MinX = -Width * 0.5f;
	const float MinZ = -Height * 0.5f;

	for (uint32 Row = 0; Row < NumY; ++Row)
	{
		for (uint32 Col = 0; Col < NumX; ++Col)
		{
			const uint32 VertexIndex = Row * NumX + Col;
			const float U = NumX > 1 ? static_cast<float>(Col) / static_cast<float>(NumX - 1) : 0.0f;
			const float V = NumY > 1 ? 1.0f - static_cast<float>(Row) / static_cast<float>(NumY - 1) : 0.0f;

			FVertexPNCTT& Vertex = RenderData.Vertices[VertexIndex];
			Vertex.Position = FVector(MinX + static_cast<float>(Col) * Config.ParticleSpacing, 0.0f, MinZ + static_cast<float>(Row) * Config.ParticleSpacing);
			Vertex.Normal = FVector::YAxisVector;
			Vertex.Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
			Vertex.UV = FVector2(U, V);
			Vertex.Tangent = FVector4(FVector::XAxisVector, 1.0f);
		}
	}

	for (uint32 Row = 0; Row + 1 < NumY; ++Row)
	{
		for (uint32 Col = 0; Col + 1 < NumX; ++Col)
		{
			const uint32 V00 = Row * NumX + Col;
			const uint32 V10 = Row * NumX + Col + 1;
			const uint32 V01 = (Row + 1) * NumX + Col;
			const uint32 V11 = (Row + 1) * NumX + Col + 1;

			// X-Z 평면에서 +Y가 front face가 되도록 winding 고정
			RenderData.Indices.push_back(V00);
			RenderData.Indices.push_back(V01);
			RenderData.Indices.push_back(V10);

			RenderData.Indices.push_back(V10);
			RenderData.Indices.push_back(V01);
			RenderData.Indices.push_back(V11);
		}
	}

	RecalculateNormalsAndTangents();
	UpdateRenderSections();
	UpdateLocalBoundsFromRenderData(Config);
	IncrementRenderRevision();

	bClothRebuildDirty = false;

	// local shape 변경 전파
	MarkWorldBoundsDirty();
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UClothComponent::RecalculateNormalsAndTangents()
{
	TArray<FVector> AccumulatedNormals;
	TArray<FVector> AccumulatedTangents;
	AccumulatedNormals.resize(RenderData.Vertices.size(), FVector::ZeroVector);
	AccumulatedTangents.resize(RenderData.Vertices.size(), FVector::ZeroVector);

	for (uint32 Index = 0; Index + 2 < RenderData.Indices.size(); Index += 3)
	{
		const uint32 I0 = RenderData.Indices[Index];
		const uint32 I1 = RenderData.Indices[Index + 1];
		const uint32 I2 = RenderData.Indices[Index + 2];

		if (I0 >= RenderData.Vertices.size() || I1 >= RenderData.Vertices.size() || I2 >= RenderData.Vertices.size())
		{
			continue;
		}

		const FVertexPNCTT& V0 = RenderData.Vertices[I0];
		const FVertexPNCTT& V1 = RenderData.Vertices[I1];
		const FVertexPNCTT& V2 = RenderData.Vertices[I2];

		const FVector Edge1 = V1.Position - V0.Position;
		const FVector Edge2 = V2.Position - V0.Position;
		const FVector TriangleNormal = Edge1.Cross(Edge2);

		if (!TriangleNormal.IsNearlyZero(GNormalTolerance))
		{
			AccumulatedNormals[I0] += TriangleNormal;
			AccumulatedNormals[I1] += TriangleNormal;
			AccumulatedNormals[I2] += TriangleNormal;
		}

		const FVector2 DeltaUV1 = V1.UV - V0.UV;
		const FVector2 DeltaUV2 = V2.UV - V0.UV;
		const float Determinant = DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X;
		FVector TriangleTangent = FVector::XAxisVector;

		if (std::abs(Determinant) > GTangentTolerance)
		{
			const float InvDeterminant = 1.0f / Determinant;
			TriangleTangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDeterminant;
		}

		if (!TriangleTangent.IsNearlyZero(GTangentTolerance))
		{
			AccumulatedTangents[I0] += TriangleTangent;
			AccumulatedTangents[I1] += TriangleTangent;
			AccumulatedTangents[I2] += TriangleTangent;
		}
	}

	for (uint32 VertexIndex = 0; VertexIndex < RenderData.Vertices.size(); ++VertexIndex)
	{
		FVector Normal = AccumulatedNormals[VertexIndex];
		if (Normal.IsNearlyZero(GNormalTolerance))
		{
			Normal = FVector::YAxisVector;
		}
		else
		{
			Normal.Normalize();
		}

		FVector Tangent = AccumulatedTangents[VertexIndex];
		if (Tangent.IsNearlyZero(GTangentTolerance))
		{
			Tangent = FVector::XAxisVector;
		}
		else
		{
			// normal 방향 성분 제거 후 tangent 정규화
			Tangent = Tangent - Normal * Tangent.Dot(Normal);
			if (Tangent.IsNearlyZero(GTangentTolerance))
			{
				Tangent = FVector::XAxisVector;
			}
			else
			{
				Tangent.Normalize();
			}
		}

		RenderData.Vertices[VertexIndex].Normal = Normal;
		RenderData.Vertices[VertexIndex].Tangent = FVector4(Tangent, 1.0f);
	}
}

void UClothComponent::UpdateRenderSections()
{
	FClothRenderSection Section;
	Section.FirstIndex = 0;
	Section.IndexCount = static_cast<uint32>(RenderData.Indices.size());
	Section.MaterialIndex = 0;
	RenderData.Sections.push_back(Section);
}

void UClothComponent::UpdateLocalBoundsFromRenderData(const FClothConfig& Config)
{
	bHasValidLocalBounds = false;

	if (RenderData.Vertices.empty())
	{
		CachedLocalCenter = FVector::ZeroVector;
		CachedLocalExtent = FVector(0.5f, 0.5f, 0.5f);
		return;
	}

	FVector Min = RenderData.Vertices[0].Position;
	FVector Max = RenderData.Vertices[0].Position;

	for (const FVertexPNCTT& Vertex : RenderData.Vertices)
	{
		Min.X = (std::min)(Min.X, Vertex.Position.X);
		Min.Y = (std::min)(Min.Y, Vertex.Position.Y);
		Min.Z = (std::min)(Min.Z, Vertex.Position.Z);

		Max.X = (std::max)(Max.X, Vertex.Position.X);
		Max.Y = (std::max)(Max.Y, Vertex.Position.Y);
		Max.Z = (std::max)(Max.Z, Vertex.Position.Z);
	}

	const FVector Margin(Config.BoundsMargin, Config.BoundsMargin, Config.BoundsMargin);
	Min -= Margin;
	Max += Margin;

	CachedLocalCenter = (Min + Max) * 0.5f;
	CachedLocalExtent = (Max - Min) * 0.5f;
	LocalExtents = CachedLocalExtent;
	bHasValidLocalBounds = true;
}

void UClothComponent::IncrementRenderRevision()
{
	++RenderData.Revision;
	if (RenderData.Revision == 0)
	{
		++RenderData.Revision;
	}
}

void UClothComponent::LoadMaterialFromSlot()
{
	if (MaterialSlot.empty() || MaterialSlot == "None")
	{
		SetMaterial(0, nullptr);
		return;
	}

	UMaterial* LoadedMaterial = FMaterialManager::Get().GetOrCreateMaterial(MaterialSlot);
	if (LoadedMaterial)
	{
		SetMaterial(0, LoadedMaterial);
	}
}

void UClothComponent::LogBackendStatusOnce()
{
	if (bBackendStatusLogged)
	{
		return;
	}

	if (!GEngine)
	{
		return;
	}

	bBackendStatusLogged = true;

	const FClothBackendStatus& Status = GEngine->GetClothContext().GetBackendStatus();
	UE_LOG("[ClothComponent] Backend=%s Available=%s Detail=%s",
		GetClothBackendName(Status.Backend),
		Status.bAvailable ? "true" : "false",
		Status.Detail.c_str());
}
