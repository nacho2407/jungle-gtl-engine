#include "Render/Proxy/ParticleDynamicEmitterData.h"

#include "Particles/ParticleSystem.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Render/Types/FrameContext.h"

#include <algorithm>
#include <cmath>

namespace
{
	template<typename PayloadType>
	const PayloadType* GetParticlePayloadBySlot(const FParticleEmitterInstance* Instance, uint16 ParticleSlot)
	{
		if (!Instance || !Instance->ParticleData || Instance->PayloadOffset < 0)
		{
			return nullptr;
		}

		const int32 PayloadEnd = Instance->PayloadOffset + static_cast<int32>(sizeof(PayloadType));
		if (PayloadEnd > Instance->ParticleStride)
		{
			return nullptr;
		}

		return reinterpret_cast<const PayloadType*>(
			Instance->ParticleData + ParticleSlot * Instance->ParticleStride + Instance->PayloadOffset);
	}

	float GetViewDepth(const FBaseParticle& Particle, const FFrameContext& Frame)
	{
		return (Particle.Position - Frame.CameraPosition).Dot(Frame.CameraForward);
	}

	const UParticleModuleTypeDataRibbon* GetRibbonTypeData(const FParticleEmitterInstance* Instance)
	{
		if (!Instance)
		{
			return nullptr;
		}

		const UParticleLODLevel* LODLevel = Instance->GetCurrentLODLevel();
		if (!LODLevel)
		{
			return nullptr;
		}

		return Cast<UParticleModuleTypeDataRibbon>(LODLevel->GetTypeDataModule());
	}

	const UParticleModuleTypeDataBeam* GetBeamTypeData(const FParticleEmitterInstance* Instance)
	{
		if (!Instance)
		{
			return nullptr;
		}
		const UParticleLODLevel* LODLevel = Instance->GetCurrentLODLevel();
		if (!LODLevel)
		{
			return nullptr;
		}
		return Cast<UParticleModuleTypeDataBeam>(LODLevel->GetTypeDataModule());
	}

	FVector ApplyBeamNoise(const FBeamParticlePayload& Payload, const FFrameContext& Frame, const FVector& Position, float T)
	{
		if (T <= 0.0f || T >= 1.0f || Payload.NoiseFrequency <= 0.0f || Payload.NoiseRange.LengthSquared() <= 0.0001f)
		{
			return Position;
		}

		constexpr float TwoPi = FMath::Pi * 2.0f;
		const float Envelope = std::sin(T * FMath::Pi);
		const float Phase = (T * Payload.NoiseFrequency + Payload.NoisePhase) * TwoPi;
		const float Phase2 = Phase * 1.37f + 1.618f;
		const float Phase3 = Phase * 0.73f + 2.414f;
		const FVector Offset =
			Frame.CameraRight * (std::sin(Phase) * Payload.NoiseRange.X) +
			Frame.CameraUp * (std::cos(Phase2) * Payload.NoiseRange.Y) +
			Frame.CameraForward * (std::sin(Phase3) * Payload.NoiseRange.Z);
		return Position + Offset * Envelope;
	}

	void GetParticleSubUVs(const FBaseParticle& Particle, int32 Columns, int32 Rows,
		FVector2& OutTopLeftUV, FVector2& OutTopRightUV, FVector2& OutBottomLeftUV, FVector2& OutBottomRightUV)
	{
		Columns = (std::max)(Columns, 1);
		Rows = (std::max)(Rows, 1);

		if (Columns == 1 && Rows == 1)
		{
			OutTopLeftUV = FVector2(0.0f, 0.0f);
			OutTopRightUV = FVector2(1.0f, 0.0f);
			OutBottomLeftUV = FVector2(0.0f, 1.0f);
			OutBottomRightUV = FVector2(1.0f, 1.0f);
			return;
		}

		const int32 TotalFrames = Columns * Rows;
		int32 FrameIndex = static_cast<int32>(std::floor(Particle.SubImageIndex));
		FrameIndex = (std::max)(0, (std::min)(FrameIndex, TotalFrames - 1));

		const int32 Column = FrameIndex % Columns;
		const int32 Row = FrameIndex / Columns;
		const float FrameWidth = 1.0f / static_cast<float>(Columns);
		const float FrameHeight = 1.0f / static_cast<float>(Rows);
		const float U0 = static_cast<float>(Column) * FrameWidth;
		const float V0 = static_cast<float>(Row) * FrameHeight;
		const float U1 = U0 + FrameWidth;
		const float V1 = V0 + FrameHeight;

		OutTopLeftUV = FVector2(U0, V0);
		OutTopRightUV = FVector2(U1, V0);
		OutBottomLeftUV = FVector2(U0, V1);
		OutBottomRightUV = FVector2(U1, V1);
	}
}

