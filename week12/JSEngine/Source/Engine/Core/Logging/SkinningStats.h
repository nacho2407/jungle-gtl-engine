#pragma once

#include "Core/CoreMinimal.h"
#include "Core/Singleton.h"

struct FSkinningStatsFrame
{
	double CPUFrameTimeMs = 0.0;
	double GPUFrameTimeMs = 0.0;

	double CPUAnimationUpdateMs = 0.0;
	double CPUPoseBuildMs = 0.0;
	double CPUSkinningMs = 0.0;
	double CPUSkinnedVertexBufferUploadMs = 0.0;
	uint64 CPUSkinnedVertexBufferUploadBytes = 0;

	uint32 VisibleSkinnedMeshCount = 0;
	uint32 VisibleCPUSkinnedMeshCount = 0;
	uint32 VisibleGPUSkinnedMeshCount = 0;
	uint64 VisibleSkinnedVertexCount = 0;
	uint64 TotalBoneCount = 0;
	double TotalBoneInfluenceCount = 0.0;
	uint64 BoneInfluenceVertexCount = 0;

	uint64 GPUBoneMatrixCount = 0;
	// Actual bone matrix payload uploaded into GPU constant buffers this frame.
	uint64 GPUBoneMatrixPayloadBytes = 0;
	uint32 GPUSkinnedDrawPassCount = 0;
	double EstimatedGPUSkinningInfluenceWork = 0.0;

	double GetAvgBoneInfluencePerVertex() const
	{
		return BoneInfluenceVertexCount > 0
			? TotalBoneInfluenceCount / static_cast<double>(BoneInfluenceVertexCount)
			: 0.0;
	}
};

class FSkinningStats : public TSingleton<FSkinningStats>
{
	friend class TSingleton<FSkinningStats>;

public:
	void TakeSnapshot();
	const FSkinningStatsFrame& GetSnapshot() const { return Snapshot; }

	void RecordCPUFrameTime(double Ms) { Current.CPUFrameTimeMs = Ms; }
	void RecordGPUFrameTime(double Ms) { Current.GPUFrameTimeMs = Ms; }
	void AddCPUAnimationUpdate(double Ms) { Current.CPUAnimationUpdateMs += Ms; }
	void AddCPUPoseBuild(double Ms) { Current.CPUPoseBuildMs += Ms; }
	void AddCPUSkinning(double Ms) { Current.CPUSkinningMs += Ms; }
	void AddCPUSkinnedVertexBufferUpload(double Ms, uint64 Bytes);
	void AddVisibleSkinnedMesh(
		uint64 VertexCount,
		uint32 BoneCount,
		double AvgInfluence,
		bool bUsesGPUSkinning,
		uint32 UploadedBoneMatrixCount);
	void AddGPUBoneMatrixUpload(uint32 UploadedBoneMatrixCount);
	void AddGPUSkinnedDraw(uint64 WorkVertexCount, double AvgInfluence);

private:
	FSkinningStats() = default;
	~FSkinningStats() = default;

	FSkinningStatsFrame Current;
	FSkinningStatsFrame Snapshot;
};

class FSkinningScopedTimer
{
public:
	typedef void (FSkinningStats::*RecordFunc)(double);

	FSkinningScopedTimer(RecordFunc InRecordFunc);
	~FSkinningScopedTimer();

private:
	RecordFunc Record = nullptr;
	long long StartCounter = 0;
};

#define SKINNING_STAT_CONCAT2(a, b) a##b
#define SKINNING_STAT_CONCAT(a, b) SKINNING_STAT_CONCAT2(a, b)
#define SKINNING_SCOPE_MS(RecordFunc) FSkinningScopedTimer SKINNING_STAT_CONCAT(_SkinningScopedTimer_, __COUNTER__)(RecordFunc)
