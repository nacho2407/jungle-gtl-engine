#include "DynamicEmitterData.h"
#include "Particles/ParticleHelper.h"
#include "Particles/TypeData/ParticleModuleTypeDataRibbon.h"
#include "Render/Types/FrameContext.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace
{
	constexpr float PI = 3.14159265358979323846f;

	struct FBeamTrailCPUStaging
	{
		TArray<FParticleBeamTrailVertex> Vertices;
		TArray<uint32> Indices;
	};

	std::unordered_map<const void*, FBeamTrailCPUStaging> GBeamTrailCPUStaging;

	FBeamTrailCPUStaging& GetCPUStaging(const void* Owner)
	{
		return GBeamTrailCPUStaging[Owner];
	}

	const FBeamTrailCPUStaging& GetCPUStagingConst(const void* Owner)
	{
		static const FBeamTrailCPUStaging Empty;
		const auto It = GBeamTrailCPUStaging.find(Owner);
		return It != GBeamTrailCPUStaging.end() ? It->second : Empty;
	}

	void RemoveCPUStaging(const void* Owner)
	{
		GBeamTrailCPUStaging.erase(Owner);
	}

	int32 ComputeBeamTaperCount(const FDynamicBeam2EmitterReplayData& Source, const FBeam2TypeDataPayload& BeamData)
	{
		if (Source.TaperMethod == 0)
		{
			return 0;
		}
		if (Source.bLowFreqNoise_Enabled)
		{
			const int32 NoiseTessellation = Source.NoiseTessellation ? Source.NoiseTessellation : 1;
			return (std::max(0, Source.Frequency) + 2) * NoiseTessellation;
		}
		if (Source.InterpolationPoints > 0)
		{
			return Source.InterpolationPoints + 1;
		}
		return std::max(2, BeamData.Steps + 1);
	}

	float Clamp01(float Value)
	{
		return std::max(0.0f, std::min(1.0f, Value));
	}

	const FBaseParticle* GetReplayParticle(const FDynamicEmitterReplayDataBase& Source, int32 DirectIndex)
	{
		if (!Source.DataContainer.ParticleData || DirectIndex < 0)
		{
			return nullptr;
		}
		return reinterpret_cast<const FBaseParticle*>(
			Source.DataContainer.ParticleData + static_cast<size_t>(DirectIndex) * Source.ParticleStride);
	}

	const FBaseParticle* GetReplayActiveParticle(const FDynamicEmitterReplayDataBase& Source, int32 ActiveIndex)
	{
		if (!Source.DataContainer.ParticleIndices || ActiveIndex < 0 || ActiveIndex >= Source.ActiveParticleCount)
		{
			return nullptr;
		}
		return GetReplayParticle(Source, Source.DataContainer.ParticleIndices[ActiveIndex]);
	}

	template <typename PayloadType>
	const PayloadType* GetReplayPayload(const FDynamicEmitterReplayDataBase& Source, const FBaseParticle* Particle, int32 Offset)
	{
		if (!Particle || Offset < 0)
		{
			return nullptr;
		}
		return reinterpret_cast<const PayloadType*>(reinterpret_cast<const uint8*>(Particle) + Offset);
	}

	FVector RotateAroundAxis(const FVector& Vector, const FVector& Axis, float Angle)
	{
		const FVector N = Axis.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
		const float C = std::cos(Angle);
		const float S = std::sin(Angle);
		return Vector * C + N.Cross(Vector) * S + N * (N.Dot(Vector) * (1.0f - C));
	}

	FVector CubicInterp(const FVector& P0, const FVector& T0, const FVector& P1, const FVector& T1, float Alpha)
	{
		const float A2 = Alpha * Alpha;
		const float A3 = A2 * Alpha;
		return P0 * (2.0f * A3 - 3.0f * A2 + 1.0f)
			+ T0 * (A3 - 2.0f * A2 + Alpha)
			+ P1 * (-2.0f * A3 + 3.0f * A2)
			+ T1 * (A3 - A2);
	}

	FLinearColor LerpColor(const FLinearColor& A, const FLinearColor& B, float Alpha)
	{
		return FLinearColor(
			A.R + (B.R - A.R) * Alpha,
			A.G + (B.G - A.G) * Alpha,
			A.B + (B.B - A.B) * Alpha,
			A.A + (B.A - A.A) * Alpha);
	}

	void AppendStripAsTriangleList(const TArray<uint32>& Strip, TArray<uint32>& Indices)
	{
		if (Strip.size() < 3)
		{
			return;
		}

		for (int32 Index = 0; Index + 2 < static_cast<int32>(Strip.size()); ++Index)
		{
			const uint32 A = Strip[Index];
			const uint32 B = Strip[Index + 1];
			const uint32 C = Strip[Index + 2];
			if (A == B || B == C || A == C)
			{
				continue;
			}

			if ((Index & 1) == 0)
			{
				Indices.push_back(A);
				Indices.push_back(B);
				Indices.push_back(C);
			}
			else
			{
				Indices.push_back(B);
				Indices.push_back(A);
				Indices.push_back(C);
			}
		}
	}

	FVector CalcBeamUp(const FVector& Location, const FVector& EndPoint, const FVector& ViewOrigin, const FVector& FallbackUp)
	{
		const FVector Right = (Location - EndPoint).GetSafeNormal(1.0e-6f, FVector::XAxisVector);
		FVector Up = Right.Cross(Location - ViewOrigin).GetSafeNormal(1.0e-6f, FallbackUp);
		return Up.GetSafeNormal(1.0e-6f, FVector::ZAxisVector);
	}

	float CalcBeamTiles(const FDynamicBeam2EmitterReplayData& Source, const FBeam2TypeDataPayload& BeamData)
	{
		float Tiles = 1.0f;
		if (Source.TextureTileDistance > 1.0e-6f)
		{
			Tiles = FVector::Distance(BeamData.TargetPoint, BeamData.SourcePoint) / Source.TextureTileDistance;
		}
		else
		{
			Tiles = static_cast<float>(std::max(1, Source.TextureTile));
		}

		if (BeamData.TravelRatio > 1.0e-6f)
		{
			Tiles *= BeamData.TravelRatio;
		}
		return Tiles;
	}

	void AppendBeamEdge(
		const FDynamicBeam2EmitterReplayData& Source,
		const FBaseParticle& Particle,
		const FVector& Center,
		const FVector& OldCenter,
		const FVector& Up,
		float Taper,
		float TexU,
		float TexU2,
		TArray<FParticleBeamTrailVertex>& Vertices,
		TArray<uint32>& Strip)
	{
		const FVector2 Size(Particle.Size.X * Source.Scale.X, Particle.Size.X * Source.Scale.X);
		const FVector Offset = Up * (Size.X * Taper);
		const uint32 BaseIndex = static_cast<uint32>(Vertices.size());

		FParticleBeamTrailVertex Top;
		Top.Position = Center + Offset;
		Top.OldPosition = OldCenter;
		Top.RelativeTime = Particle.RelativeTime;
		Top.ParticleId = 0.0f;
		Top.Size = Size;
		Top.Rotation = Particle.Rotation;
		Top.SubImageIndex = 0.0f;
		Top.Color = Particle.Color;
		Top.Tex_U = TexU;
		Top.Tex_V = 0.0f;
		Top.Tex_U2 = TexU2;
		Top.Tex_V2 = 0.0f;

		FParticleBeamTrailVertex Bottom = Top;
		Bottom.Position = Center - Offset;
		Bottom.Tex_V = 1.0f;
		Bottom.Tex_V2 = 1.0f;

		Vertices.push_back(Top);
		Vertices.push_back(Bottom);
		Strip.push_back(BaseIndex);
		Strip.push_back(BaseIndex + 1);
	}

	void ConvertBeamStripToTriangles(const TArray<uint32>& Strip, TArray<uint32>& Indices)
	{
		if (Strip.size() < 3)
		{
			return;
		}

		for (int32 Index = 0; Index + 2 < static_cast<int32>(Strip.size()); ++Index)
		{
			const uint32 A = Strip[Index];
			const uint32 B = Strip[Index + 1];
			const uint32 C = Strip[Index + 2];
			if (A == B || B == C || A == C)
			{
				continue;
			}

			// D3D triangle-strip winding flips every primitive. UE submits this as a
			// strip; Krafton uploads a triangle list, so preserve the strip's logical
			// winding at this final adapter boundary.
			if ((Index & 1) == 0)
			{
				Indices.push_back(A);
				Indices.push_back(B);
				Indices.push_back(C);
			}
			else
			{
				Indices.push_back(B);
				Indices.push_back(A);
				Indices.push_back(C);
			}
		}
	}

	float ReadTaper(const FDynamicBeam2EmitterReplayData& Source, const FBaseParticle* Particle, const FBeam2TypeDataPayload& BeamData, int32 PointIndex, int32 PointCount)
	{
		const int32 TaperCount = ComputeBeamTaperCount(Source, BeamData);
		if (!Particle || Source.TaperValuesOffset < 0 || TaperCount <= 0)
		{
			return 1.0f;
		}
		const float* Values = reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(Particle) + Source.TaperValuesOffset);
		const int32 Index = std::max(0, std::min(TaperCount - 1,
			PointCount > 1 ? static_cast<int32>((static_cast<float>(PointIndex) / static_cast<float>(PointCount - 1)) * static_cast<float>(TaperCount - 1)) : 0));
		return Values[Index];
	}

	FVector ReadBeamNoisePoint(
		const FDynamicBeam2EmitterReplayData& Source,
		const FBaseParticle& Particle,
		const FVector* NoisePoints,
		const FVector* NextNoisePoints,
		int32 NoiseIndex)
	{
		const int32 ClampedIndex = std::max(0, std::min(std::max(0, Source.Frequency), NoiseIndex));
		FVector NoisePoint = NoisePoints ? NoisePoints[ClampedIndex] : FVector::ZeroVector;

		if (Source.bSmoothNoise_Enabled &&
			Source.NoiseLockTime >= 0.0f &&
			NextNoisePoints &&
			Source.NoiseRateOffset >= 0)
		{
			const float* NoiseRate = reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(&Particle) + Source.NoiseRateOffset);
			const FVector NextNoise = NextNoisePoints[ClampedIndex];
			const FVector NoiseDir = (NextNoise - NoisePoint).GetSafeNormal(1.0e-6f, FVector::ZeroVector);
			const FVector CheckNoisePoint = NoisePoint + (NoiseDir * Source.NoiseSpeed) * (NoiseRate ? *NoiseRate : 0.0f);
			if (std::fabs(CheckNoisePoint.X - NextNoise.X) < Source.NoiseLockRadius &&
				std::fabs(CheckNoisePoint.Y - NextNoise.Y) < Source.NoiseLockRadius &&
				std::fabs(CheckNoisePoint.Z - NextNoise.Z) < Source.NoiseLockRadius)
			{
				NoisePoint = NextNoise;
			}
			else
			{
				NoisePoint = CheckNoisePoint;
			}
		}

		return NoisePoint;
	}

	float ReadBeamNoiseDistanceScale(const FDynamicBeam2EmitterReplayData& Source, const FBaseParticle& Particle)
	{
		if (Source.NoiseDistanceScaleOffset < 0)
		{
			return 1.0f;
		}

		const float* NoiseDistanceScalePayload =
			reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(&Particle) + Source.NoiseDistanceScaleOffset);
		return NoiseDistanceScalePayload ? *NoiseDistanceScalePayload : 1.0f;
	}

	FVector SampleBeamNoiseOffsetAtIndex(
		const FDynamicBeam2EmitterReplayData& Source,
		const FBaseParticle& Particle,
		int32 NoiseIndex,
		const FVector* NoisePoints,
		const FVector* NextNoisePoints)
	{
		if (!NoisePoints || Source.Frequency <= 0)
		{
			return FVector::ZeroVector;
		}

		return ReadBeamNoisePoint(Source, Particle, NoisePoints, NextNoisePoints, NoiseIndex)
			* Source.NoiseRangeScale
			* ReadBeamNoiseDistanceScale(Source, Particle);
	}

	FVector SafeBeamInterpPoint(const FVector* InterpPoints, int32 InterpPointCount, int32 Index, const FVector& Fallback)
	{
		if (!InterpPoints || InterpPointCount <= 0)
		{
			return Fallback;
		}
		return InterpPoints[std::max(0, std::min(InterpPointCount - 1, Index))];
	}

	FVector GetBeamInterpolatedNoiseCurrentPosition(
		const FVector* InterpPoints,
		int32 InterpPointCount,
		const FVector& TargetPoint,
		int32 StepIndex,
		int32 StepCount,
		int32 InterpIndex,
		float InterpFraction,
		bool bInterpFractionIsZero)
	{
		if (bInterpFractionIsZero)
		{
			return SafeBeamInterpPoint(InterpPoints, InterpPointCount, StepIndex * InterpIndex, TargetPoint);
		}

		const int32 BaseIndex = StepIndex * InterpIndex;
		if (StepIndex == StepCount - 1)
		{
			return SafeBeamInterpPoint(InterpPoints, InterpPointCount, BaseIndex, TargetPoint) * (1.0f - InterpFraction)
				+ TargetPoint * InterpFraction;
		}

		return SafeBeamInterpPoint(InterpPoints, InterpPointCount, BaseIndex, TargetPoint) * (1.0f - InterpFraction)
			+ SafeBeamInterpPoint(InterpPoints, InterpPointCount, BaseIndex + 1, TargetPoint) * InterpFraction;
	}

	FVector GetBeamInterpolatedNoiseNextPosition(
		const FVector* InterpPoints,
		int32 InterpPointCount,
		const FVector& TargetPoint,
		int32 StepIndex,
		int32 StepCount,
		int32 InterpIndex,
		float InterpFraction,
		bool bInterpFractionIsZero)
	{
		if (bInterpFractionIsZero)
		{
			if (StepIndex == StepCount - 2)
			{
				return TargetPoint;
			}
			return SafeBeamInterpPoint(InterpPoints, InterpPointCount, (StepIndex + 2) * InterpIndex, TargetPoint);
		}

		const int32 BaseIndex = (StepIndex + 1) * InterpIndex;
		if (StepIndex == StepCount - 1)
		{
			return SafeBeamInterpPoint(InterpPoints, InterpPointCount, BaseIndex, TargetPoint) * InterpFraction
				+ TargetPoint * (1.0f - InterpFraction);
		}

		return SafeBeamInterpPoint(InterpPoints, InterpPointCount, BaseIndex, TargetPoint) * InterpFraction
			+ SafeBeamInterpPoint(InterpPoints, InterpPointCount, BaseIndex + 1, TargetPoint) * (1.0f - InterpFraction);
	}

	void AppendBeamPathSheet(
		const FDynamicBeam2EmitterReplayData& Source,
		const FBaseParticle& Particle,
		const TArray<FVector>& Points,
		const TArray<float>& Tapers,
		const FFrameContext& Frame,
		int32 SheetIndex,
		float Tiles,
		TArray<FParticleBeamTrailVertex>& Vertices,
		TArray<uint32>& Indices)
	{
		if (Points.size() < 2)
		{
			return;
		}

		TArray<uint32> Strip;
		Strip.reserve(Points.size() * 2);
		FVector CachedUp = FVector::ZeroVector;

		for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
		{
			const int32 EdgePointIndex = PointIndex == 0
				? ((PointIndex + 1 < static_cast<int32>(Points.size())) ? PointIndex + 1 : PointIndex)
				: PointIndex - 1;
			const FVector& Center = Points[PointIndex];
			const FVector& EdgePoint = Points[std::max(0, EdgePointIndex)];
			const FVector LocationForUp = PointIndex == 0 ? Center : EdgePoint;
			const FVector EndPointForUp = PointIndex == 0 ? EdgePoint : Center;
			const FVector Right = (LocationForUp - EndPointForUp).GetSafeNormal(1.0e-6f, FVector::XAxisVector);

			FVector Up;
			if (Source.UpVectorStepSize == 0 || PointIndex == 0 || CachedUp.IsNearlyZero())
			{
				Up = CalcBeamUp(LocationForUp, EndPointForUp, Frame.CameraPosition, Frame.CameraUp);
				if (Source.UpVectorStepSize != 0)
				{
					CachedUp = Up;
				}
			}
			else
			{
				Up = CachedUp;
			}

			if (SheetIndex > 0)
			{
				const float Angle = (PI / static_cast<float>(std::max(1, Source.Sheets))) * static_cast<float>(SheetIndex);
				Up = RotateAroundAxis(Up, Right, Angle).GetSafeNormal(1.0e-6f, Up);
			}

			const float Alpha = static_cast<float>(PointIndex) / static_cast<float>(Points.size() - 1);
			const float Taper = PointIndex < static_cast<int32>(Tapers.size()) ? Tapers[PointIndex] : 1.0f;
			const FVector OldCenter = (PointIndex + 1 == static_cast<int32>(Points.size())) ? Particle.OldLocation : Center;
			AppendBeamEdge(Source, Particle, Center, OldCenter, Up, Taper, Tiles * Alpha, Alpha, Vertices, Strip);
		}

		ConvertBeamStripToTriangles(Strip, Indices);
	}
}