EParticleRenderType GetEmitterRenderType(const FParticleEmitterInstance* Instance)
{
	const UParticleLODLevel* LODLevel = Instance ? Instance->GetCurrentLODLevel() : nullptr;
	const UParticleModuleTypeDataBase* TypeData = LODLevel ? LODLevel->GetTypeDataModule() : nullptr;
	return TypeData ? TypeData->GetRenderType() : EParticleRenderType::Sprite;
}

FDynamicEmitterDataBase::FDynamicEmitterDataBase(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource)
	: EmitterIndex(InEmitterIndex)
	, Source(InSource)
{
}

const FDynamicEmitterReplayDataBase& FDynamicEmitterDataBase::GetSource() const
{
	return Source;
}

FDynamicSpriteEmitterDataBase::FDynamicSpriteEmitterDataBase(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource, bool bInDepthSort)
	: FDynamicEmitterDataBase(InEmitterIndex, InSource)
	, bDepthSort(bInDepthSort)
{
}

void FDynamicSpriteEmitterDataBase::GatherAliveParticleIndices(TArray<uint16>& OutRenderOrder) const
{
	OutRenderOrder.clear();

	const FParticleEmitterInstance* Instance = Source.Instance;
	if (!Instance || !Instance->ParticleIndices) return;

	const int32 ActiveCount = Instance->GetActiveParticleCount();
	const FParticleDataContainer& Data = Instance->GetParticleDataContainer();
	OutRenderOrder.reserve(ActiveCount);

	for (int32 Index = 0; Index < ActiveCount; ++Index)
	{
		const uint16 ParticleSlot = Instance->ParticleIndices[Index];
		if (Data.GetParticle(ParticleSlot).bAlive)
		{
			OutRenderOrder.push_back(ParticleSlot);
		}
	}
}

void FDynamicSpriteEmitterDataBase::SortParticleRenderOrder(const FFrameContext& Frame, TArray<uint16>& RenderOrder) const
{
	if (!bDepthSort || RenderOrder.size() < 2) return;

	const FParticleDataContainer& Data = Source.Instance->GetParticleDataContainer();
	std::sort(RenderOrder.begin(), RenderOrder.end(),
		[&Data, &Frame](uint16 A, uint16 B)
		{
			const FBaseParticle& ParticleA = Data.GetParticle(A);
			const FBaseParticle& ParticleB = Data.GetParticle(B);
			const float DepthA = GetViewDepth(ParticleA, Frame);
			const float DepthB = GetViewDepth(ParticleB, Frame);
			return DepthA > DepthB;
		});
}

uint32 FDynamicSpriteEmitterDataBase::BuildDynamicVertexData(const FFrameContext& Frame, FSpriteParticleGeometry& OutGeometry) const
{
	if (!Source.Instance) return 0;

	TArray<uint16> RenderOrder;
	GatherAliveParticleIndices(RenderOrder);
	SortParticleRenderOrder(Frame, RenderOrder);

	const FParticleDataContainer& Data = Source.Instance->GetParticleDataContainer();
	const uint32 FirstIndex = OutGeometry.GetIndexCount();
	const UParticleModuleRequired* RequiredModule = Source.Instance->GetRequiredModule();
	const int32 SubImagesHorizontal = RequiredModule ? RequiredModule->SubImagesHorizontal : 1;
	const int32 SubImagesVertical = RequiredModule ? RequiredModule->SubImagesVertical : 1;
	for (uint16 ParticleSlot : RenderOrder)
	{
		const FBaseParticle& Particle = Data.GetParticle(ParticleSlot);
		FVector2 TopLeftUV;
		FVector2 TopRightUV;
		FVector2 BottomLeftUV;
		FVector2 BottomRightUV;
		GetParticleSubUVs(Particle, SubImagesHorizontal, SubImagesVertical, TopLeftUV, TopRightUV, BottomLeftUV, BottomRightUV);
		OutGeometry.AddParticleQuad(Particle, Frame.CameraRight, Frame.CameraUp, TopLeftUV, TopRightUV, BottomLeftUV, BottomRightUV);
	}

	return OutGeometry.GetIndexCount() - FirstIndex;
}

