#pragma once

#include "StaticMesh.h"
#include "Geometry/Edge.h"
#include <queue>

struct FTopologicalVertex
{
	FVector Position;
	TArray<uint32> RenderVertices;
};

struct FCollapseCandidate
{
	FIndexEdge Edge;         
	FVector4 OptimalPos; 
	float Error = FLT_MAX;

	bool operator<(const FCollapseCandidate& Other) const
	{
		return Error > Other.Error; 
	}
};

class FStaticMeshSimplifier
{
public:
	static void BuildLODs(class UStaticMesh* TargetMesh);

private:
	FStaticMeshSimplifier(class UStaticMesh* InTargetMesh);

	/* Quadric Build Phase */
	void BuildMeshQuadrics();
	void BuildTopologicalVertices();
	void CalculateInitialQuadrics();
	void FindBoundaryEdges();
	void AddPlaneQuadric(uint32 VertIdx, const FVector& InNormal, float InD);

	/* Error Calculation Phase */
	static float CalculateVertexError(const FMatrix& Q, const FVector& V);
	FCollapseCandidate CalculateEdgeError(uint32 ia, uint32 ib);

	/* Simplification Phase */
	void SimplifyMesh();
	void UpdateRenderVertices(uint32 TopoIa, uint32 TopoIb, const FCollapseCandidate& Victim);
	void UpdateTriangles(uint32 TopoIa, uint32 TopoIb, TArray<uint32>& OutTopologicalIndices, int32& OutCurrentTriangles, TSet<uint32>& OutNewNeighbors);
	void BuildFinalIndices(const TArray<uint32>& TopologicalIndices, int32 CurrentTriangles);
	
	/* LOD Output Phase */
	void SaveCurrentStateAsLOD(int32 CurrentLOD, const TArray<uint32>& TopologicalIndices);

private:
	class UStaticMesh* TargetMesh = nullptr;
	FStaticMesh* MeshData = nullptr;

	// 임시 데이터 구조
	TArray<FTopologicalVertex> TopologicalVertices; 
	TMap<uint32, uint32> RenderToTopoMap;			

	TArray<FMatrix> Quadrics;					    
	TSet<FIndexEdge> Edges;						    
	TMap<FIndexEdge, int32> EdgeUsage;				
	TSet<uint32> BoundaryVertices;					
	TMap<uint32, TSet<uint32>> VertexToTriangleMap; 
};