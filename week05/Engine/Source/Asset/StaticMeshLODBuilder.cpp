#include "Asset/StaticMeshLODBuilder.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <set>
#include <vector>

#include "Math/CollapseQueue.h"
#include "Renderer/MeshData.h"

namespace
{
	constexpr uint32 GInvalidIndex = (std::numeric_limits<uint32>::max)();

	struct FWorkingMeshState
	{
		FStaticMesh Mesh;
		TArray<bool> VertexRemoved;
		TArray<bool> TriangleRemoved;
	};

	int32 GetActiveTriangleCount(const FWorkingMeshState& State)
	{
		return static_cast<int32>(std::count(State.TriangleRemoved.begin(), State.TriangleRemoved.end(), false));
	}

	void BuildConnectivity(
		const FWorkingMeshState& State,
		TArray<FQuadric>& OutQuadrics,
		std::set<FEdge>& OutEdges,
		TMap<FEdge, int32>& OutEdgeUsage,
		std::vector<std::set<uint32>>& OutVertexToTriangles)
	{
		OutQuadrics.clear();
		OutQuadrics.resize(State.Mesh.Vertices.size());
		OutEdges.clear();
		OutEdgeUsage.clear();
		OutVertexToTriangles.assign(State.Mesh.Vertices.size(), {});

		for (uint32 TriIndex = 0; TriIndex < State.TriangleRemoved.size(); ++TriIndex)
		{
			if (State.TriangleRemoved[TriIndex])
			{
				continue;
			}

			const size_t IndexBase = static_cast<size_t>(TriIndex) * 3;
			if (IndexBase + 2 >= State.Mesh.Indices.size())
			{
				continue;
			}

			const uint32 I0 = State.Mesh.Indices[IndexBase];
			const uint32 I1 = State.Mesh.Indices[IndexBase + 1];
			const uint32 I2 = State.Mesh.Indices[IndexBase + 2];
			if (I0 == GInvalidIndex || I1 == GInvalidIndex || I2 == GInvalidIndex)
			{
				continue;
			}
			if (I0 == I1 || I1 == I2 || I0 == I2)
			{
				continue;
			}
			if (I0 >= State.Mesh.Vertices.size() || I1 >= State.Mesh.Vertices.size() || I2 >= State.Mesh.Vertices.size())
			{
				continue;
			}
			if (State.VertexRemoved[I0] || State.VertexRemoved[I1] || State.VertexRemoved[I2])
			{
				continue;
			}

			const FVertex& V0 = State.Mesh.Vertices[I0];
			const FVertex& V1 = State.Mesh.Vertices[I1];
			const FVertex& V2 = State.Mesh.Vertices[I2];

			const FVector Normal = FVector::CrossProduct(V1.Position - V0.Position, V2.Position - V0.Position).GetSafeNormal();
			if (Normal.IsZero())
			{
				continue;
			}

			const float D = -FVector::DotProduct(Normal, V0.Position);
			OutQuadrics[I0].AddPlane(Normal, D);
			OutQuadrics[I1].AddPlane(Normal, D);
			OutQuadrics[I2].AddPlane(Normal, D);

			const FEdge E0(I0, I1);
			const FEdge E1(I1, I2);
			const FEdge E2(I0, I2);

			OutEdges.insert(E0);
			OutEdges.insert(E1);
			OutEdges.insert(E2);

			OutEdgeUsage[E0]++;
			OutEdgeUsage[E1]++;
			OutEdgeUsage[E2]++;

			OutVertexToTriangles[I0].insert(TriIndex);
			OutVertexToTriangles[I1].insert(TriIndex);
			OutVertexToTriangles[I2].insert(TriIndex);
		}
	}

	std::set<uint32> CollectBoundaryVertices(const TMap<FEdge, int32>& EdgeUsage)
	{
		std::set<uint32> BoundaryVertices;
		for (const auto& [Edge, Count] : EdgeUsage)
		{
			if (Count == 1)
			{
				BoundaryVertices.insert(Edge.GetA());
				BoundaryVertices.insert(Edge.GetB());
			}
		}

		return BoundaryVertices;
	}