FDynamicSpriteEmitterData::FDynamicSpriteEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource, bool bInDepthSort)
	: FDynamicSpriteEmitterDataBase(InEmitterIndex, InSource, bInDepthSort)
{
}

int32 FDynamicSpriteEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FParticleSpriteVertex);
}

FDynamicRibbonEmitterData::FDynamicRibbonEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource)
	: FDynamicSpriteEmitterDataBase(InEmitterIndex, InSource, false)
{
}

int32 FDynamicRibbonEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FParticleSpriteVertex);
}

void FDynamicRibbonEmitterData::SortParticleRenderOrder(const FFrameContext& Frame, TArray<uint16>& RenderOrder) const
{
	(void)Frame;
	if (!Source.Instance || RenderOrder.size() < 2) return;

	const FParticleDataContainer& Data = Source.Instance->GetParticleDataContainer();
	std::sort(RenderOrder.begin(), RenderOrder.end(),
		[this, &Data](uint16 A, uint16 B)
		{
			const FRibbonParticlePayload* PayloadA = GetParticlePayloadBySlot<FRibbonParticlePayload>(Source.Instance, A);
			const FRibbonParticlePayload* PayloadB = GetParticlePayloadBySlot<FRibbonParticlePayload>(Source.Instance, B);
			const uint16 RibbonIdA = PayloadA ? PayloadA->RibbonId : 0;
			const uint16 RibbonIdB = PayloadB ? PayloadB->RibbonId : 0;
			if (RibbonIdA != RibbonIdB)
			{
				return RibbonIdA < RibbonIdB;
			}

			const FBaseParticle& ParticleA = Data.GetParticle(A);
			const FBaseParticle& ParticleB = Data.GetParticle(B);
			if (ParticleA.Age != ParticleB.Age)
			{
				return ParticleA.Age > ParticleB.Age;
			}

			return ParticleA.FrameIndex < ParticleB.FrameIndex;
		});
}