void FDynamicSpriteEmitterDataBase::SortSpriteParticles(const FParticleSortContext& SortCtx)
{
    const FDynamicSpriteEmitterReplayDataBase& Source =
        static_cast<const FDynamicSpriteEmitterReplayDataBase&>(GetSource());

    if (Source.SortMode == PSORTMODE_None) return;
    if (!Source.DataContainer.ParticleIndices || !Source.DataContainer.ParticleData) return;

    const int32 Count = Source.DataContainer.ParticleIndicesNumShorts;
    if (Count <= 1) return;

    uint16* Indices       = Source.DataContainer.ParticleIndices;
    const uint8* RawData  = Source.DataContainer.ParticleData;
    const int32 Stride    = Source.ParticleStride;

    auto GetParticle = [&](uint16 Idx) -> const FBaseParticle*
    {
        return reinterpret_cast<const FBaseParticle*>(RawData + Idx * Stride);
    };

    switch (Source.SortMode)
    {
    case PSORTMODE_DistanceToView:
        std::sort(Indices, Indices + Count, [&](uint16 A, uint16 B)
        {
            const float DA = FVector::DistSquared(GetParticle(A)->Location, SortCtx.CameraPosition);
            const float DB = FVector::DistSquared(GetParticle(B)->Location, SortCtx.CameraPosition);
            return DA > DB;
        });
        break;

    case PSORTMODE_ViewProjDepth:
        std::sort(Indices, Indices + Count, [&](uint16 A, uint16 B)
        {
            const float DA = (GetParticle(A)->Location - SortCtx.CameraPosition).Dot(SortCtx.CameraForward);
            const float DB = (GetParticle(B)->Location - SortCtx.CameraPosition).Dot(SortCtx.CameraForward);
            return DA > DB;
        });
        break;

    case PSORTMODE_Age_OldestFirst:
        std::sort(Indices, Indices + Count, [&](uint16 A, uint16 B)
        {
            return GetParticle(A)->RelativeTime > GetParticle(B)->RelativeTime;
        });
        break;

    case PSORTMODE_Age_NewestFirst:
        std::sort(Indices, Indices + Count, [&](uint16 A, uint16 B)
        {
            return GetParticle(A)->RelativeTime < GetParticle(B)->RelativeTime;
        });
        break;
    }
}

