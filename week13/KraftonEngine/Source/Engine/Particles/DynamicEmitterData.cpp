#include "DynamicEmitterData.h"
#include "Particles/ParticleHelper.h"
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

	struct FRibbonBuildPoint
	{
		FVector Position;
		FVector Up;
		float Width;
		float U;
		FLinearColor Color;
		float RelativeTime;
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

	FVector MakeSheetUp(const FVector& Direction, int32 SheetIndex, int32 SheetCount)
	{
		const FVector Dir = Direction.GetSafeNormal(1.0e-6f, FVector::XAxisVector);
		FVector Up = std::fabs(Dir.Dot(FVector::ZAxisVector)) > 0.95f ? FVector::YAxisVector : FVector::ZAxisVector;
		Up = (Up - Dir * Up.Dot(Dir)).GetSafeNormal(1.0e-6f, FVector::YAxisVector);
		const float Angle = SheetCount > 1 ? (PI * static_cast<float>(SheetIndex) / static_cast<float>(SheetCount)) : 0.0f;
		return RotateAroundAxis(Up, Dir, Angle).GetSafeNormal(1.0e-6f, Up);
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

	void BuildRibbonPointSequence(
		const FDynamicTrailsEmitterReplayData& Source,
		const FBaseParticle* StartParticle,
		const FRibbonTypeDataPayload* StartPayload,
		TArray<FRibbonBuildPoint>& Points)
	{
		Points.clear();
		if (!StartParticle || !StartPayload || !TRAIL_EMITTER_IS_HEAD(StartPayload->Flags))
		{
			return;
		}

		TArray<const FBaseParticle*> Particles;
		TArray<const FRibbonTypeDataPayload*> Payloads;
		const FBaseParticle* Particle = StartParticle;
		const FRibbonTypeDataPayload* Payload = StartPayload;
		while (Particle && Payload)
		{
			Particles.push_back(Particle);
			Payloads.push_back(Payload);
			const int32 NextIndex = TRAIL_EMITTER_GET_NEXT(Payload->Flags);
			if (NextIndex == TRAIL_EMITTER_NULL_NEXT || NextIndex == INDEX_NONE)
			{
				break;
			}
			Particle = GetReplayParticle(Source, NextIndex);
			Payload = GetReplayPayload<FRibbonTypeDataPayload>(Source, Particle, Source.TrailDataOffset);
		}

		if (Particles.size() < 2)
		{
			return;
		}

		for (int32 SegmentIndex = 0; SegmentIndex + 1 < static_cast<int32>(Particles.size()); ++SegmentIndex)
		{
			const FBaseParticle& P0 = *Particles[SegmentIndex];
			const FBaseParticle& P1 = *Particles[SegmentIndex + 1];
			const FRibbonTypeDataPayload& D0 = *Payloads[SegmentIndex];
			const FRibbonTypeDataPayload& D1 = *Payloads[SegmentIndex + 1];
			const int32 InterpCount = std::max(1, D1.RenderingInterpCount);

			if (SegmentIndex == 0)
			{
				Points.push_back({ P0.Location, D0.Up.GetSafeNormal(1.0e-6f, FVector::ZAxisVector), P0.Size.X * D0.PinchScaleFactor, D0.TiledU, P0.Color, P0.RelativeTime });
			}

			const FVector T0 = D0.Tangent * std::max(1.0f, D0.SpawnDelta);
			const FVector T1 = D1.Tangent * std::max(1.0f, D1.SpawnDelta);
			for (int32 InterpIndex = 1; InterpIndex <= InterpCount; ++InterpIndex)
			{
				const float Alpha = static_cast<float>(InterpIndex) / static_cast<float>(InterpCount);
				FRibbonBuildPoint Point;
				Point.Position = CubicInterp(P0.Location, T0, P1.Location, T1, Alpha);
				Point.Up = FVector::Lerp(D0.Up, D1.Up, Alpha).GetSafeNormal(1.0e-6f, FVector::ZAxisVector);
				Point.Width = FVector::Lerp(FVector(P0.Size.X * D0.PinchScaleFactor, 0.0f, 0.0f), FVector(P1.Size.X * D1.PinchScaleFactor, 0.0f, 0.0f), Alpha).X;
				Point.U = D0.TiledU + (D1.TiledU - D0.TiledU) * Alpha;
				Point.Color = P0.Color;
				Point.RelativeTime = P0.RelativeTime + (P1.RelativeTime - P0.RelativeTime) * Alpha;
				Points.push_back(Point);
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

	FVector SampleBeamNoiseOffset(
		const FDynamicBeam2EmitterReplayData& Source,
		const FBaseParticle& Particle,
		float Alpha,
		const FVector* NoisePoints,
		const FVector* NextNoisePoints)
	{
		if (!NoisePoints || Source.Frequency <= 0)
		{
			return FVector::ZeroVector;
		}

		float NoiseDistanceScale = 1.0f;
		if (Source.NoiseDistanceScaleOffset >= 0)
		{
			const float* NoiseDistanceScalePayload =
				reinterpret_cast<const float*>(reinterpret_cast<const uint8*>(&Particle) + Source.NoiseDistanceScaleOffset);
			if (NoiseDistanceScalePayload)
			{
				NoiseDistanceScale = *NoiseDistanceScalePayload;
			}
		}

		const float NoisePosition = Clamp01(Alpha) * static_cast<float>(Source.Frequency);
		const int32 NoiseIndex = std::max(0, std::min(Source.Frequency, static_cast<int32>(std::floor(NoisePosition))));
		const int32 NextIndex = std::max(0, std::min(Source.Frequency, NoiseIndex + 1));
		const float NoiseAlpha = NoisePosition - static_cast<float>(NoiseIndex);

		const FVector NoiseA = ReadBeamNoisePoint(Source, Particle, NoisePoints, NextNoisePoints, NoiseIndex);
		const FVector NoiseB = ReadBeamNoisePoint(Source, Particle, NoisePoints, NextNoisePoints, NextIndex);
		return FVector::Lerp(NoiseA, NoiseB, NoiseAlpha) * Source.NoiseRangeScale * NoiseDistanceScale;
	}

	void AppendBeamSheet(
		const FDynamicBeam2EmitterReplayData& Source,
		const FBaseParticle& Particle,
		const TArray<FVector>& Centers,
		const TArray<float>& Tapers,
		int32 SheetIndex,
		TArray<FParticleBeamTrailVertex>& Vertices,
		TArray<uint32>& Indices)
	{
		if (Centers.size() < 2)
		{
			return;
		}

		const FVector Direction = (Centers.back() - Centers.front()).GetSafeNormal(1.0e-6f, FVector::XAxisVector);
		const FVector SheetUp = MakeSheetUp(Direction, SheetIndex, std::max(1, Source.Sheets));
		const uint32 BaseIndex = static_cast<uint32>(Vertices.size());

		for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Centers.size()); ++PointIndex)
		{
			const float Alpha = Centers.size() > 1 ? static_cast<float>(PointIndex) / static_cast<float>(Centers.size() - 1) : 0.0f;
			const float Width = std::max(0.0f, Particle.Size.X) * (PointIndex < static_cast<int32>(Tapers.size()) ? Tapers[PointIndex] : 1.0f);
			const FVector Offset = SheetUp * (Width * 0.5f);

			FParticleBeamTrailVertex Left;
			Left.Position = Centers[PointIndex] - Offset;
			Left.OldPosition = Particle.OldLocation;
			Left.RelativeTime = Particle.RelativeTime;
			Left.ParticleId = static_cast<float>(PointIndex);
			Left.Size = FVector2(Particle.Size.X, Particle.Size.Y);
			Left.Rotation = Particle.Rotation;
			Left.SubImageIndex = 0.0f;
			Left.Color = Particle.Color;
			Left.Tex_U = Alpha * static_cast<float>(std::max(1, Source.TextureTile));
			Left.Tex_V = 0.0f;
			Left.Tex_U2 = Left.Tex_U;
			Left.Tex_V2 = 0.0f;

			FParticleBeamTrailVertex Right = Left;
			Right.Position = Centers[PointIndex] + Offset;
			Right.Tex_V = 1.0f;
			Right.Tex_V2 = 1.0f;

			Vertices.push_back(Left);
			Vertices.push_back(Right);
		}

		for (uint32 Segment = 0; Segment + 1 < static_cast<uint32>(Centers.size()); ++Segment)
		{
			const uint32 I0 = BaseIndex + Segment * 2;
			const uint32 I1 = I0 + 1;
			const uint32 I2 = I0 + 2;
			const uint32 I3 = I0 + 3;
			Indices.push_back(I0);
			Indices.push_back(I2);
			Indices.push_back(I1);
			Indices.push_back(I1);
			Indices.push_back(I2);
			Indices.push_back(I3);
		}
	}

	void BuildBeamCenters(
		const FDynamicBeam2EmitterReplayData& Source,
		const FBaseParticle& Particle,
		const FBeam2TypeDataPayload& BeamData,
		bool bUseNoise,
		bool bUseInterpolatedPath,
		TArray<FVector>& OutCenters,
		TArray<float>& OutTapers)
	{
		OutCenters.clear();
		OutTapers.clear();

		const bool bLocked = BEAM2_TYPEDATA_LOCKED(BeamData.Lock_Max_NumNoisePoints);
		const FVector EndPoint = bLocked ? BeamData.TargetPoint : Particle.Location;
		const int32 Steps = std::max(1, BeamData.Steps);
		const int32 PointCount = std::max(Steps + 1, BeamData.TriangleCount > 0 ? (BeamData.TriangleCount / 2) + 1 : 2);

		const FVector* InterpPoints = Source.InterpolatedPointsOffset >= 0
			? reinterpret_cast<const FVector*>(reinterpret_cast<const uint8*>(&Particle) + Source.InterpolatedPointsOffset)
			: nullptr;
		const FVector* NoisePoints = Source.TargetNoisePointsOffset >= 0
			? reinterpret_cast<const FVector*>(reinterpret_cast<const uint8*>(&Particle) + Source.TargetNoisePointsOffset)
			: nullptr;
		const FVector* NextNoisePoints = Source.NextNoisePointsOffset >= 0
			? reinterpret_cast<const FVector*>(reinterpret_cast<const uint8*>(&Particle) + Source.NextNoisePointsOffset)
			: nullptr;

		TArray<FVector> InterpPath;
		if (bUseInterpolatedPath && InterpPoints && Source.InterpolationPoints > 0)
		{
			InterpPath.reserve(Source.InterpolationPoints + 2);
			InterpPath.push_back(BeamData.SourcePoint);
			const int32 InterpCount = std::min(Source.InterpolationPoints, std::max(0, BeamData.InterpolationSteps));
			for (int32 InterpIndex = 0; InterpIndex < InterpCount; ++InterpIndex)
			{
				InterpPath.push_back(InterpPoints[InterpIndex]);
			}
			InterpPath.push_back(EndPoint);
		}

		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			const float Alpha = PointCount > 1 ? static_cast<float>(PointIndex) / static_cast<float>(PointCount - 1) : 0.0f;
			FVector Center = FVector::Lerp(BeamData.SourcePoint, EndPoint, Alpha);

			if (!InterpPath.empty())
			{
				const float PathPosition = Alpha * static_cast<float>(InterpPath.size() - 1);
				const int32 SegmentIndex = std::min(static_cast<int32>(InterpPath.size()) - 2, std::max(0, static_cast<int32>(std::floor(PathPosition))));
				const float SegmentAlpha = PathPosition - static_cast<float>(SegmentIndex);
				Center = FVector::Lerp(InterpPath[SegmentIndex], InterpPath[SegmentIndex + 1], SegmentAlpha);
			}
			else if (InterpPoints && PointIndex > 0 && PointIndex <= Source.InterpolationPoints)
			{
				Center = InterpPoints[PointIndex - 1];
				if (!bLocked)
				{
					Center = FVector::Lerp(BeamData.SourcePoint, Center, Clamp01(BeamData.TravelRatio));
				}
			}

			if (bUseNoise && NoisePoints && Source.Frequency > 0)
			{
				Center += SampleBeamNoiseOffset(Source, Particle, Alpha, NoisePoints, NextNoisePoints);
			}

			OutCenters.push_back(Center);
			OutTapers.push_back(ReadTaper(Source, &Particle, BeamData, PointIndex, PointCount));
	}
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

void FDynamicBeam2EmitterData::BuildMeshData()
{
    FBeamTrailCPUStaging& Staging = GetCPUStaging(this);
    Staging.Vertices.clear();
    Staging.Indices.clear();
    DoBufferFill();
}

void FDynamicBeam2EmitterData::DoBufferFill()
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
            FillData_InterpolatedNoise();
        }
        else
        {
            FillData_Noise();
        }
    }
    else
    {
        FillVertexData_NoNoise();
    }
}