uint32 FDynamicRibbonEmitterData::BuildDynamicVertexData(const FFrameContext& Frame, FSpriteParticleGeometry& OutGeometry) const
{
	if (!Source.Instance) return 0;

	TArray<uint16> RenderOrder;
	GatherAliveParticleIndices(RenderOrder);
	SortParticleRenderOrder(Frame, RenderOrder);

	if (RenderOrder.size() < 2) return 0;

	const uint32 FirstIndex = OutGeometry.GetIndexCount();
	const FParticleDataContainer& Data = Source.Instance->GetParticleDataContainer();

	struct FRibbonPoint
	{
		uint16 Slot;
		const FBaseParticle* Particle = nullptr;
		const FRibbonParticlePayload* Payload = nullptr;
		float U = 0.0f;
	};

	auto FlushRibbonPoints = [&](const TArray<FRibbonPoint>& Points)
	{
		const int32 PointCount = static_cast<int32>(Points.size());
		if (PointCount < 2)
		{
			return;
		}

		TArray<uint32> LeftIndices;
		TArray<uint32> RightIndices;
		LeftIndices.reserve(PointCount);
		RightIndices.reserve(PointCount);

		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			const FRibbonPoint& Point = Points[PointIndex];
			if (!Point.Particle || !Point.Payload)
			{
				continue;
			}

			FVector Tangent;
			if (PointIndex == 0)
			{
				Tangent = Points[1].Particle->Position - Points[0].Particle->Position;
			}
			else if (PointIndex == PointCount - 1)
			{
				Tangent = Points[PointIndex].Particle->Position - Points[PointIndex - 1].Particle->Position;
			}
			else
			{
				Tangent = Points[PointIndex + 1].Particle->Position - Points[PointIndex - 1].Particle->Position;
			}

			if (Tangent.LengthSquared() <= 0.0001f)
			{
				continue;
			}

			Tangent.Normalize();

			FVector Side = Frame.CameraForward.Cross(Tangent);
			if (Side.LengthSquared() <= 0.0001f)
			{
				Side = Frame.CameraRight;
			}
			else
			{
				Side.Normalize();
			}

			const FVector Position = Point.Particle->Position;
			const FVector HalfWidth = Side * (Point.Payload->Width * 0.5f);
			const FVector4 Color = Point.Particle->Color;

			const uint32 LeftIndex = OutGeometry.AddVertex({
				Position - HalfWidth,
				Color,
				FVector2(Point.U, 0.0f)
				});

			const uint32 RightIndex = OutGeometry.AddVertex({
				Position + HalfWidth,
				Color,
				FVector2(Point.U, 1.0f)
				});

			LeftIndices.push_back(LeftIndex);
			RightIndices.push_back(RightIndex);
		}

		const int32 VertexPointCount = static_cast<int32>(LeftIndices.size());
		if (VertexPointCount < 2)
		{
			return;
		}

		for (int32 PointIndex = 0; PointIndex + 1 < VertexPointCount; ++PointIndex)
		{
			const uint32 Left0 = LeftIndices[PointIndex];
			const uint32 Right0 = RightIndices[PointIndex];
			const uint32 Left1 = LeftIndices[PointIndex + 1];
			const uint32 Right1 = RightIndices[PointIndex + 1];

			OutGeometry.AddTriangle(Left0, Right0, Left1);
			OutGeometry.AddTriangle(Right0, Right1, Left1);
		}
	};

	TArray<FRibbonPoint> Points;
	Points.reserve(RenderOrder.size());

	uint16 CurrentRibbonId = 0;
	bool bHasCurrentRibbon = false;
	float AccumulatedDistance = 0.0f;
	FVector PreviousPosition = FVector::ZeroVector;

	const UParticleModuleTypeDataRibbon* RibbonTypeData = GetRibbonTypeData(Source.Instance);
	const float TileDistance = RibbonTypeData ? std::max(RibbonTypeData->TextureTileDistance, 1.0f) : 100.0f;

	for (uint16 ParticleSlot : RenderOrder)
	{
		const FRibbonParticlePayload* Payload = GetParticlePayloadBySlot<FRibbonParticlePayload>(Source.Instance, ParticleSlot);
		if (!Payload)
		{
			continue;
		}

		const FBaseParticle& Particle = Data.GetParticle(ParticleSlot);
		const uint16 RibbonId = Payload->RibbonId;

		if (!bHasCurrentRibbon || RibbonId != CurrentRibbonId)
		{
			FlushRibbonPoints(Points);
			Points.clear();

			CurrentRibbonId = RibbonId;
			bHasCurrentRibbon = true;
			AccumulatedDistance = 0.0f;
			PreviousPosition = Particle.Position;
		}
		else
		{
			AccumulatedDistance += FVector::Distance(PreviousPosition, Particle.Position);
			PreviousPosition = Particle.Position;
		}

		FRibbonPoint Point;
		Point.Slot = ParticleSlot;
		Point.Particle = &Particle;
		Point.Payload = Payload;
		Point.U = AccumulatedDistance / TileDistance;
		Points.push_back(Point);
	}

	FlushRibbonPoints(Points);

	return OutGeometry.GetIndexCount() - FirstIndex;
}

FDynamicBeamEmitterData::FDynamicBeamEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource)
	: FDynamicSpriteEmitterDataBase(InEmitterIndex, InSource, false)
{
}

int32 FDynamicBeamEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FParticleSpriteVertex);
}

void FDynamicBeamEmitterData::SortParticleRenderOrder(const FFrameContext& Frame, TArray<uint16>& RenderOrder) const
{
	(void)Frame;
	if (!Source.Instance || RenderOrder.size() < 2) return;

	const FParticleDataContainer& Data = Source.Instance->GetParticleDataContainer();
	std::sort(RenderOrder.begin(), RenderOrder.end(),
		[this, &Data](uint16 A, uint16 B)
		{
			const FBeamParticlePayload* PayloadA = GetParticlePayloadBySlot<FBeamParticlePayload>(Source.Instance, A);
			const FBeamParticlePayload* PayloadB = GetParticlePayloadBySlot<FBeamParticlePayload>(Source.Instance, B);
			const uint16 BeamIndexA = PayloadA ? PayloadA->BeamIndex : 0;
			const uint16 BeamIndexB = PayloadB ? PayloadB->BeamIndex : 0;
			if (BeamIndexA != BeamIndexB)
			{
				return BeamIndexA < BeamIndexB;
			}

			return Data.GetParticle(A).FrameIndex < Data.GetParticle(B).FrameIndex;
		});
}

