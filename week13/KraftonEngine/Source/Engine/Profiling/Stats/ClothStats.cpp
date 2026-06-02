#include "ClothStats.h"

#if STATS
uint32 FClothStats::ComponentCount = 0;
uint64 FClothStats::ParticleCount = 0;
uint64 FClothStats::IndexCount = 0;
uint64 FClothStats::PinnedCount = 0;
uint64 FClothStats::CollisionPrimitiveCount = 0;
uint64 FClothStats::SimulationStepCount = 0;
uint32 FClothStats::VertexUploadCount = 0;
double FClothStats::SimulationTimeMs = 0.0;

void FClothStats::ResetSimulation()
{
	ComponentCount = 0;
	ParticleCount = 0;
	IndexCount = 0;
	PinnedCount = 0;
	CollisionPrimitiveCount = 0;
	SimulationStepCount = 0;
	SimulationTimeMs = 0.0;
}

void FClothStats::ResetRender()
{
	VertexUploadCount = 0;
}

void FClothStats::RecordComponent(
	uint32 InParticleCount,
	uint32 InIndexCount,
	uint32 InPinnedCount,
	uint32 InCollisionPrimitiveCount,
	uint32 InSimulationStepCount,
	double InSimulationTimeMs)
{
	++ComponentCount;
	ParticleCount += InParticleCount;
	IndexCount += InIndexCount;
	PinnedCount += InPinnedCount;
	CollisionPrimitiveCount += InCollisionPrimitiveCount;
	SimulationStepCount += InSimulationStepCount;
	SimulationTimeMs += InSimulationTimeMs;
}

void FClothStats::AddVertexUpload()
{
	++VertexUploadCount;
}
#endif