FDynamicBeam2EmitterData::~FDynamicBeam2EmitterData()
{
	RemoveCPUStaging(this);
}

const TArray<FParticleBeamTrailVertex>& FDynamicBeam2EmitterData::GetBuiltVertices() const
{
	return GetCPUStagingConst(this).Vertices;
}

const TArray<uint32>& FDynamicBeam2EmitterData::GetBuiltIndices() const
{
	return GetCPUStagingConst(this).Indices;
}

void FDynamicBeam2EmitterData::BuildMeshData(const FFrameContext& Frame)
{
    FBeamTrailCPUStaging& Staging = GetCPUStaging(this);
    Staging.Vertices.clear();
    Staging.Indices.clear();
    if (!Source.bRenderGeometry)
    {
        return;
    }
    DoBufferFill(Frame);
}

void FDynamicBeam2EmitterData::DoBufferFill(const FFrameContext& Frame)
{
    // UE original responsibility:
    // FDynamicBeam2EmitterData::DoBufferFill chooses the correct Beam path and
    // calls FillIndexData plus one of FillVertexData_NoNoise, FillData_Noise,
    // or FillData_InterpolatedNoise.
    //
    // Missing Jungle foundation:
    // FAsyncBufferFillData, beam-trail vertex factory, RHI dynamic buffer fill,
    // and the exact UE strip/degenerate index writer.
    //
    // Keep this boundary. Do not replace it with a simplified quad builder.

    FillIndexData();

    if (Source.bLowFreqNoise_Enabled)
    {
        if (Source.InterpolationPoints > 0)
        {
            FillData_InterpolatedNoise(Frame);
        }
        else
        {
            FillData_Noise(Frame);
        }
    }
    else
    {
        FillVertexData_NoNoise(Frame);
    }
}