uint32 FDynamicBeamEmitterData::BuildDynamicVertexData(const FFrameContext& Frame, FSpriteParticleGeometry& OutGeometry) const
{
	if (!Source.Instance) return 0;
	
	TArray<uint16> RenderOrder;
	GatherAliveParticleIndices(RenderOrder);
	SortParticleRenderOrder(Frame, RenderOrder);

	if (RenderOrder.empty()) return 0;

	const uint32 FirstIndex = OutGeometry.GetIndexCount();
	const FParticleDataContainer& Data = Source.Instance->GetParticleDataContainer();

	const UParticleModuleTypeDataBeam* BeamTypeData = GetBeamTypeData(Source.Instance);
	const int32 InterpolationPoints = BeamTypeData ? std::max(BeamTypeData->InterpolationPoints, 0) : 0;

	const float TextureTileDistance = BeamTypeData
		? (std::max)(BeamTypeData->TextureTileDistance, 1.0f)
		: 100.0f;

	auto EvalHermite = [](const FVector& P0, const FVector& P1, const FVector& T0, const FVector& T1, float T)
	{
		const float T2 = T * T;
		const float T3 = T2 * T;

		const float H00 = 2.0f * T3 - 3.0f * T2 + 1.0f;
		const float H10 = T3 - 2.0f * T2 + T;
		const float H01 = -2.0f * T3 + 3.0f * T2;
		const float H11 = T3 - T2;

		return P0 * H00 + T0 * H10 + P1 * H01 + T1 * H11;
	};

	for (uint16 ParticleSlot : RenderOrder)
	{
		const FBeamParticlePayload* Payload = GetParticlePayloadBySlot<FBeamParticlePayload>(Source.Instance, ParticleSlot);
		if (!Payload) continue;

		const FBaseParticle& Particle = Data.GetParticle(ParticleSlot);

		const int32 PointCount = std::max(2, InterpolationPoints + 2);

		TArray<FVector> BeamPoints;
		BeamPoints.reserve(PointCount);

		const FVector BeamDirection = (Payload->TargetPoint - Payload->SourcePoint).Normalized();
		const FVector SourceTangent = Payload->SourceTangent;
		const FVector TargetTangent = BeamDirection * Payload->TargetStrength;

		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			const float T = PointCount > 1
				? static_cast<float>(PointIndex) / static_cast<float>(PointCount - 1)
				: 0.0f;

			const FVector Position = EvalHermite(Payload->SourcePoint, Payload->TargetPoint, SourceTangent, TargetTangent, T);
			BeamPoints.push_back(ApplyBeamNoise(*Payload, Frame, Position, T));
		}

		TArray<uint32> LeftIndices;
		TArray<uint32> RightIndices;
		LeftIndices.reserve(PointCount);
		RightIndices.reserve(PointCount);

		float AccumulatedDistance = 0.0f;
		FVector PreviousPosition = BeamPoints[0];

		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			const FVector Position = BeamPoints[PointIndex];
			
			if (PointIndex > 0)
			{
				AccumulatedDistance += (Position - PreviousPosition).Length();
				PreviousPosition = Position;
			}

			FVector Tangent;
			if (PointIndex == 0)
			{
				Tangent = BeamPoints[1] - BeamPoints[0];
			}
			else if (PointIndex == PointCount - 1)
			{
				Tangent = BeamPoints[PointIndex] - BeamPoints[PointIndex - 1];
			}
			else
			{
				Tangent = BeamPoints[PointIndex + 1] - BeamPoints[PointIndex - 1];
			}

			if (Tangent.LengthSquared() <= 0.0001f) continue;

			Tangent.Normalize();

			FVector Side = Frame.CameraForward.Cross(Tangent);
			if (Side.LengthSquared() <= 0.0001f)
			{
				Side = Frame.CameraRight;
			}
			else
			{
				Side.Normalize();
			}

			const float Width = Payload->Width;
			const FVector HalfWidth = Side * (Width * 0.5f);
			const float U = AccumulatedDistance / TextureTileDistance;

			LeftIndices.push_back(OutGeometry.AddVertex({
				Position - HalfWidth,
				Particle.Color,
				FVector2(U, 0.0f)
			}));

			RightIndices.push_back(OutGeometry.AddVertex({
				Position + HalfWidth,
				Particle.Color,
				FVector2(U, 1.0f)
			}));
		}

		const int32 VertexPointCount = static_cast<int32>(LeftIndices.size());
		for (int32 PointIndex = 0; PointIndex + 1 < VertexPointCount; ++PointIndex)
		{
			const uint32 Left0 = LeftIndices[PointIndex];
			const uint32 Right0 = RightIndices[PointIndex];
			const uint32 Left1 = LeftIndices[PointIndex + 1];
			const uint32 Right1 = RightIndices[PointIndex + 1];

			OutGeometry.AddTriangle(Left0, Right0, Left1);
			OutGeometry.AddTriangle(Right0, Right1, Left1);
		}
	}

	return OutGeometry.GetIndexCount() - FirstIndex;
}

