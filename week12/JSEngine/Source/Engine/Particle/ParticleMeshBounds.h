#pragma once

#include "Asset/StaticMesh.h"
#include "Particle/ParticleTypes.h"

#include <algorithm>
#include <cmath>

namespace ParticleMeshBounds
{
	inline FVector GetSafeParticleScale(const FBaseParticle& Particle)
	{
		return FVector(
			std::max(std::fabs(Particle.Size.X), 0.001f),
			std::max(std::fabs(Particle.Size.Y), 0.001f),
			std::max(std::fabs(Particle.Size.Z), 0.001f));
	}

	inline FMatrix BuildInstanceTransform(
		const FDynamicEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FBaseParticle& Particle)
	{
		const FMatrix ParticleTransform = FMatrix::MakeTRS(
			Particle.Location,
			FMatrix::MakeRotationEuler(Particle.MeshRotation),
			GetSafeParticleScale(Particle));
		return ReplayData.CoordinateSpace == EParticleCoordinateSpace::Local
			? ParticleTransform * ComponentToWorld
			: ParticleTransform;
	}

	inline FAABB BuildConservativeWorldBounds(
		const FDynamicEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FAABB& MeshLocalBounds)
	{
		FAABB Bounds;
		if (!MeshLocalBounds.IsValid())
		{
			return Bounds;
		}

		for (int32 ActiveIndex = 0; ActiveIndex < ReplayData.ActiveParticleCount; ++ActiveIndex)
		{
			const FBaseParticle* Particle = ReplayData.GetParticleByActiveIndex(ActiveIndex);
			if (Particle == nullptr)
			{
				continue;
			}

			const FMatrix InstanceTransform = BuildInstanceTransform(ReplayData, ComponentToWorld, *Particle);
			Bounds.Merge(FAABB::TransformAABB(MeshLocalBounds, InstanceTransform));
		}

		return Bounds;
	}
}