int32 FDynamicBeam2EmitterData::FillIndexData()
{
	// UE original responsibility: build the beam strip index stream, including
	// sheets and degenerate joins. Jungle keeps the same logical point sequence
	// and sheet pass, then converts only the final output to triangle-list indices.
	return static_cast<int32>(GetCPUStaging(this).Indices.size());
}

int32 FDynamicBeam2EmitterData::FillVertexData_NoNoise(const FFrameContext& Frame)
{
	TArray<FVector> Points;
	TArray<float> Tapers;
	FBeamTrailCPUStaging& Staging = GetCPUStaging(this);
	const int32 InitialVertexCount = static_cast<int32>(Staging.Vertices.size());

	for (int32 ActiveIndex = 0; ActiveIndex < Source.ActiveParticleCount; ++ActiveIndex)
	{
		const FBaseParticle* Particle = GetReplayActiveParticle(Source, ActiveIndex);
		const FBeam2TypeDataPayload* BeamData = GetReplayPayload<FBeam2TypeDataPayload>(Source, Particle, Source.BeamDataOffset);
		if (!Particle || !BeamData)
		{
			continue;
		}

		Points.clear();
		Tapers.clear();
		const bool bLocked = BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints);
		const FVector EndPoint = bLocked ? BeamData->TargetPoint : Particle->Location;
		const FVector* InterpPoints = Source.InterpolatedPointsOffset >= 0
			? reinterpret_cast<const FVector*>(reinterpret_cast<const uint8*>(Particle) + Source.InterpolatedPointsOffset)
			: nullptr;

		Points.push_back(BeamData->SourcePoint);
		if (InterpPoints && Source.InterpolationPoints > 0 && BeamData->Steps > 0)
		{
			const int32 StepCount = std::min(BeamData->Steps, Source.InterpolationPoints);
			for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
			{
				Points.push_back(InterpPoints[StepIndex]);
			}
		}
		else
		{
			Points.push_back(EndPoint);
		}

		if (Points.size() == 1 || FVector::Distance(Points.back(), EndPoint) > 1.0e-4f)
		{
			Points.push_back(EndPoint);
		}

		for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
		{
			Tapers.push_back(ReadTaper(Source, Particle, *BeamData, PointIndex, static_cast<int32>(Points.size())));
		}

		const float Tiles = CalcBeamTiles(Source, *BeamData);
		for (int32 SheetIndex = 0; SheetIndex < std::max(1, Source.Sheets); ++SheetIndex)
		{
			AppendBeamPathSheet(Source, *Particle, Points, Tapers, Frame, SheetIndex, Tiles, Staging.Vertices, Staging.Indices);
		}
	}

	return static_cast<int32>(Staging.Vertices.size()) - InitialVertexCount;
}

