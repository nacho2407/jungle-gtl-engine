#include "DynamicEmitterData.h"
#include "Particles/ParticleHelper.h"
#include <algorithm>

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

void FDynamicBeam2EmitterData::BuildMeshData()
{
    Vertices.clear();
    Indices.clear();
    DoBufferFill_Stub();
}

void FDynamicBeam2EmitterData::DoBufferFill_Stub()
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

    FillIndexData_Stub();

    if (Source.bLowFreqNoise_Enabled)
    {
        if (Source.InterpolationPoints > 0)
        {
            FillData_InterpolatedNoise_Stub();
        }
        else
        {
            FillData_Noise_Stub();
        }
    }
    else
    {
        FillVertexData_NoNoise_Stub();
    }
}

int32 FDynamicBeam2EmitterData::FillIndexData_Stub()
{
    // UE original responsibility:
    // Build the beam/trail strip index stream, including sheets and degenerate
    // joins. Jungle currently has only CPU staging arrays.
    // TODO(Cascade port): port UE FillIndexData semantics before RHI hookup.
    return 0;
}

int32 FDynamicBeam2EmitterData::FillVertexData_NoNoise_Stub()
{
    // UE original responsibility:
    // Fill beam vertices for the no-noise path using SourcePoint/TargetPoint,
    // interpolation points, taper, texture tiling, and sheet basis.
    // TODO(Cascade port): port UE FillVertexData_NoNoise semantics.
    return 0;
}

int32 FDynamicBeam2EmitterData::FillData_Noise_Stub()
{
    // UE original responsibility:
    // Fill beam vertices for low-frequency noise path using TargetNoisePoints,
    // NextNoisePoints, NoiseRate, NoiseDeltaTime, NoiseDistanceScale, and
    // NoiseTessellation.
    // TODO(Cascade port): port UE FillData_Noise semantics.
    return 0;
}

int32 FDynamicBeam2EmitterData::FillData_InterpolatedNoise_Stub()
{
    // UE original responsibility:
    // Fill beam vertices for interpolated + noise path. This is not equivalent
    // to a straight SourcePoint -> Particle.Location segment.
    // TODO(Cascade port): port UE FillData_InterpolatedNoise semantics.
    return 0;
}

void FDynamicRibbonEmitterData::BuildMeshData()
{
    Vertices.clear();
    Indices.clear();
    DoBufferFill_Stub();
}

void FDynamicRibbonEmitterData::DoBufferFill_Stub()
{
    // UE original responsibility:
    // FDynamicRibbonEmitterData walks the replay snapshot trail linked-list and
    // calls FillIndexData / FillVertexData variants according to tessellation,
    // render axis, sheets, and tangent interpolation.
    //
    // Missing Jungle foundation:
    // beam-trail vertex factory, render-axis adapter, exact trail index writer,
    // and RHI dynamic buffer fill.
    //
    // Keep this boundary. Do not replace it with a segment-quad simplification.

    FillIndexData_Stub();
    FillVertexData_Stub();
}

int32 FDynamicRibbonEmitterData::FillIndexData_Stub()
{
    // UE original responsibility:
    // Build the ribbon strip index stream from trail START/NEXT payload links,
    // including sheets and degenerate joins.
    // TODO(Cascade port): port UE ribbon FillIndexData semantics.
    return 0;
}

int32 FDynamicRibbonEmitterData::FillVertexData_Stub()
{
    // UE original responsibility:
    // Fill ribbon vertices from trail payload, RenderingInterpCount, Tangent,
    // Up, TiledU, PinchScaleFactor, RenderAxis, and SheetsPerTrail.
    // TODO(Cascade port): port UE ribbon FillVertexData semantics.
    return 0;
}

int32 FDynamicRibbonEmitterData::FillInterpolatedVertexData_Stub()
{
    // UE original responsibility:
    // Fill tessellated ribbon vertices with tangent/cubic interpolation between
    // particles. This must use the same RenderingInterpCount as
    // DetermineVertexAndTriangleCount.
    // TODO(Cascade port): port UE interpolated ribbon fill semantics.
    return 0;
}