int32 FDynamicBeam2EmitterData::FillIndexData()
{
	// UE original responsibility: build the beam strip index stream, including
	// sheets and degenerate joins. Jungle keeps the same logical point sequence
	// and sheet pass, then converts only the final output to triangle-list indices.
	return static_cast<int32>(GetCPUStaging(this).Indices.size());
}

int32 FDynamicBeam2EmitterData::FillVertexData_NoNoise()
{
	TArray<FVector> Centers;
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

		BuildBeamCenters(Source, *Particle, *BeamData, false, true, Centers, Tapers);
		for (int32 SheetIndex = 0; SheetIndex < std::max(1, Source.Sheets); ++SheetIndex)
		{
			AppendBeamSheet(Source, *Particle, Centers, Tapers, SheetIndex, Staging.Vertices, Staging.Indices);
		}
	}

	return static_cast<int32>(Staging.Vertices.size()) - InitialVertexCount;
}

int32 FDynamicBeam2EmitterData::FillData_Noise()
{
	TArray<FVector> Centers;
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

		BuildBeamCenters(Source, *Particle, *BeamData, true, false, Centers, Tapers);
		for (int32 SheetIndex = 0; SheetIndex < std::max(1, Source.Sheets); ++SheetIndex)
		{
			AppendBeamSheet(Source, *Particle, Centers, Tapers, SheetIndex, Staging.Vertices, Staging.Indices);
		}
	}

	return static_cast<int32>(Staging.Vertices.size()) - InitialVertexCount;
}