int32 FDynamicBeam2EmitterData::FillData_Noise(const FFrameContext& Frame)
{
	TArray<FVector> Points;
	TArray<float> Tapers;
	FBeamTrailCPUStaging& Staging = GetCPUStaging(this);
	const int32 InitialVertexCount = static_cast<int32>(Staging.Vertices.size());

	for (int32 ActiveIndex = 0; ActiveIndex < Source.ActiveParticleCount; ++ActiveIndex)
	{
		const FBaseParticle* Particle = GetReplayActiveParticle(Source, ActiveIndex);
		const FBeam2TypeDataPayload* BeamData = GetReplayPayload<FBeam2TypeDataPayload>(Source, Particle, Source.BeamDataOffset);
		if (!Particle || !BeamData)
		{
			continue;
		}

		const FVector* NoisePoints = Source.TargetNoisePointsOffset >= 0
			? reinterpret_cast<const FVector*>(reinterpret_cast<const uint8*>(Particle) + Source.TargetNoisePointsOffset)
			: nullptr;
		const FVector* NextNoisePoints = Source.NextNoisePointsOffset >= 0
			? reinterpret_cast<const FVector*>(reinterpret_cast<const uint8*>(Particle) + Source.NextNoisePointsOffset)
			: nullptr;
		if (!NoisePoints)
		{
			continue;
		}

		Points.clear();
		Tapers.clear();
		const bool bLocked = BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints);
		const int32 Steps = BeamData->Steps;
		if (Steps <= 0)
		{
			continue;
		}
		const int32 TessFactor = std::max(1, Source.NoiseTessellation);
		const FVector Start = BeamData->SourcePoint;
		const FVector End = bLocked ? BeamData->TargetPoint : Particle->Location;
		const FVector Direction = (End - Start).GetSafeNormal(1.0e-6f, BeamData->Direction);
		float StepSize = static_cast<float>(BeamData->StepSize);

		FVector LastPosition = Start;
		FVector LastDraw = Start;
		const float SourceStrength = Source.bUseSource ? static_cast<float>(BeamData->SourceStrength) : Source.NoiseTangentStrength;
		FVector LastTangent = (Source.bUseSource ? BeamData->SourceTangent : Direction).GetSafeNormal(1.0e-6f, Direction) * SourceStrength;
		Points.push_back(LastDraw);

		for (int32 StepIndex = 0; StepIndex < Steps; ++StepIndex)
		{
			const FVector CurrentPosition = LastPosition + Direction * StepSize;
			FVector CurrentDraw = CurrentPosition + SampleBeamNoiseOffsetAtIndex(Source, *Particle, StepIndex, NoisePoints, NextNoisePoints);

			const bool bFinalStep = bLocked && (StepIndex + 1 == Steps);
			FVector NextTargetPosition = CurrentPosition + Direction * StepSize;
			FVector NextTargetDraw = NextTargetPosition + SampleBeamNoiseOffsetAtIndex(Source, *Particle, StepIndex + 1, NoisePoints, NextNoisePoints);
			FVector TargetTangent = FVector::ZeroVector;
			float TargetStrength = Source.NoiseTangentStrength;
			if (bFinalStep)
			{
				NextTargetDraw = BeamData->TargetPoint;
				if (Source.bTargetNoise)
				{
					NextTargetDraw += SampleBeamNoiseOffsetAtIndex(Source, *Particle, Source.Frequency, NoisePoints, NextNoisePoints);
				}
				if (Source.bUseTarget)
				{
					TargetTangent = BeamData->TargetTangent;
					TargetStrength = static_cast<float>(BeamData->TargetStrength);
				}
				else
				{
					TargetTangent = (NextTargetDraw - LastDraw) * ((1.0f - Source.NoiseTension) * 0.5f);
				}
			}
			else
			{
				TargetTangent = (NextTargetDraw - LastDraw) * ((1.0f - Source.NoiseTension) * 0.5f);
			}
			TargetTangent = TargetTangent.GetSafeNormal(1.0e-6f, Direction) * TargetStrength;

			for (int32 TessIndex = 0; TessIndex < TessFactor; ++TessIndex)
			{
				const float TessAlpha = static_cast<float>(TessIndex + 1) / static_cast<float>(TessFactor);
				Points.push_back(CubicInterp(LastDraw, LastTangent, CurrentDraw, TargetTangent, TessAlpha));
			}

			LastPosition = CurrentPosition;
			LastDraw = CurrentDraw;
			LastTangent = TargetTangent;
		}

		if (bLocked)
		{
			FVector CurrentDraw = BeamData->TargetPoint;
			if (Source.bTargetNoise)
			{
				CurrentDraw += SampleBeamNoiseOffsetAtIndex(Source, *Particle, Source.Frequency, NoisePoints, NextNoisePoints);
			}

			FVector TargetTangent = Source.bUseTarget
				? BeamData->TargetTangent
				: (CurrentDraw - LastDraw) * ((1.0f - Source.NoiseTension) * 0.5f);
			const float TargetStrength = Source.bUseTarget ? static_cast<float>(BeamData->TargetStrength) : Source.NoiseTangentStrength;
			TargetTangent = TargetTangent.GetSafeNormal(1.0e-6f, Direction) * TargetStrength;

			for (int32 TessIndex = 0; TessIndex < TessFactor; ++TessIndex)
			{
				const float TessAlpha = static_cast<float>(TessIndex + 1) / static_cast<float>(TessFactor);
				Points.push_back(CubicInterp(LastDraw, LastTangent, CurrentDraw, TargetTangent, TessAlpha));
			}
		}

		for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
		{
			Tapers.push_back(ReadTaper(Source, Particle, *BeamData, PointIndex, static_cast<int32>(Points.size())));
		}

		const float Tiles = CalcBeamTiles(Source, *BeamData);
		for (int32 SheetIndex = 0; SheetIndex < std::max(1, Source.Sheets); ++SheetIndex)
		{
			AppendBeamPathSheet(Source, *Particle, Points, Tapers, Frame, SheetIndex, Tiles, Staging.Vertices, Staging.Indices);
		}
	}

	return static_cast<int32>(Staging.Vertices.size()) - InitialVertexCount;
}

