#pragma once

#include "Core/Types/CoreTypes.h"
#include "Profiling/Stats/Stats.h"

#if STATS
/**
 * @brief cloth overlay 표시용 frame stat 집계값
 */
struct FClothStats
{
	static uint32 ComponentCount;
	static uint64 ParticleCount;
	static uint64 IndexCount;
	static uint64 PinnedCount;
	static uint64 CollisionPrimitiveCount;
	static uint64 SimulationStepCount;
	static uint32 VertexUploadCount;
	static double SimulationTimeMs;

	/**
	 * @brief simulation tick 기반 cloth stat을 초기화합니다
	 */
	static void ResetSimulation();

	/**
	 * @brief render upload 기반 cloth stat을 초기화합니다
	 */
	static void ResetRender();

	/**
	 * @brief cloth component simulation 결과를 현재 frame stat에 누적합니다
	 *
	 * @param InParticleCount component의 simulation particle 수
	 *
	 * @param InIndexCount component의 render index 수
	 *
	 * @param InPinnedCount component의 hard pin particle 수
	 *
	 * @param InCollisionPrimitiveCount component의 collision primitive 수
	 *
	 * @param InSimulationStepCount component가 최근 tick에서 소비한 fixed step 수
	 *
	 * @param InSimulationTimeMs component simulation tick에 걸린 cpu 시간
	 */
	static void RecordComponent(
		uint32 InParticleCount,
		uint32 InIndexCount,
		uint32 InPinnedCount,
		uint32 InCollisionPrimitiveCount,
		uint32 InSimulationStepCount,
		double InSimulationTimeMs);

	/**
	 * @brief cloth dynamic vertex buffer upload 성공 횟수를 증가시킵니다
	 */
	static void AddVertexUpload();
};

#define CLOTH_STATS_RESET_SIMULATION() FClothStats::ResetSimulation()
#define CLOTH_STATS_RESET_RENDER()     FClothStats::ResetRender()
#define CLOTH_STATS_RECORD_COMPONENT(ParticleCount, IndexCount, PinnedCount, CollisionPrimitiveCount, SimulationStepCount, SimulationTimeMs) \
	FClothStats::RecordComponent((ParticleCount), (IndexCount), (PinnedCount), (CollisionPrimitiveCount), (SimulationStepCount), (SimulationTimeMs))
#define CLOTH_STATS_ADD_VERTEX_UPLOAD() FClothStats::AddVertexUpload()
#else
#define CLOTH_STATS_RESET_SIMULATION() ((void)0)
#define CLOTH_STATS_RESET_RENDER()     ((void)0)
#define CLOTH_STATS_RECORD_COMPONENT(ParticleCount, IndexCount, PinnedCount, CollisionPrimitiveCount, SimulationStepCount, SimulationTimeMs) ((void)0)
#define CLOTH_STATS_ADD_VERTEX_UPLOAD() ((void)0)
#endif
