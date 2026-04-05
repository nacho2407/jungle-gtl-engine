#include "MeshBVH.h"
#include "Renderer/RenderMesh.h"

#include <algorithm>
#include <cmath>
#include <limits>

void FMeshBVH::Build(const FRenderMesh& Mesh)
{
	Triangles.clear();
	TriangleIndices.clear();
	Nodes.clear();
	RootNodeIndex = -1;

	if (Mesh.Vertices.empty() || Mesh.Indices.size() < 3)
	{
		return;
	}

	Triangles.reserve(Mesh.Indices.size() / 3);
	for (uint32 Index = 0; Index + 2 < Mesh.Indices.size(); Index += 3)
	{
		const uint32 I0 = Mesh.Indices[Index];
		const uint32 I1 = Mesh.Indices[Index + 1];
		const uint32 I2 = Mesh.Indices[Index + 2];
		if (I0 >= Mesh.Vertices.size() || I1 >= Mesh.Vertices.size() || I2 >= Mesh.Vertices.size())
		{
			continue;
		}

		FTriangleRef Triangle;
		Triangle.V0 = Mesh.Vertices[I0].Position;
		Triangle.V1 = Mesh.Vertices[I1].Position;
		Triangle.V2 = Mesh.Vertices[I2].Position;
		Triangle.Centroid = (Triangle.V0 + Triangle.V1 + Triangle.V2) / 3.0f;
		Triangle.Bounds = FAABB(Triangle.V0, Triangle.V0);
		Triangle.Bounds.Expand(Triangle.V1);
		Triangle.Bounds.Expand(Triangle.V2);
		Triangles.push_back(Triangle);
	}

	if (Triangles.empty())
	{
		return;
	}

	TriangleIndices.resize(Triangles.size());
	for (int32 Index = 0; Index < static_cast<int32>(TriangleIndices.size()); ++Index)
	{
		TriangleIndices[Index] = Index;
	}

	Nodes.reserve(Triangles.size() * 2);
	RootNodeIndex = BuildRecursive(0, static_cast<int32>(TriangleIndices.size()));
}