int32 FDynamicBeam2EmitterData::FillData_InterpolatedNoise(const FFrameContext& Frame)
{
	TArray<FVector> Points;
	TArray<float> Tapers;
	FBeamTrailCPUStaging& Staging = GetCPUStaging(this);
	const int32 InitialVertexCount = static_cast<int32>(Staging.Vertices.size());

	for (int32 ActiveIndex = 0; ActiveIndex < Source.ActiveParticleCount; ++ActiveIndex)
	{
		const FBaseParticle* Particle = GetReplayActiveParticle(Source, ActiveIndex);
		const FBeam2TypeDataPayload* BeamData = GetReplayPayload<FBeam2TypeDataPayload>(Source, Particle, Source.BeamDataOffset);
		if (!Particle || !BeamData)
		{
			continue;
		}

		const FVector* InterpPoints = Source.InterpolatedPointsOffset >= 0
			? reinterpret_cast<const FVector*>(reinterpret_cast<const uint8*>(Particle) + Source.InterpolatedPointsOffset)
			: nullptr;
		const FVector* NoisePoints = Source.TargetNoisePointsOffset >= 0
			? reinterpret_cast<const FVector*>(reinterpret_cast<const uint8*>(Particle) + Source.TargetNoisePointsOffset)
			: nullptr;
		const FVector* NextNoisePoints = Source.NextNoisePointsOffset >= 0
			? reinterpret_cast<const FVector*>(reinterpret_cast<const uint8*>(Particle) + Source.NextNoisePointsOffset)
			: nullptr;
		if (!InterpPoints || !NoisePoints || BeamData->Steps <= 0)
		{
			continue;
		}

		Points.clear();
		Tapers.clear();
		const bool bLocked = BEAM2_TYPEDATA_LOCKED(BeamData->Lock_Max_NumNoisePoints);
		const int32 Steps = BeamData->Steps;
		if (Steps <= 0)
		{
			continue;
		}
		const int32 TessFactor = std::max(1, Source.NoiseTessellation);
		const FVector End = bLocked ? BeamData->TargetPoint : Particle->Location;
		const FVector Direction = (End - BeamData->SourcePoint).GetSafeNormal(1.0e-6f, BeamData->Direction);
		const float InterpStepSize = static_cast<float>(BeamData->InterpolationSteps) / static_cast<float>(Steps);
		const float InterpFraction = InterpStepSize - std::floor(InterpStepSize);
		const bool bInterpFractionIsZero = false;
		const int32 InterpIndex = static_cast<int32>(std::floor(InterpStepSize));

		FVector LastDraw = BeamData->SourcePoint;
		const float SourceStrength = Source.bUseSource ? Source.NoiseTangentStrength : Source.NoiseTangentStrength;
		FVector LastTangent = (Source.bUseSource ? BeamData->SourceTangent : Direction).GetSafeNormal(1.0e-6f, Direction) * SourceStrength;
		Points.push_back(LastDraw);

		for (int32 StepIndex = 0; StepIndex < Steps; ++StepIndex)
		{
			const FVector CurrentPosition = GetBeamInterpolatedNoiseCurrentPosition(
				InterpPoints,
				Source.InterpolationPoints,
				BeamData->TargetPoint,
				StepIndex,
				Steps,
				InterpIndex,
				InterpFraction,
				bInterpFractionIsZero);
			FVector CurrentDraw = CurrentPosition + SampleBeamNoiseOffsetAtIndex(Source, *Particle, StepIndex, NoisePoints, NextNoisePoints);

			const bool bFinalStep = bLocked && (StepIndex + 1 == Steps);
			FVector NextTargetPosition = GetBeamInterpolatedNoiseNextPosition(
				InterpPoints,
				Source.InterpolationPoints,
				BeamData->TargetPoint,
				StepIndex,
				Steps,
				InterpIndex,
				InterpFraction,
				bInterpFractionIsZero);
			FVector NextTargetDraw = NextTargetPosition + SampleBeamNoiseOffsetAtIndex(Source, *Particle, StepIndex + 1, NoisePoints, NextNoisePoints);
			FVector TargetTangent = FVector::ZeroVector;
			float TargetStrength = Source.NoiseTangentStrength;
			if (bFinalStep)
			{
				NextTargetDraw = BeamData->TargetPoint;
				if (Source.bTargetNoise)
				{
					NextTargetDraw += SampleBeamNoiseOffsetAtIndex(Source, *Particle, Source.Frequency, NoisePoints, NextNoisePoints);
				}
				if (Source.bUseTarget)
				{
					TargetTangent = BeamData->TargetTangent;
					TargetStrength = Source.NoiseTangentStrength;
				}
				else
				{
					TargetTangent = (NextTargetDraw - LastDraw) * ((1.0f - Source.NoiseTension) * 0.5f);
				}
			}
			else
			{
				TargetTangent = (NextTargetDraw - LastDraw) * ((1.0f - Source.NoiseTension) * 0.5f);
			}
			TargetTangent = TargetTangent.GetSafeNormal(1.0e-6f, Direction) * TargetStrength;

			for (int32 TessIndex = 0; TessIndex < TessFactor; ++TessIndex)
			{
				const float TessAlpha = static_cast<float>(TessIndex + 1) / static_cast<float>(TessFactor);
				Points.push_back(CubicInterp(LastDraw, LastTangent, CurrentDraw, TargetTangent, TessAlpha));
			}

			LastDraw = CurrentDraw;
			LastTangent = TargetTangent;
		}

		if (bLocked)
		{
			FVector CurrentDraw = BeamData->TargetPoint;
			if (Source.bTargetNoise)
			{
				CurrentDraw += SampleBeamNoiseOffsetAtIndex(Source, *Particle, Source.Frequency, NoisePoints, NextNoisePoints);
			}

			FVector TargetTangent = Source.bUseTarget
				? BeamData->TargetTangent
				: (CurrentDraw - LastDraw) * ((1.0f - Source.NoiseTension) * 0.5f);
			TargetTangent = TargetTangent.GetSafeNormal(1.0e-6f, Direction) * Source.NoiseTangentStrength;

			for (int32 TessIndex = 0; TessIndex < TessFactor; ++TessIndex)
			{
				const float TessAlpha = static_cast<float>(TessIndex + 1) / static_cast<float>(TessFactor);
				Points.push_back(CubicInterp(LastDraw, LastTangent, CurrentDraw, TargetTangent, TessAlpha));
			}
		}

		for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
		{
			Tapers.push_back(ReadTaper(Source, Particle, *BeamData, PointIndex, static_cast<int32>(Points.size())));
		}

		const float Tiles = CalcBeamTiles(Source, *BeamData);
		for (int32 SheetIndex = 0; SheetIndex < std::max(1, Source.Sheets); ++SheetIndex)
		{
			AppendBeamPathSheet(Source, *Particle, Points, Tapers, Frame, SheetIndex, Tiles, Staging.Vertices, Staging.Indices);
		}
	}

	return static_cast<int32>(Staging.Vertices.size()) - InitialVertexCount;
}

void FDynamicTrailsEmitterData::DoBufferFill(const FFrameContext& Frame)
{
    // UE original responsibility:
    // FDynamicTrailsEmitterData::DoBufferFill checks async buffer inputs and calls
    // FillIndexData then FillVertexData. Jungle's CPU staging arrays stand in for
    // the missing FAsyncBufferFillData/RHI layer, and simulation payload is read-only here.
    FillIndexData();
    FillVertexData(Frame);
}

