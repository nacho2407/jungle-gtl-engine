#include "Mesh/MeshSimplifier.h"

#include <algorithm>
#include <set>

FSimplifiedMesh FMeshSimplifier::Simplify(
	const TArray<FNormalVertex>& InVertices,
	const TArray<uint32>& InIndices,
	const TArray<FStaticMeshSection>& InSections,
	float TargetRatio)
{
	FSimplifiedMesh Result;

	if (TargetRatio >= 1.0f || InIndices.size() < 3 || InVertices.empty())
	{
		Result.Vertices = InVertices;
		Result.Indices = InIndices;
		Result.Sections = InSections;
		return Result;
	}

	const uint32 NumVerts = static_cast<uint32>(InVertices.size());
	const uint32 TargetVerts = static_cast<uint32>(NumVerts * TargetRatio);
	if (TargetVerts < 3)
	{
		Result.Vertices = InVertices;
		Result.Indices = InIndices;
		Result.Sections = InSections;
		return Result;
	}

	// ── Union-Find (path compression) ──
	TArray<uint32> Parent(NumVerts);
	for (uint32 i = 0; i < NumVerts; ++i) Parent[i] = i;

	auto Find = [&Parent](uint32 x) -> uint32 {
		while (Parent[x] != x) { Parent[x] = Parent[Parent[x]]; x = Parent[x]; }
		return x;
	};

	// ── Build edge list from triangles ──
	struct Edge { uint32 V0, V1; float Cost; };
	TArray<Edge> Edges;

	std::set<uint64> EdgeSet;
	const uint32 NumIndices = static_cast<uint32>(InIndices.size());
	for (uint32 i = 0; i < NumIndices; i += 3)
	{
		uint32 Tri[3] = { InIndices[i], InIndices[i + 1], InIndices[i + 2] };
		for (int j = 0; j < 3; ++j)
		{
			uint32 A = Tri[j], B = Tri[(j + 1) % 3];
			if (A > B) std::swap(A, B);
			uint64 Key = (static_cast<uint64>(A) << 32) | B;
			if (EdgeSet.insert(Key).second)
			{
				float Len = FVector::Distance(InVertices[A].pos, InVertices[B].pos);
				Edges.push_back({ A, B, Len });
			}
		}
	}

	// ── Sort edges by cost (shortest first) ──
	std::sort(Edges.begin(), Edges.end(),
		[](const Edge& A, const Edge& B) { return A.Cost < B.Cost; });

	// ── Collapse edges until target vertex count ──
	// For a closed triangle mesh: F ≈ 2V, so reducing V by ratio ≈ reduces F by ratio
	uint32 CurrentVerts = NumVerts;
	for (const Edge& E : Edges)
	{
		if (CurrentVerts <= TargetVerts) break;

		uint32 RA = Find(E.V0);
		uint32 RB = Find(E.V1);
		if (RA == RB) continue;

		Parent[RB] = RA;
		CurrentVerts--;
	}

	// ── Build vertex remap: old index → new compact index ──
	TArray<uint32> Remap(NumVerts, UINT32_MAX);
	TArray<FNormalVertex> NewVerts;
	NewVerts.reserve(CurrentVerts);

	for (uint32 i = 0; i < NumVerts; ++i)
	{
		uint32 Root = Find(i);
		if (Remap[Root] == UINT32_MAX)
		{
			Remap[Root] = static_cast<uint32>(NewVerts.size());
			NewVerts.push_back(InVertices[Root]);
		}
		Remap[i] = Remap[Root];
	}

	// ── Map each original triangle to its section ──
	const uint32 NumTris = NumIndices / 3;
	const uint32 NumSections = static_cast<uint32>(InSections.size());
	TArray<uint32> TriSection(NumTris, 0);
	for (uint32 s = 0; s < NumSections; ++s)
	{
		uint32 FirstTri = InSections[s].FirstIndex / 3;
		for (uint32 t = 0; t < InSections[s].NumTriangles; ++t)
		{
			if (FirstTri + t < NumTris)
				TriSection[FirstTri + t] = s;
		}
	}

	// ── Remap indices, remove degenerates, group by section ──
	TArray<TArray<uint32>> SectionIndices(NumSections);
	for (uint32 i = 0; i < NumIndices; i += 3)
	{
		uint32 A = Remap[InIndices[i]];
		uint32 B = Remap[InIndices[i + 1]];
		uint32 C = Remap[InIndices[i + 2]];

		if (A == B || B == C || A == C) continue;

		uint32 Sec = TriSection[i / 3];
		SectionIndices[Sec].push_back(A);
		SectionIndices[Sec].push_back(B);
		SectionIndices[Sec].push_back(C);
	}

	// ── Build output ──
	TArray<uint32> NewIndices;
	TArray<FStaticMeshSection> NewSections;

	for (uint32 s = 0; s < NumSections; ++s)
	{
		if (SectionIndices[s].empty()) continue;

		FStaticMeshSection Section;
		Section.MaterialIndex = InSections[s].MaterialIndex;
		Section.MaterialSlotName = InSections[s].MaterialSlotName;
		Section.FirstIndex = static_cast<uint32>(NewIndices.size());
		Section.NumTriangles = static_cast<uint32>(SectionIndices[s].size()) / 3;

		NewIndices.insert(NewIndices.end(), SectionIndices[s].begin(), SectionIndices[s].end());
		NewSections.push_back(Section);
	}

	Result.Vertices = std::move(NewVerts);
	Result.Indices = std::move(NewIndices);
	Result.Sections = std::move(NewSections);
	return Result;
}