	bool WouldFlipTriangles(
		const FWorkingMeshState& State,
		uint32 KeepIdx,
		uint32 RemoveIdx,
		const FVector& NewPosition,
		const std::set<uint32>& UpdatedTriangles)
	{
		for (uint32 UpdatedTriIdx : UpdatedTriangles)
		{
			if (UpdatedTriIdx >= State.TriangleRemoved.size() || State.TriangleRemoved[UpdatedTriIdx])
			{
				continue;
			}

			const size_t IndexBase = static_cast<size_t>(UpdatedTriIdx) * 3;
			const uint32 OldI0 = State.Mesh.Indices[IndexBase];
			const uint32 OldI1 = State.Mesh.Indices[IndexBase + 1];
			const uint32 OldI2 = State.Mesh.Indices[IndexBase + 2];
			if (OldI0 == GInvalidIndex || OldI1 == GInvalidIndex || OldI2 == GInvalidIndex)
			{
				continue;
			}

			const uint32 NewI0 = (OldI0 == RemoveIdx) ? KeepIdx : OldI0;
			const uint32 NewI1 = (OldI1 == RemoveIdx) ? KeepIdx : OldI1;
			const uint32 NewI2 = (OldI2 == RemoveIdx) ? KeepIdx : OldI2;
			if (NewI0 == NewI1 || NewI1 == NewI2 || NewI0 == NewI2)
			{
				continue;
			}

			const FVector& OldP0 = State.Mesh.Vertices[OldI0].Position;
			const FVector& OldP1 = State.Mesh.Vertices[OldI1].Position;
			const FVector& OldP2 = State.Mesh.Vertices[OldI2].Position;
			const FVector OldNormal = FVector::CrossProduct(OldP1 - OldP0, OldP2 - OldP0).GetSafeNormal();

			const FVector NewP0 = (NewI0 == KeepIdx) ? NewPosition : State.Mesh.Vertices[NewI0].Position;
			const FVector NewP1 = (NewI1 == KeepIdx) ? NewPosition : State.Mesh.Vertices[NewI1].Position;
			const FVector NewP2 = (NewI2 == KeepIdx) ? NewPosition : State.Mesh.Vertices[NewI2].Position;
			const FVector NewNormal = FVector::CrossProduct(NewP1 - NewP0, NewP2 - NewP0).GetSafeNormal();

			if (!OldNormal.IsZero() && !NewNormal.IsZero() && FVector::DotProduct(OldNormal, NewNormal) < 1e-4f)
			{
				return true;
			}
		}

		return false;
	}

	void ApplyCollapse(FWorkingMeshState& State, uint32 KeepIdx, uint32 RemoveIdx, const FVector& NewPosition)
	{
		FVertex& KeepVertex = State.Mesh.Vertices[KeepIdx];
		const FVertex& RemoveVertex = State.Mesh.Vertices[RemoveIdx];

		KeepVertex.Position = NewPosition;
		KeepVertex.Color = FVector4(
			(KeepVertex.Color.X + RemoveVertex.Color.X) * 0.5f,
			(KeepVertex.Color.Y + RemoveVertex.Color.Y) * 0.5f,
			(KeepVertex.Color.Z + RemoveVertex.Color.Z) * 0.5f,
			(KeepVertex.Color.W + RemoveVertex.Color.W) * 0.5f);
		KeepVertex.Normal = (KeepVertex.Normal + RemoveVertex.Normal).GetSafeNormal();
		KeepVertex.UV = FVector2(
			(KeepVertex.UV.X + RemoveVertex.UV.X) * 0.5f,
			(KeepVertex.UV.Y + RemoveVertex.UV.Y) * 0.5f);

		for (uint32 TriIndex = 0; TriIndex < State.TriangleRemoved.size(); ++TriIndex)
		{
			if (State.TriangleRemoved[TriIndex])
			{
				continue;
			}

			const size_t IndexBase = static_cast<size_t>(TriIndex) * 3;
			uint32& I0 = State.Mesh.Indices[IndexBase];
			uint32& I1 = State.Mesh.Indices[IndexBase + 1];
			uint32& I2 = State.Mesh.Indices[IndexBase + 2];

			if (I0 == RemoveIdx) I0 = KeepIdx;
			if (I1 == RemoveIdx) I1 = KeepIdx;
			if (I2 == RemoveIdx) I2 = KeepIdx;

			if (I0 == I1 || I1 == I2 || I0 == I2)
			{
				I0 = GInvalidIndex;
				I1 = GInvalidIndex;
				I2 = GInvalidIndex;
				State.TriangleRemoved[TriIndex] = true;
			}
		}

		State.VertexRemoved[RemoveIdx] = true;
		State.Mesh.Vertices[RemoveIdx].Position = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	}