int32 FDynamicBeam2EmitterData::FillData_InterpolatedNoise()
{
	TArray<FVector> Centers;
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

		BuildBeamCenters(Source, *Particle, *BeamData, true, true, Centers, Tapers);
		for (int32 SheetIndex = 0; SheetIndex < std::max(1, Source.Sheets); ++SheetIndex)
		{
			AppendBeamSheet(Source, *Particle, Centers, Tapers, SheetIndex, Staging.Vertices, Staging.Indices);
		}
	}

	return static_cast<int32>(Staging.Vertices.size()) - InitialVertexCount;
}

void FDynamicTrailsEmitterData::DoBufferFill()
{
    // UE original responsibility:
    // FDynamicTrailsEmitterData::DoBufferFill checks async buffer inputs and calls
    // FillIndexData then FillVertexData. Jungle's CPU staging arrays stand in for
    // the missing FAsyncBufferFillData/RHI layer, and simulation payload is read-only here.
    FillIndexData();
    FillVertexData();
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
	TArray<FRibbonBuildPoint> Points;
	const int32 Sheets = std::max(1, Source.Sheets);
	uint32 ExpectedBaseVertex = 0;
	for (int32 ActiveIndex = 0; ActiveIndex < Source.ActiveParticleCount; ++ActiveIndex)
	{
		const uint16 DirectIndex = Source.DataContainer.ParticleIndices ? Source.DataContainer.ParticleIndices[ActiveIndex] : 0;
		const FBaseParticle* StartParticle = GetReplayParticle(Source, DirectIndex);
		const FRibbonTypeDataPayload* StartPayload = GetReplayPayload<FRibbonTypeDataPayload>(Source, StartParticle, Source.TrailDataOffset);
		if (!StartParticle || !StartPayload || !TRAIL_EMITTER_IS_HEAD(StartPayload->Flags))
		{
			continue;
		}

		BuildRibbonPointSequence(Source, StartParticle, StartPayload, Points);
		if (Points.size() < 2)
		{
			continue;
		}

		for (int32 SheetIndex = 0; SheetIndex < Sheets; ++SheetIndex)
		{
			const uint32 BaseIndex = ExpectedBaseVertex + static_cast<uint32>(SheetIndex * Points.size() * 2);
			for (uint32 Segment = 0; Segment + 1 < static_cast<uint32>(Points.size()); ++Segment)
			{
				const uint32 I0 = BaseIndex + Segment * 2;
				const uint32 I1 = I0 + 1;
				const uint32 I2 = I0 + 2;
				const uint32 I3 = I0 + 3;
				Staging.Indices.push_back(I0);
				Staging.Indices.push_back(I2);
				Staging.Indices.push_back(I1);
				Staging.Indices.push_back(I1);
				Staging.Indices.push_back(I2);
				Staging.Indices.push_back(I3);
			}
		}
		ExpectedBaseVertex += static_cast<uint32>(Points.size() * 2 * Sheets);
	}

	return static_cast<int32>(Staging.Indices.size()) / 3 - InitialTriangleCount;
}