int32 FMeshBVH::BuildRecursive(int32 Start, int32 End, int32 Depth)
{
	const int32 NodeIndex = static_cast<int32>(Nodes.size());
	Nodes.push_back({});

	FNode Node;
	const int32 Count = End - Start;

	FAABB NodeBounds;
	FAABB CentroidBounds;
	for (int32 Index = Start; Index < End; ++Index)
	{
		const FTriangleRef& Triangle = Triangles[TriangleIndices[Index]];
		NodeBounds.Expand(Triangle.Bounds);
		CentroidBounds.Expand(Triangle.Centroid);
	}
	Node.Bounds = NodeBounds;

	if (Count <= MaxTrianglesPerLeaf || Depth >= MaxDepth)
	{
		Node.FirstTriangle = Start;
		Node.TriangleCount = Count;
		Nodes[NodeIndex] = Node;
		return NodeIndex;
	}

	const int32 Axis = CentroidBounds.MaxExtentAxis();
	const float CentroidMin = GetAxis(CentroidBounds.PMin, Axis);
	const float CentroidMax = GetAxis(CentroidBounds.PMax, Axis);

	if (std::fabs(CentroidMax - CentroidMin) < 1e-5f)
	{
		Node.FirstTriangle = Start;
		Node.TriangleCount = Count;
		Nodes[NodeIndex] = Node;
		return NodeIndex;
	}

	FBucket Buckets[NUM_BUCKETS] = {};
	for (int32 i = Start; i < End; ++i)
	{
		const FTriangleRef& Triangle = Triangles[TriangleIndices[i]];
		const float t = (GetAxis(Triangle.Centroid, Axis) - CentroidMin) / (CentroidMax - CentroidMin);
		const int32 b = std::clamp((int32)(t * NUM_BUCKETS), 0, NUM_BUCKETS - 1);
		Buckets[b].Count++;
		Buckets[b].Bounds.Expand(Triangle.Bounds);
	}

	// Prefix sweep: PrefixBounds[i] = buckets [0..i] 합산
	FAABB PrefixBounds[NUM_BUCKETS];
	int32 PrefixCount[NUM_BUCKETS];
	PrefixBounds[0] = Buckets[0].Bounds;
	PrefixCount[0]  = Buckets[0].Count;
	for (int32 i = 1; i < NUM_BUCKETS; ++i)
	{
		PrefixBounds[i] = PrefixBounds[i - 1];
		PrefixBounds[i].Expand(Buckets[i].Bounds);
		PrefixCount[i] = PrefixCount[i - 1] + Buckets[i].Count;
	}

	// Suffix sweep: SuffixBounds[i] = buckets [i..NUM_BUCKETS-1] 합산
	FAABB SuffixBounds[NUM_BUCKETS];
	int32 SuffixCount[NUM_BUCKETS];
	SuffixBounds[NUM_BUCKETS - 1] = Buckets[NUM_BUCKETS - 1].Bounds;
	SuffixCount[NUM_BUCKETS - 1]  = Buckets[NUM_BUCKETS - 1].Count;
	for (int32 i = NUM_BUCKETS - 2; i >= 0; --i)
	{
		SuffixBounds[i] = SuffixBounds[i + 1];
		SuffixBounds[i].Expand(Buckets[i].Bounds);
		SuffixCount[i] = SuffixCount[i + 1] + Buckets[i].Count;
	}

	// split i: 왼쪽 [0..i-1], 오른쪽 [i..NUM_BUCKETS-1]
	float BestCost = std::numeric_limits<float>::max();
	int32 BestSplit = -1;
	const float ParentArea = NodeBounds.SurfaceArea();

	for (int32 i = 1; i < NUM_BUCKETS; ++i)
	{
		float Cost = 1.0f;
		if (ParentArea > 1e-8f)
		{
			Cost += (PrefixBounds[i - 1].SurfaceArea() * PrefixCount[i - 1]
				   + SuffixBounds[i].SurfaceArea()      * SuffixCount[i]) / ParentArea;
		}
		if (Cost < BestCost) { BestCost = Cost; BestSplit = i; }
	}

	if (BestSplit < 0)
	{
		Node.FirstTriangle = Start;
		Node.TriangleCount = Count;
		Nodes[NodeIndex] = Node;
		return NodeIndex;
	}

	float SplitPos = CentroidMin + (CentroidMax - CentroidMin) * ((float)BestSplit / NUM_BUCKETS);

	auto MidIt = std::partition(
		TriangleIndices.begin() + Start,
		TriangleIndices.begin() + End,
		[this, Axis, SplitPos](const int32 TriangleIndex)
		{
			return GetAxis(Triangles[TriangleIndex].Centroid, Axis) < SplitPos;
		}
	);

	int32 Mid = (int32)(MidIt - TriangleIndices.begin());

	if (Mid == Start || Mid == End)
	{
		Mid = Start + Count / 2;
		std::nth_element(
			TriangleIndices.begin() + Start,
			TriangleIndices.begin() + Mid,
			TriangleIndices.begin() + End,
			[this, Axis](const int32 A, const int32 B)
			{
				return GetAxis(Triangles[A].Centroid, Axis) < GetAxis(Triangles[B].Centroid, Axis);
			});
	}

	Node.SplitAxis = Axis;
	const int32 ChildDepth = Depth + 1;
	Node.LeftChild = BuildRecursive(Start, Mid, ChildDepth);
	Node.RightChild = BuildRecursive(Mid, End, ChildDepth);
	Nodes[NodeIndex] = Node;
	return NodeIndex;
}

