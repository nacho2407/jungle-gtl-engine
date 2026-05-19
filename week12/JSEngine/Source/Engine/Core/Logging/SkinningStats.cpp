#include "Core/Logging/SkinningStats.h"

#include "Math/Matrix.h"

#include <Windows.h>

namespace
{
	double CounterToMs(long long StartCounter, long long EndCounter)
	{
		LARGE_INTEGER Frequency;
		QueryPerformanceFrequency(&Frequency);
		return static_cast<double>(EndCounter - StartCounter) * 1000.0 /
			static_cast<double>(Frequency.QuadPart);
	}
}

void FSkinningStats::TakeSnapshot()
{
	Snapshot = Current;
	Current = FSkinningStatsFrame();
}

void FSkinningStats::AddCPUSkinnedVertexBufferUpload(double Ms, uint64 Bytes)
{
	Current.CPUSkinnedVertexBufferUploadMs += Ms;
	Current.CPUSkinnedVertexBufferUploadBytes += Bytes;
}

void FSkinningStats::AddVisibleSkinnedMesh(
	uint64 VertexCount,
	uint32 BoneCount,
	double AvgInfluence,
	bool bUsesGPUSkinning)
{
	Current.VisibleSkinnedMeshCount++;
	Current.VisibleSkinnedVertexCount += VertexCount;
	Current.TotalBoneCount += BoneCount;
	Current.TotalBoneInfluenceCount += AvgInfluence * static_cast<double>(VertexCount);
	Current.BoneInfluenceVertexCount += VertexCount;

	if (bUsesGPUSkinning)
	{
		Current.VisibleGPUSkinnedMeshCount++;
	}
	else
	{
		Current.VisibleCPUSkinnedMeshCount++;
	}
}

void FSkinningStats::AddGPUBoneMatrixUpload(uint32 UploadedBoneMatrixCount, uint64 UploadedBytes)
{
	Current.GPUBoneMatrixUploadCount += UploadedBoneMatrixCount;
	Current.GPUBoneMatrixUploadBytes += UploadedBytes;
}

void FSkinningStats::AddGPUSkinnedDraw(uint64 WorkVertexCount, double AvgInfluence)
{
	Current.GPUSkinnedDrawPassCount++;
	Current.EstimatedGPUSkinningInfluenceWork += static_cast<double>(WorkVertexCount) * AvgInfluence;
}

FSkinningScopedTimer::FSkinningScopedTimer(RecordFunc InRecordFunc)
	: Record(InRecordFunc)
{
	LARGE_INTEGER Counter;
	QueryPerformanceCounter(&Counter);
	StartCounter = Counter.QuadPart;
}

FSkinningScopedTimer::~FSkinningScopedTimer()
{
	if (!Record)
	{
		return;
	}

	LARGE_INTEGER Counter;
	QueryPerformanceCounter(&Counter);
	(FSkinningStats::Get().*Record)(CounterToMs(StartCounter, Counter.QuadPart));
}