int32 FDynamicTrailsEmitterData::FillVertexData()
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

void FDynamicRibbonEmitterData::BuildMeshData()
{
    FBeamTrailCPUStaging& Staging = GetCPUStaging(this);
    Staging.Vertices.clear();
    Staging.Indices.clear();
    DoBufferFill();
}

int32 FDynamicRibbonEmitterData::FillInterpolatedVertexData()
{
	FBeamTrailCPUStaging& Staging = GetCPUStaging(this);
	const int32 InitialVertexCount = static_cast<int32>(Staging.Vertices.size());
	const int32 Sheets = std::max(1, Source.Sheets);
	TArray<FRibbonBuildPoint> Points;

	for (int32 ActiveIndex = 0; ActiveIndex < Source.ActiveParticleCount; ++ActiveIndex)
	{
		const uint16 DirectIndex = Source.DataContainer.ParticleIndices ? Source.DataContainer.ParticleIndices[ActiveIndex] : 0;
		const FBaseParticle* StartParticle = GetReplayParticle(Source, DirectIndex);
		const FRibbonTypeDataPayload* StartPayload = GetReplayPayload<FRibbonTypeDataPayload>(Source, StartParticle, Source.TrailDataOffset);
		BuildRibbonPointSequence(Source, StartParticle, StartPayload, Points);
		if (Points.size() < 2)
		{
			continue;
		}

		const FVector TrailDirection = (Points.back().Position - Points.front().Position).GetSafeNormal(1.0e-6f, FVector::XAxisVector);
		for (int32 SheetIndex = 0; SheetIndex < Sheets; ++SheetIndex)
		{
			const uint32 BaseIndex = static_cast<uint32>(Staging.Vertices.size());
			for (int32 PointIndex = 0; PointIndex < static_cast<int32>(Points.size()); ++PointIndex)
			{
				const FRibbonBuildPoint& Point = Points[PointIndex];
				const FVector SheetUp = RotateAroundAxis(Point.Up, TrailDirection, Sheets > 1 ? PI * static_cast<float>(SheetIndex) / static_cast<float>(Sheets) : 0.0f);
				const FVector Offset = SheetUp.GetSafeNormal(1.0e-6f, FVector::ZAxisVector) * (std::max(0.0f, Point.Width) * 0.5f);

				FParticleBeamTrailVertex Left;
				Left.Position = Point.Position - Offset;
				Left.OldPosition = Point.Position;
				Left.RelativeTime = Point.RelativeTime;
				Left.ParticleId = static_cast<float>(PointIndex);
				Left.Size = FVector2(Point.Width, Point.Width);
				Left.Color = Point.Color;
				Left.Tex_U = Point.U;
				Left.Tex_V = 0.0f;
				Left.Tex_U2 = Point.U;
				Left.Tex_V2 = 0.0f;

				FParticleBeamTrailVertex Right = Left;
				Right.Position = Point.Position + Offset;
				Right.Tex_V = 1.0f;
				Right.Tex_V2 = 1.0f;

				Staging.Vertices.push_back(Left);
				Staging.Vertices.push_back(Right);
			}
		}
	}

	return static_cast<int32>(Staging.Vertices.size()) - InitialVertexCount;
}

int32 FDynamicRibbonEmitterData::FillVertexData()
{
	return FillInterpolatedVertexData();
}