bool FMeshBVH::IntersectRayTriangle(const Ray& InRay, const FTriangleRef& Triangle, float& OutDistance) const
{
	constexpr float Epsilon = 1e-6f;

	const FVector Edge1 = Triangle.V1 - Triangle.V0;
	const FVector Edge2 = Triangle.V2 - Triangle.V0;

	const FVector H = FVector::CrossProduct(InRay.D, Edge2);
	const float A = FVector::DotProduct(Edge1, H);
	if (A <= Epsilon)
	{
		return false;
	}

	const float F = 1.0f / A;
	const FVector S = InRay.O - Triangle.V0;
	const float U = F * FVector::DotProduct(S, H);
	if (U < 0.0f || U > 1.0f)
	{
		return false;
	}

	const FVector Q = FVector::CrossProduct(S, Edge1);
	const float V = F * FVector::DotProduct(InRay.D, Q);
	if (V < 0.0f || U + V > 1.0f)
	{
		return false;
	}

	const float T = F * FVector::DotProduct(Edge2, Q);
	if (T > Epsilon)
	{
		OutDistance = T;
		return true;
	}

	return false;
}

bool FMeshBVH::IntersectRay(const FVector& RayOrigin, const FVector& RayDirection, float& OutDistance) const
{
	if (RootNodeIndex < 0 || RayDirection.IsZero())
	{
		return false;
	}

	const Ray LocalRay(RayOrigin, RayDirection.GetSafeNormal());

	float ClosestDistance = OutDistance;
	if (ClosestDistance <= 0.0f)
	{
		ClosestDistance = (std::numeric_limits<float>::max)();
	}

	// MaxDepth=16, 양쪽 자식을 모두 push해도 최대 스택 깊이는 MaxDepth+1
	constexpr int32 StackCapacity = MaxDepth + 2;
	int32 Stack[StackCapacity];
	int32 StackTop = 0;
	Stack[StackTop++] = RootNodeIndex;

	bool bHit = false;
	while (StackTop > 0)
	{
		const int32 NodeIndex = Stack[--StackTop];

		const FNode& Node = Nodes[NodeIndex];
		if (!Node.Bounds.Intersect(LocalRay, ClosestDistance))
		{
			continue;
		}

		if (Node.IsLeaf())
		{
			for (int32 LocalIndex = 0; LocalIndex < Node.TriangleCount; ++LocalIndex)
			{
				const int32 TriangleIndex = TriangleIndices[Node.FirstTriangle + LocalIndex];
				float HitDistance = 0.0f;
				if (IntersectRayTriangle(LocalRay, Triangles[TriangleIndex], HitDistance) && HitDistance < ClosestDistance)
				{
					ClosestDistance = HitDistance;
					bHit = true;
				}
			}
			continue;
		}

		float LeftNear = 0.0f;
		float LeftFar = 0.0f;
		const bool bHitLeft = Node.LeftChild >= 0 && Nodes[Node.LeftChild].Bounds.Intersect(LocalRay, ClosestDistance, LeftNear, LeftFar);

		float RightNear = 0.0f;
		float RightFar = 0.0f;
		const bool bHitRight = Node.RightChild >= 0 && Nodes[Node.RightChild].Bounds.Intersect(LocalRay, ClosestDistance, RightNear, RightFar);

		if (bHitLeft && bHitRight)
		{
			// 가까운 자식을 나중에 push해서 먼저 처리
			if (LeftNear <= RightNear)
			{
				Stack[StackTop++] = Node.RightChild;
				Stack[StackTop++] = Node.LeftChild;
			}
			else
			{
				Stack[StackTop++] = Node.LeftChild;
				Stack[StackTop++] = Node.RightChild;
			}
		}
		else if (bHitLeft)
		{
			Stack[StackTop++] = Node.LeftChild;
		}
		else if (bHitRight)
		{
			Stack[StackTop++] = Node.RightChild;
		}
	}

	if (bHit)
	{
		OutDistance = ClosestDistance;
	}
	return bHit;
}