	std::unique_ptr<FStaticMesh> BuildLodMesh(const FWorkingMeshState& State, const FStaticMesh& SourceMesh)
	{
		auto Result = std::make_unique<FStaticMesh>();
		Result->Topology = SourceMesh.Topology;
		Result->PathFileName = SourceMesh.PathFileName;

		TMap<uint32, uint32> IndexMap;
		auto MapVertexIndex = [&](uint32 SourceIndex) -> uint32
		{
			const auto It = IndexMap.find(SourceIndex);
			if (It != IndexMap.end())
			{
				return It->second;
			}

			const uint32 NewIndex = static_cast<uint32>(Result->Vertices.size());
			IndexMap.emplace(SourceIndex, NewIndex);
			Result->Vertices.push_back(State.Mesh.Vertices[SourceIndex]);
			return NewIndex;
		};

		auto AppendTriangle = [&](uint32 I0, uint32 I1, uint32 I2)
		{
			Result->Indices.push_back(MapVertexIndex(I0));
			Result->Indices.push_back(MapVertexIndex(I1));
			Result->Indices.push_back(MapVertexIndex(I2));
		};

		if (SourceMesh.Sections.empty())
		{
			for (uint32 TriIndex = 0; TriIndex < State.TriangleRemoved.size(); ++TriIndex)
			{
				if (State.TriangleRemoved[TriIndex])
				{
					continue;
				}

				const size_t IndexBase = static_cast<size_t>(TriIndex) * 3;
				const uint32 I0 = State.Mesh.Indices[IndexBase];
				const uint32 I1 = State.Mesh.Indices[IndexBase + 1];
				const uint32 I2 = State.Mesh.Indices[IndexBase + 2];
				if (I0 == GInvalidIndex || I1 == GInvalidIndex || I2 == GInvalidIndex)
				{
					continue;
				}
				if (I0 == I1 || I1 == I2 || I0 == I2)
				{
					continue;
				}

				AppendTriangle(I0, I1, I2);
			}
		}
		else
		{
			Result->Sections.reserve(SourceMesh.Sections.size());
			for (const FMeshSection& SourceSection : SourceMesh.Sections)
			{
				FMeshSection NewSection = SourceSection;
				NewSection.StartIndex = static_cast<uint32>(Result->Indices.size());
				NewSection.IndexCount = 0;

				const uint32 SectionEnd = (std::min)(
					SourceSection.StartIndex + SourceSection.IndexCount,
					static_cast<uint32>(State.Mesh.Indices.size()));

				for (uint32 IndexStart = SourceSection.StartIndex; IndexStart + 2 < SectionEnd; IndexStart += 3)
				{
					const uint32 TriIndex = IndexStart / 3;
					if (TriIndex >= State.TriangleRemoved.size() || State.TriangleRemoved[TriIndex])
					{
						continue;
					}

					const uint32 I0 = State.Mesh.Indices[IndexStart];
					const uint32 I1 = State.Mesh.Indices[IndexStart + 1];
					const uint32 I2 = State.Mesh.Indices[IndexStart + 2];
					if (I0 == GInvalidIndex || I1 == GInvalidIndex || I2 == GInvalidIndex)
					{
						continue;
					}
					if (I0 == I1 || I1 == I2 || I0 == I2)
					{
						continue;
					}

					AppendTriangle(I0, I1, I2);
					NewSection.IndexCount += 3;
				}

				Result->Sections.push_back(NewSection);
			}
		}

		Result->UpdateLocalBound();
		Result->bIsDirty = true;
		return Result;
	}
}