FDynamicMeshEmitterData::FDynamicMeshEmitterData(int32 InEmitterIndex, const FDynamicEmitterReplayDataBase& InSource, FParticleMeshEmitterInstance* InMeshInstance)
	: FDynamicEmitterDataBase(InEmitterIndex, InSource)
	, MeshInstance(InMeshInstance)
{
}

int32 FDynamicMeshEmitterData::GetDynamicVertexStride() const
{
	return sizeof(FMeshParticleInstanceData);
}

uint32 FDynamicMeshEmitterData::BuildDynamicVertexData(const FFrameContext& Frame, TArray<FMeshParticleInstanceData>& OutInstances, bool bSortByViewDistance) const
{
	if (!Source.Instance || !Source.Instance->ParticleIndices || !MeshInstance) return 0;

	const uint32 FirstInstance = static_cast<uint32>(OutInstances.size());
	const int32 ActiveCount = Source.Instance->GetActiveParticleCount();
	const FParticleDataContainer& Data = Source.Instance->GetParticleDataContainer();

	TArray<int32> RenderOrder;
	RenderOrder.reserve(ActiveCount);
	for (int32 ParticleIndex = 0; ParticleIndex < ActiveCount; ++ParticleIndex)
	{
		const uint16 ParticleSlot = Source.Instance->ParticleIndices[ParticleIndex];
		if (Data.GetParticle(ParticleSlot).bAlive)
		{
			RenderOrder.push_back(ParticleIndex);
		}
	}

	if (bSortByViewDistance && RenderOrder.size() > 1)
	{
		std::sort(RenderOrder.begin(), RenderOrder.end(),
			[this, &Data, &Frame](int32 A, int32 B)
			{
				const FBaseParticle& ParticleA = Data.GetParticle(Source.Instance->ParticleIndices[A]);
				const FBaseParticle& ParticleB = Data.GetParticle(Source.Instance->ParticleIndices[B]);
				return GetViewDepth(ParticleA, Frame) > GetViewDepth(ParticleB, Frame);
			});
	}

	for (int32 ParticleIndex : RenderOrder)
	{
		const FBaseParticle& Particle = Data.GetParticle(Source.Instance->ParticleIndices[ParticleIndex]);

		FMeshParticleTransform Transform;
		Transform.Position = Particle.Position;
		Transform.Scale = Particle.Size;
		Transform.RotationEuler = FVector(0, 0, Particle.Rotation);

		if (const FParticleMeshPayload* Payload = MeshInstance->GetParticlePayload<FParticleMeshPayload>(ParticleIndex))
		{
			Transform.Scale = FVector(
				Transform.Scale.X * Payload->MeshScale.X,
				Transform.Scale.Y * Payload->MeshScale.Y,
				Transform.Scale.Z * Payload->MeshScale.Z);
			Transform.RotationEuler += Payload->MeshRotation;
		}

		OutInstances.push_back(MakeMeshParticleInstanceData(Transform, Particle.Color));
	}

	return static_cast<uint32>(OutInstances.size()) - FirstInstance;
}
