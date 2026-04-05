#pragma once
#include "CoreMinimal.h"

#include "Renderer/MeshData.h"
#include "Collapse.h"
#include <set>

namespace nanite
{
	class FCollapseQueue
	{
	public:
		FCollapseQueue(const FStaticMesh* InMesh, const TArray<FQuadric>& InQuadrics, const std::set<uint32>& InFixedVertices) :
			Mesh(InMesh), Quadrics(InQuadrics), FixedVertices(InFixedVertices)
		{
		}

		~FCollapseQueue() = default;

		const FCollapse& PickBest() const
		{
			return *CollapseSet.begin();
		}

		void Insert(const FEdge& E, int32 Phase = 0)
		{
			bool bFixA = FixedVertices.find(E.GetA()) != FixedVertices.end();
			bool bFixB = FixedVertices.find(E.GetB()) != FixedVertices.end();

			if (bFixA || bFixB) return;

			FCollapse Collapse;
			Collapse.Edge = E;
			Collapse.Quadric = Quadrics[E.GetA()] + Quadrics[E.GetB()];
			Collapse.Position = FQuadric::FindOptimalPosition(Collapse.Quadric, Mesh->Vertices[E.GetA()].Position, Mesh->Vertices[E.GetB()].Position, bFixA, bFixB);
			Collapse.Error = Collapse.Quadric.Evaluate(Collapse.Position);
			Collapse.Length = (Mesh->Vertices[E.GetA()].Position - Mesh->Vertices[E.GetB()].Position).Size();
			Collapse.bFixA = bFixA;
			Collapse.bFixB = bFixB;
			Collapse.Phase = Phase;

			Insert(Collapse);
		}

		void Insert(const FCollapse& C)
		{
			auto [iter, B] = CollapseSet.emplace(C);
			EdgeToCollpaseMap[C.Edge] = iter;
		}

		int32 Erase(const FEdge& E)
		{
			const auto MapIt = EdgeToCollpaseMap.find(E);
			if (MapIt != EdgeToCollpaseMap.end())
			{
				const auto CollapseIt = MapIt->second;
				const int32 Phase = CollapseIt->Phase;
				EdgeToCollpaseMap.erase(MapIt);
				CollapseSet.erase(CollapseIt);
				return Phase;
			}
			return 0;
		}

		int Erase(const FCollapse& C)
		{
			return Erase(C.Edge);
		}

		int Size() const { return static_cast<int>(CollapseSet.size()); }
		void Reserve(size_t numExpectedElements) { EdgeToCollpaseMap.reserve(numExpectedElements * 2 + 1); }
	private:
		std::set<FCollapse> CollapseSet;
		TMap<FEdge, std::set<FCollapse>::iterator> EdgeToCollpaseMap;

		const FStaticMesh* Mesh;
		const TArray<FQuadric>& Quadrics;
		const std::set<uint32>& FixedVertices;
	};
}