void FStaticMeshLODBuilder::BuildLODs(UStaticMesh& Asset, const FStaticMeshLODSettings& Settings)
{
	Asset.ClearLODs();

	FStaticMesh* SourceMesh = Asset.GetRenderData();
	if (!SourceMesh || SourceMesh->Topology != EMeshTopology::EMT_TriangleList)
	{
		return;
	}

	const int32 OriginalTriCount = static_cast<int32>(SourceMesh->Indices.size() / 3);
	if (OriginalTriCount <= 0)
	{
		return;
	}

	const int32 NumLODs = (std::max)(Settings.NumLODs, 0);
	if (NumLODs == 0)
	{
		return;
	}

	FWorkingMeshState State = {};
	State.Mesh.Topology = SourceMesh->Topology;
	State.Mesh.Vertices = SourceMesh->Vertices;
	State.Mesh.Indices = SourceMesh->Indices;
	State.Mesh.Sections = SourceMesh->Sections;
	State.Mesh.PathFileName = SourceMesh->PathFileName;
	State.VertexRemoved.resize(SourceMesh->Vertices.size(), false);
	State.TriangleRemoved.resize(OriginalTriCount, false);

	int32 PreviousTriCount = OriginalTriCount;
	for (int32 LodLevel = 1; LodLevel <= NumLODs; ++LodLevel)
	{
		const float ReductionFraction = (std::clamp)(Settings.TriangleReductionStep * static_cast<float>(LodLevel), 0.0f, 0.95f);
		const int32 TargetTriCount = (std::max)(1, static_cast<int32>(std::floor(static_cast<float>(OriginalTriCount) * (1.0f - ReductionFraction))));

		bool bCollapsedAnyTriangle = false;
		int32 CurrentTriCount = GetActiveTriangleCount(State);
		while (CurrentTriCount > TargetTriCount)
		{
			TArray<FQuadric> Quadrics;
			std::set<FEdge> Edges;
			TMap<FEdge, int32> EdgeUsage;
			std::vector<std::set<uint32>> VertexToTriangles;
			BuildConnectivity(State, Quadrics, Edges, EdgeUsage, VertexToTriangles);
			if (Edges.empty())
			{
				break;
			}

			const std::set<uint32> BoundaryVertexIndices = CollectBoundaryVertices(EdgeUsage);
			nanite::FCollapseQueue CollapseQueue(&State.Mesh, Quadrics, BoundaryVertexIndices);
			CollapseQueue.Reserve(Edges.size());
			for (const FEdge& Edge : Edges)
			{
				CollapseQueue.Insert(Edge);
			}

			bool bCollapsedThisIteration = false;
			while (CollapseQueue.Size() > 0)
			{
				const FCollapse BestCandidate = CollapseQueue.PickBest();
				CollapseQueue.Erase(BestCandidate);

				uint32 KeepIdx = BestCandidate.Edge.GetA();
				uint32 RemoveIdx = BestCandidate.Edge.GetB();
				if (BestCandidate.bFixB)
				{
					KeepIdx = BestCandidate.Edge.GetB();
					RemoveIdx = BestCandidate.Edge.GetA();
				}

				if (KeepIdx >= VertexToTriangles.size() || RemoveIdx >= VertexToTriangles.size())
				{
					continue;
				}
				if (State.VertexRemoved[KeepIdx] || State.VertexRemoved[RemoveIdx])
				{
					continue;
				}

				const std::set<uint32>& TrisWithKeep = VertexToTriangles[KeepIdx];
				const std::set<uint32>& TrisWithRemove = VertexToTriangles[RemoveIdx];

				std::set<uint32> RemovedTriangles;
				std::set_intersection(
					TrisWithKeep.begin(), TrisWithKeep.end(),
					TrisWithRemove.begin(), TrisWithRemove.end(),
					std::inserter(RemovedTriangles, RemovedTriangles.begin()));

				if (RemovedTriangles.size() != 2)
				{
					continue;
				}

				std::set<uint32> UpdatedTrianglesTmp;
				std::set_union(
					TrisWithKeep.begin(), TrisWithKeep.end(),
					TrisWithRemove.begin(), TrisWithRemove.end(),
					std::inserter(UpdatedTrianglesTmp, UpdatedTrianglesTmp.begin()));

				std::set<uint32> UpdatedTriangles;
				std::set_difference(
					UpdatedTrianglesTmp.begin(), UpdatedTrianglesTmp.end(),
					RemovedTriangles.begin(), RemovedTriangles.end(),
					std::inserter(UpdatedTriangles, UpdatedTriangles.begin()));

				if (WouldFlipTriangles(State, KeepIdx, RemoveIdx, BestCandidate.Position, UpdatedTriangles))
				{
					continue;
				}

				ApplyCollapse(State, KeepIdx, RemoveIdx, BestCandidate.Position);
				CurrentTriCount = GetActiveTriangleCount(State);
				bCollapsedThisIteration = true;
				bCollapsedAnyTriangle = true;
				break;
			}

			if (!bCollapsedThisIteration)
			{
				break;
			}
		}

		const int32 BuiltTriCount = GetActiveTriangleCount(State);
		if (!bCollapsedAnyTriangle || BuiltTriCount >= PreviousTriCount)
		{
			break;
		}

		std::unique_ptr<FStaticMesh> LodMesh = BuildLodMesh(State, *SourceMesh);
		if (!LodMesh || LodMesh->Indices.empty())
		{
			break;
		}

		Asset.AddLOD(std::move(LodMesh), Settings.DistanceStep * static_cast<float>(LodLevel));
		PreviousTriCount = BuiltTriCount;
	}
}
