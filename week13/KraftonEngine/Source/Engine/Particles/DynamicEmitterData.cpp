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
            return DA > DB;  // back-to-front
        });
        break;

    case PSORTMODE_ViewProjDepth:
        std::sort(Indices, Indices + Count, [&](uint16 A, uint16 B)
        {
            const float DA = (GetParticle(A)->Location - SortCtx.CameraPosition).Dot(SortCtx.CameraForward);
            const float DB = (GetParticle(B)->Location - SortCtx.CameraPosition).Dot(SortCtx.CameraForward);
            return DA > DB;  // back-to-front
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

    // UE original responsibility:
    // FDynamicBeam2EmitterData reads FDynamicBeam2EmitterReplayData and copied particle
    // payload, then fills CPU/RHI vertex and index buffers with beam strip/noise/taper/sheet
    // rules.
    //
    // Missing Jungle foundation:
    // Cascade beam-trail vertex factory/RHI fill code and view-dependent sheet basis adapter.
    //
    // System to connect later:
    // Port UE ParticleSystemRender.cpp FillVertexData_NoNoise/FillData_Noise/
    // FillData_InterpolatedNoise and convert only final strip indices to Jungle staging.
}

void FDynamicRibbonEmitterData::BuildMeshData()
{
    Vertices.clear();
    Indices.clear();

    // UE original responsibility:
    // FDynamicRibbonEmitterData walks the replay snapshot's trail linked-list from START to
    // NEXT, evaluates FRibbonTypeDataPayload RenderingInterpCount/tangent/up/sheets, and fills
    // CPU/RHI vertex and index buffers.
    //
    // Missing Jungle foundation:
    // Cascade trail render fill path, render-axis adapter, and beam-trail vertex factory.
    //
    // System to connect later:
    // Port UE ParticleSystemRender.cpp FDynamicRibbonEmitterData::FillVertexData and
    // FillIndexData into this CPU staging layer without changing simulation payload.
}