int32 FDynamicTrailsEmitterData::FillIndexData()
{
	FBeamTrailCPUStaging& Staging = GetCPUStaging(this);
	const int32 InitialTriangleCount = static_cast<int32>(Staging.Indices.size()) / 3;
	if (!SourcePointer)
	{
		return 0;
	}

	const FDynamicTrailsEmitterReplayData& Source = *SourcePointer;
	TArray<uint32> StripIndices;
	uint32 VertexIndex = 0;
	int32 CurrentTrail = 0;
	for (int32 ActiveIndex = 0; ActiveIndex < Source.ActiveParticleCount; ++ActiveIndex)
	{
		const uint16 DirectIndex = Source.DataContainer.ParticleIndices ? Source.DataContainer.ParticleIndices[ActiveIndex] : 0;
		const FBaseParticle* StartParticle = GetReplayParticle(Source, DirectIndex);
		const FRibbonTypeDataPayload* StartPayload = GetReplayPayload<FRibbonTypeDataPayload>(Source, StartParticle, Source.TrailDataOffset);
		if (!StartParticle || !StartPayload || !TRAIL_EMITTER_IS_HEAD(StartPayload->Flags))
		{
			continue;
		}

		const int32 LocalTrianglesToRender = StartPayload->TriangleCount;
		if (LocalTrianglesToRender == 0)
		{
			continue;
		}

		// UE fills a triangle strip and joins multiple trails with degenerate
		// indices. Krafton's renderer consumes triangle lists, so conversion is
		// delayed until the strip stream is complete.
		if (CurrentTrail == 0)
		{
			StripIndices.push_back(VertexIndex++);
			StripIndices.push_back(VertexIndex++);
		}
		else
		{
			StripIndices.push_back(VertexIndex - 1);
			StripIndices.push_back(VertexIndex);
			StripIndices.push_back(VertexIndex++);
			StripIndices.push_back(VertexIndex++);
		}

		for (int32 LocalIndex = 0; LocalIndex < LocalTrianglesToRender; ++LocalIndex)
		{
			StripIndices.push_back(VertexIndex++);
		}

		++CurrentTrail;
	}

	AppendStripAsTriangleList(StripIndices, Staging.Indices);
	return static_cast<int32>(Staging.Indices.size()) / 3 - InitialTriangleCount;
}

int32 FDynamicTrailsEmitterData::FillVertexData(const FFrameContext& /*Frame*/)
{
    // UE original responsibility:
    // Base trail vertex fill entry point, overridden by Ribbon/AnimTrail dynamic data.
    // Missing Jungle foundation: shared trail render fill path.
    return 0;
}

FDynamicRibbonEmitterData::~FDynamicRibbonEmitterData()
{
	RemoveCPUStaging(this);
}

const TArray<FParticleBeamTrailVertex>& FDynamicRibbonEmitterData::GetBuiltVertices() const
{
	return GetCPUStagingConst(this).Vertices;
}

const TArray<uint32>& FDynamicRibbonEmitterData::GetBuiltIndices() const
{
	return GetCPUStagingConst(this).Indices;
}

void FDynamicRibbonEmitterData::BuildMeshData(const FFrameContext& Frame)
{
    FBeamTrailCPUStaging& Staging = GetCPUStaging(this);
    Staging.Vertices.clear();
    Staging.Indices.clear();
    if (!bRenderGeometry)
    {
        return;
    }
    DoBufferFill(Frame);
}

int32 FDynamicRibbonEmitterData::FillVertexData(const FFrameContext& Frame)
{
	FBeamTrailCPUStaging& Staging = GetCPUStaging(this);
	const int32 InitialVertexCount = static_cast<int32>(Staging.Vertices.size());
	const FVector CameraUp = Frame.CameraUp.GetSafeNormal(1.0e-6f, FVector::ZAxisVector);
	const FVector ViewOrigin = Frame.CameraPosition;
	float CurrDistance = 0.0f;

	for (int32 ActiveIndex = 0; ActiveIndex < Source.ActiveParticleCount; ++ActiveIndex)
	{
		const uint16 DirectIndex = Source.DataContainer.ParticleIndices ? Source.DataContainer.ParticleIndices[ActiveIndex] : 0;
		const FBaseParticle* PackingParticle = GetReplayParticle(Source, DirectIndex);
		const FRibbonTypeDataPayload* TrailPayload = GetReplayPayload<FRibbonTypeDataPayload>(Source, PackingParticle, Source.TrailDataOffset);
		if (!PackingParticle || !TrailPayload || !TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags))
		{
			continue;
		}
		if (TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags) == TRAIL_EMITTER_NULL_NEXT)
		{
			continue;
		}

		const float TextureIncrement = TrailPayload->TriangleCount > 0
			? 1.0f / (static_cast<float>(TrailPayload->TriangleCount) / 2.0f)
			: 0.0f;
		float TexU = 0.0f;
		const FBaseParticle* PrevParticle = nullptr;
		const FRibbonTypeDataPayload* PrevTrailPayload = nullptr;
		FVector PrevWorkingUp = FVector::ZAxisVector;

		FVector WorkingUp = TrailPayload->Up;
		if (RenderAxisOption == Trails_CameraUp)
		{
			FVector DirToCamera = (PackingParticle->Location - ViewOrigin).GetSafeNormal(1.0e-6f, FVector::ZeroVector);
			FVector NormalizedTangent = TrailPayload->Tangent.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
			WorkingUp = NormalizedTangent.Cross(DirToCamera);
			if (WorkingUp.IsNearlyZero())
			{
				WorkingUp = CameraUp;
			}
			WorkingUp = WorkingUp.GetSafeNormal(1.0e-6f, CameraUp);
		}

		while (TrailPayload && PackingParticle)
		{
			const float CurrSize = PackingParticle->Size.X * Source.Scale.X;
			const int32 InterpCount = TrailPayload->RenderingInterpCount;
			if (InterpCount > 1 && PrevParticle && PrevTrailPayload)
			{
				FillInterpolatedVertexData(
					Frame,
					PackingParticle,
					TrailPayload,
					PrevParticle,
					PrevTrailPayload,
					WorkingUp,
					PrevWorkingUp,
					TexU,
					TextureIncrement);
			}
			else
			{
				const float CurrTileU = bTextureTileDistance ? TrailPayload->TiledU : TexU;

				FParticleBeamTrailVertex Top;
				Top.Position = PackingParticle->Location + WorkingUp * CurrSize;
				Top.OldPosition = PackingParticle->OldLocation;
				Top.RelativeTime = PackingParticle->RelativeTime;
				Top.ParticleId = 0.0f;
				Top.Size = FVector2(CurrSize, CurrSize);
				Top.Rotation = PackingParticle->Rotation;
				Top.SubImageIndex = 0.0f;
				Top.Color = PackingParticle->Color;
				Top.Tex_U = TexU;
				Top.Tex_V = 0.0f;
				Top.Tex_U2 = CurrTileU;
				Top.Tex_V2 = 0.0f;

				FParticleBeamTrailVertex Bottom = Top;
				Bottom.Position = PackingParticle->Location - WorkingUp * CurrSize;
				Bottom.Tex_V = 1.0f;
				Bottom.Tex_V2 = 1.0f;

				Staging.Vertices.push_back(Top);
				Staging.Vertices.push_back(Bottom);
				TexU += TextureIncrement;
			}

			PrevParticle = PackingParticle;
			PrevTrailPayload = TrailPayload;
			PrevWorkingUp = WorkingUp;

			const int32 NextIndex = TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags);
			if (NextIndex == TRAIL_EMITTER_NULL_NEXT)
			{
				TrailPayload = nullptr;
				PackingParticle = nullptr;
			}
			else
			{
				PackingParticle = GetReplayParticle(Source, NextIndex);
				TrailPayload = GetReplayPayload<FRibbonTypeDataPayload>(Source, PackingParticle, Source.TrailDataOffset);
				if (!PackingParticle || !TrailPayload)
				{
					break;
				}

				WorkingUp = TrailPayload->Up;
				if (RenderAxisOption == Trails_CameraUp)
				{
					FVector DirToCamera = (PackingParticle->Location - ViewOrigin).GetSafeNormal(1.0e-6f, FVector::ZeroVector);
					FVector NormalizedTangent = TrailPayload->Tangent.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
					WorkingUp = NormalizedTangent.Cross(DirToCamera);
					if (WorkingUp.IsNearlyZero())
					{
						WorkingUp = CameraUp;
					}
					WorkingUp = WorkingUp.GetSafeNormal(1.0e-6f, CameraUp);
				}
			}
		}
	}

	// UE writes dynamic parameters to a parallel FParticleBeamTrailVertexDynamicParameter stream here.
	// Jungle currently has no beam/ribbon dynamic-parameter vertex stream in the CPU staging adapter;
	// wire ParticleSystemSceneProxy/BeamTrail.hlsl dynamic-parameter support here when that system exists.
	(void)CurrDistance;
	return static_cast<int32>(Staging.Vertices.size()) - InitialVertexCount;
}

int32 FDynamicRibbonEmitterData::FillInterpolatedVertexData(
	const FFrameContext& Frame,
	const FBaseParticle* PackingParticle,
	const FRibbonTypeDataPayload* TrailPayload,
	const FBaseParticle* PrevParticle,
	const FRibbonTypeDataPayload* PrevTrailPayload,
	const FVector& WorkingUp,
	const FVector& PrevWorkingUp,
	float& TexU,
	float TextureIncrement)
{
	FBeamTrailCPUStaging& Staging = GetCPUStaging(this);
	const int32 InitialVertexCount = static_cast<int32>(Staging.Vertices.size());
	if (!PackingParticle || !TrailPayload || !PrevParticle || !PrevTrailPayload)
	{
		return 0;
	}

	const int32 InterpCount = TrailPayload->RenderingInterpCount;
	if (InterpCount <= 1)
	{
		return 0;
	}

	const FVector CameraUp = Frame.CameraUp.GetSafeNormal(1.0e-6f, FVector::ZAxisVector);
	const FVector CurrPosition = PackingParticle->Location;
	const FVector CurrTangent = TrailPayload->Tangent;
	const FVector CurrUp = WorkingUp;
	const FLinearColor CurrColor = PackingParticle->Color;
	const FVector PrevPosition = PrevParticle->Location;
	const FVector PrevTangent = PrevTrailPayload->Tangent;
	const FVector PrevUp = PrevWorkingUp;
	const FLinearColor PrevColor = PrevParticle->Color;
	const float CurrSize = PackingParticle->Size.X * Source.Scale.X;
	const float PrevSize = PrevParticle->Size.X * Source.Scale.X;
	const float InvCount = 1.0f / static_cast<float>(InterpCount);

	for (int32 SpawnIndex = InterpCount - 1; SpawnIndex >= 0; --SpawnIndex)
	{
		const float TimeStep = InvCount * static_cast<float>(SpawnIndex);
		const FVector InterpPos = CubicInterp(CurrPosition, CurrTangent, PrevPosition, PrevTangent, TimeStep);
		const FVector InterpUp = FVector::Lerp(CurrUp, PrevUp, TimeStep).GetSafeNormal(1.0e-6f, CameraUp);
		const FLinearColor InterpColor = LerpColor(CurrColor, PrevColor, TimeStep);
		const float InterpSize = CurrSize + (PrevSize - CurrSize) * TimeStep;
		const float CurrTileU = bTextureTileDistance
			? (TrailPayload->TiledU + (PrevTrailPayload->TiledU - TrailPayload->TiledU) * TimeStep)
			: TexU;

		FParticleBeamTrailVertex Top;
		Top.Position = InterpPos + InterpUp * InterpSize;
		Top.OldPosition = Top.Position;
		Top.RelativeTime = PackingParticle->RelativeTime;
		Top.ParticleId = 0.0f;
		Top.Size = FVector2(InterpSize, InterpSize);
		Top.Rotation = PackingParticle->Rotation;
		Top.SubImageIndex = 0.0f;
		Top.Color = InterpColor;
		Top.Tex_U = TexU;
		Top.Tex_V = 0.0f;
		Top.Tex_U2 = CurrTileU;
		Top.Tex_V2 = 0.0f;

		FParticleBeamTrailVertex Bottom = Top;
		Bottom.Position = InterpPos - InterpUp * InterpSize;
		Bottom.Tex_V = 1.0f;
		Bottom.Tex_V2 = 1.0f;

		Staging.Vertices.push_back(Top);
		Staging.Vertices.push_back(Bottom);
		TexU += TextureIncrement;
	}

	return static_cast<int32>(Staging.Vertices.size()) - InitialVertexCount;
}
