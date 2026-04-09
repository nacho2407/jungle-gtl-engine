#include "StaticMeshSimplifier.h"
#include "StaticMesh.h"
#include <algorithm>

FStaticMeshSimplifier::FStaticMeshSimplifier(UStaticMesh* InTargetMesh)
	: TargetMesh(InTargetMesh)
{
	if (TargetMesh)
	{
		MeshData = TargetMesh->GetMeshData();
	}
}

// LOD를 단계별로 생성하여 UStaticMesh에 건네줍니다. 내부 구현은... ^^
void FStaticMeshSimplifier::BuildLODs(UStaticMesh* TargetMesh)
{
	if (!TargetMesh || !TargetMesh->GetMeshData() || TargetMesh->GetMeshData()->Indices.size() < 18)
	{
		return;
	}

	FStaticMeshSimplifier Builder(TargetMesh);

	// [수정] TArray의 얕은 복사(Shallow Copy)로 인한 원본 데이터 오염을 막기 위해 
	// 명시적으로 요소를 순회하며 깊은 복사(Deep Copy)를 수행합니다.
	TArray<FNormalVertex> OriginalVertices;
	for (size_t i = 0; i < Builder.MeshData->Vertices.size(); ++i)
	{
		OriginalVertices.push_back(Builder.MeshData->Vertices[i]);
	}

	TArray<uint32> OriginalIndices;
	for (size_t i = 0; i < Builder.MeshData->Indices.size(); ++i)
	{
		OriginalIndices.push_back(Builder.MeshData->Indices[i]);
	}

	// LOD 슬롯 사전 할당
	for (int32 i = 1; i < UStaticMesh::MAX_LOD; ++i)
	{
		Builder.TargetMesh->LODMeshData[i] = new FStaticMesh();
	}

	Builder.BuildMeshQuadrics();
	Builder.SimplifyMesh(); 

	// [수정] LOD0 = 원본 메시 복원 (깊은 복사 강제)
	Builder.MeshData->Vertices.clear();
	for (size_t i = 0; i < OriginalVertices.size(); ++i)
	{
		Builder.MeshData->Vertices.push_back(OriginalVertices[i]);
	}

	Builder.MeshData->Indices.clear();
	for (size_t i = 0; i < OriginalIndices.size(); ++i)
	{
		Builder.MeshData->Indices.push_back(OriginalIndices[i]);
	}

	// 실제로 생성된 LOD 수 집계
	Builder.TargetMesh->ValidLODCount = 1;
	for (int32 i = 1; i < UStaticMesh::MAX_LOD; ++i)
	{
		if (Builder.TargetMesh->LODMeshData[i] && !Builder.TargetMesh->LODMeshData[i]->Indices.empty())
		{
			Builder.TargetMesh->ValidLODCount = i + 1;
		}
		else
		{
			break;
		}
	}
}

// ==============================================================================
// 1. Quadric Build Phase
// ==============================================================================

void FStaticMeshSimplifier::BuildMeshQuadrics()
{
	if (!MeshData) return;
	
	BuildTopologicalVertices();
	CalculateInitialQuadrics();
	FindBoundaryEdges();
}

void FStaticMeshSimplifier::BuildTopologicalVertices()
{
	// Topological Vertices 초기화 (위치가 같지만 uv, normal 값이 같은 중복 정점을 제거)
	TopologicalVertices.clear();
	RenderToTopoMap.clear();

	const TArray<FNormalVertex>& Vertices = MeshData->Vertices;

	// 맵 초기화 후 X좌표 기준 정점의 렌더 인덱스를 정렬
	for (int i = 0; i < Vertices.size(); ++i) RenderToTopoMap[i] = -1;
	TArray<int32> SortedIndices;
    SortedIndices.resize(Vertices.size());

    for (int i = 0; i < Vertices.size(); i++) SortedIndices[i] = i;
    
    std::sort(SortedIndices.begin(), SortedIndices.end(), [&](int32 A, int32 B) {
        return Vertices[A].Position.X < Vertices[B].Position.X;
    });

	// 2. 정렬된 순서대로 순회하며 O(N) 속도로 병합
    for (int i = 0; i < SortedIndices.size(); i++)
    {
        int32 RenderIdx = SortedIndices[i];
        const FVector& Pos = Vertices[RenderIdx].Position;
        int32 FoundTopoIdx = -1;

        // 내 주변(X좌표가 아주 가까운 녀석)만 뒤로 탐색하며 동일한 위상 정점이 있는지 검사
        for (int j = i - 1; j >= 0; j--)
        {
            int32 PrevRenderIdx = SortedIndices[j];
            
            // X 거리가 임계값을 넘어가면 더 이상 탐색할 필요가 없음
            if (Pos.X - Vertices[PrevRenderIdx].Position.X > 1e-3f) break; 
            
            if ((Vertices[PrevRenderIdx].Position - Pos).SizeSquared() < 1e-6f)
            {
                FoundTopoIdx = RenderToTopoMap[PrevRenderIdx];
                break;
            }
        }

        // 일치하는 위상 정점을 못 찾았다면 새로 생성
        if (FoundTopoIdx == -1)
        {
            FTopologicalVertex NewTopoVertex;
            NewTopoVertex.Position = Pos;
            FoundTopoIdx = static_cast<int32>(TopologicalVertices.size());
            TopologicalVertices.push_back(NewTopoVertex);
        }

        TopologicalVertices[FoundTopoIdx].RenderVertices.push_back(RenderIdx);
        RenderToTopoMap[RenderIdx] = FoundTopoIdx;
    }
}

void FStaticMeshSimplifier::CalculateInitialQuadrics()
{
	const TArray<uint32> PrimeTable = { 
		53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 
		49157, 98317, 196613, 393241, 786433, 1572869, 3145739, 6291469, 
		12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457
	};
	uint32 NextPrime = *std::lower_bound(PrimeTable.begin(), PrimeTable.end(), 2u * MeshData->Vertices.size() + 1u);

	Quadrics.resize(MeshData->Vertices.size());
	VertexToTriangleMap.reserve(NextPrime);

	const TArray<FNormalVertex>& Vertices = MeshData->Vertices;
	const TArray<uint32>& Indices = MeshData->Indices;
	uint32 NumTriangles = static_cast<uint32>(Indices.size()) / 3u;

	for (uint32 tidx = 0; tidx < NumTriangles; tidx++)
	{
		const uint32 I0 = Indices[tidx * 3 + 0];
		const uint32 I1 = Indices[tidx * 3 + 1];
		const uint32 I2 = Indices[tidx * 3 + 2];

		const uint32& V0 = RenderToTopoMap[I0];
		const uint32& V1 = RenderToTopoMap[I1];
		const uint32& V2 = RenderToTopoMap[I2];

		Edges.emplace(FIndexEdge(V0, V1)); Edges.emplace(FIndexEdge(V0, V2)); Edges.emplace(FIndexEdge(V1, V2));
		EdgeUsage[FIndexEdge(V0, V1)]++; EdgeUsage[FIndexEdge(V0, V2)]++; EdgeUsage[FIndexEdge(V1, V2)]++;

		// Quadric 계산: 평면 방정식 Dot(N, V) + d = 0
		FVector V01 = Vertices[V1].Position - Vertices[V0].Position;
		FVector V02 = Vertices[V2].Position - Vertices[V0].Position;
		FVector Normal = FVector::CrossProduct(V01, V02).GetSafeNormal();
		float d = -FVector::DotProduct(Normal, Vertices[V0].Position);

		AddPlaneQuadric(V0, Normal, d);
		AddPlaneQuadric(V1, Normal, d);
		AddPlaneQuadric(V2, Normal, d);

		VertexToTriangleMap[V0].emplace(tidx);
		VertexToTriangleMap[V1].emplace(tidx);
		VertexToTriangleMap[V2].emplace(tidx);
	}
}

// SIMD 최적화된 함수 (Normal과 d값을 통해 Quadric 행렬 계산)
void FStaticMeshSimplifier::AddPlaneQuadric(uint32 VertIdx, const FVector& InNormal, float InD)
{
	// 1. 평면 벡터 p = [a, b, c, d] 생성 (SIMD 레지스터 로드)
	DirectX::XMVECTOR P = DirectX::XMVectorSet(InNormal.X, InNormal.Y, InNormal.Z, InD);

	// 2. 외적(Outer Product)을 SIMD로 계산 (p * p^T)
	// P 벡터에 각각 a, b, c, d를 복제(Replicate)하여 곱하면 4x4 행렬의 각 행(Row)이 완성됩니다.
	DirectX::XMVECTOR R0 = DirectX::XMVectorMultiply(P, DirectX::XMVectorReplicate(InNormal.X));
	DirectX::XMVECTOR R1 = DirectX::XMVectorMultiply(P, DirectX::XMVectorReplicate(InNormal.Y));
	DirectX::XMVECTOR R2 = DirectX::XMVectorMultiply(P, DirectX::XMVectorReplicate(InNormal.Z));
	DirectX::XMVECTOR R3 = DirectX::XMVectorMultiply(P, DirectX::XMVectorReplicate(InD));

	// 3. 기존 Quadric 행렬에 누적 합산 (SIMD 덧셈)
	// FMatrix 구조를 참조하여 기존 데이터를 로드, 더한 후 다시 저장합니다.
	float* MatrixPtr = Quadrics[VertIdx].M[0];
	DirectX::XMVECTOR Q0 = DirectX::XMVectorAdd(DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&MatrixPtr[0])), R0);
	DirectX::XMVECTOR Q1 = DirectX::XMVectorAdd(DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&MatrixPtr[4])), R1);
	DirectX::XMVECTOR Q2 = DirectX::XMVectorAdd(DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&MatrixPtr[8])), R2);
	DirectX::XMVECTOR Q3 = DirectX::XMVectorAdd(DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(&MatrixPtr[12])), R3);

	DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&MatrixPtr[0]), Q0);
	DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&MatrixPtr[4]), Q1);
	DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&MatrixPtr[8]), Q2);
	DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(&MatrixPtr[12]), Q3);
}

void FStaticMeshSimplifier::FindBoundaryEdges()
{
	// Nanite는 전체 메쉬를 한번에 간소화하지 않고, 삼각형 128개-512개로 구성된 클러스터 단위로 간소화함
	// 따라서 클러스터의 가장자리에 해당되는 정점을 간소화하지 않고 넘기기 위해 따로 저장한다.
	for (const auto& [E, cnt] : EdgeUsage)
	{
		if (cnt == 1)
		{
			BoundaryVertices.emplace(E.A);
			BoundaryVertices.emplace(E.B);
		}
	}
}

// ==============================================================================
// 2. Error Calculation Phase
// ==============================================================================

// 정점의 위치벡터를 v = [x, y, z, 1]이라고 할 때, 오차값 E(v) = v^TQv를 구한다.
float FStaticMeshSimplifier::CalculateVertexError(const FMatrix& Q, const FVector& Pos)
{
	const DirectX::XMVECTOR V = DirectX::XMVectorSet(Pos.X, Pos.Y, Pos.Z, 1.0f);

	// Q는 대칭 행렬이므로 v^T*Q*v == dot(v, Q*v) == dot(v*Q, v)
	// XMVector4Transform은 행 벡터 규약(v * M)으로 계산하며, Q가 대칭이면 v*Q == Q*v이므로 결과 동일
	const DirectX::XMMATRIX* QMat = reinterpret_cast<const DirectX::XMMATRIX*>(&Q.M[0][0]);
	const DirectX::XMVECTOR VtQ = DirectX::XMVector4Transform(V, *QMat);

	// v^T * Q * v = dot(v, Q*v)
	return DirectX::XMVectorGetX(DirectX::XMVector4Dot(V, VtQ));
}

FCollapseCandidate FStaticMeshSimplifier::CalculateEdgeError(uint32 ia, uint32 ib)
{
	FCollapseCandidate Candidate;
	Candidate.Edge = FIndexEdge(ia, ib);

	FMatrix Q = Quadrics[ia] + Quadrics[ib];
	FVector PosA = TopologicalVertices[ia].Position;
	FVector PosB = TopologicalVertices[ib].Position;
	FVector PosMid = (PosA + PosB) * 0.5f;

	// [중요] 이전 코드에서 빠져있었던 에러 사전 계산 (FLT_MAX 방지)
	float ErrorA = CalculateVertexError(Q, PosA);
	float ErrorB = CalculateVertexError(Q, PosB);
	float ErrorMid = CalculateVertexError(Q, PosMid);

	// ==========================================================
	// 모드 1: Half-Edge Collapse (추천)
	// 텍스쳐 찢어짐(UV 왜곡)을 최소화하고 가장 안정적으로 메쉬를 줄입니다.
	// ==========================================================
	// 1. 성게(Spike) 버그를 막기 위한 3x3 수동 행렬식 검사
	float det3x3 = 
		Q.M[0][0] * (Q.M[1][1] * Q.M[2][2] - Q.M[1][2] * Q.M[2][1]) -
		Q.M[0][1] * (Q.M[1][0] * Q.M[2][2] - Q.M[1][2] * Q.M[2][0]) +
		Q.M[0][2] * (Q.M[1][0] * Q.M[2][1] - Q.M[1][1] * Q.M[2][0]);

	FMatrix Q_Opt = Q;
	Q_Opt.M[3][0] = 0.0f; Q_Opt.M[3][1] = 0.0f; Q_Opt.M[3][2] = 0.0f; Q_Opt.M[3][3] = 1.0f;

	if (std::abs(det3x3) > 1e-3f && Q_Opt.IsInvertible())
	{
		FMatrix Q_Inv = Q_Opt.GetInverse();
		Candidate.OptimalPos = FVector(Q_Inv.M[0][3], Q_Inv.M[1][3], Q_Inv.M[2][3]);
		Candidate.Error = CalculateVertexError(Q, FVector(Q_Inv.M[0][3], Q_Inv.M[1][3], Q_Inv.M[2][3]));
	}
	else
	{
		Candidate.Error = std::min({ErrorA, ErrorB, ErrorMid});
		if (Candidate.Error == ErrorMid)    Candidate.OptimalPos = PosMid;
		else if (Candidate.Error == ErrorA) Candidate.OptimalPos = PosA;
		else                                Candidate.OptimalPos = PosB;
	}

	// UV 찢어짐 방지: 두 위상 정점의 렌더 정점 UV 차이가 클수록 패널티 부여
	// UV 좌표가 크게 다른 간선(UV seam 경계)은 collapse 우선순위를 낮춘다.
	float MaxUVDistSq = 0.0f;
	for (uint32 RA : TopologicalVertices[ia].RenderVertices)
	{
		for (uint32 RB : TopologicalVertices[ib].RenderVertices)
		{
			float du = MeshData->Vertices[RA].UVs.X - MeshData->Vertices[RB].UVs.X;
			float dv = MeshData->Vertices[RA].UVs.Y - MeshData->Vertices[RB].UVs.Y;
			MaxUVDistSq = std::max(MaxUVDistSq, du * du + dv * dv);
		}
	}
	constexpr float UVSeamPenaltyScale = 1000.0f;
	Candidate.Error += UVSeamPenaltyScale * MaxUVDistSq;

	// Nanite 엣지 보존 로직, BoundaryVertex인 경우 무한대 오차 부여
	if (BoundaryVertices.contains(ia) || BoundaryVertices.contains(ib))
	{
		Candidate.Error = FLT_MAX;
	}

	return Candidate;
}

// ==============================================================================
// 3. Simplification Phase
// ==============================================================================

void FStaticMeshSimplifier::SimplifyMesh()
{
	// [렌더링 정점, 위상 정점]을 추적하는 배열 생성 (런타임에 정보가 바뀌는 배열)
	TArray<uint32> TopologicalIndices(MeshData->Indices.size());
	for (int i = 0; i < TopologicalIndices.size(); i++)
	{
		TopologicalIndices[i] = RenderToTopoMap[MeshData->Indices[i]];
	}

	std::priority_queue<FCollapseCandidate> PQ;

	// 모든 유효 간선의 오차를 계산하여 큐에 삽입한다.
	for (auto e : Edges)
	{
		PQ.push(CalculateEdgeError(e.A, e.B));
	}

	float TargetRatio = 0.9f;
	int32 CurrentTriangles = static_cast<int32>(MeshData->Indices.size()) / 3;
	int32 TargetTriangles = static_cast<int32>(CurrentTriangles * TargetRatio);
	int32 CurrentLOD = 1;

	TArray<bool> IsVertexDeleted(MeshData->Vertices.size(), false);

	// priority queue에서 계산된 오차가 작은 순서대로 간선이 pop된다.
	while (!PQ.empty() && CurrentLOD < UStaticMesh::MAX_LOD)
	{
		FCollapseCandidate Victim = PQ.top();
		PQ.pop();

		uint32 ia = Victim.Edge.A;
		uint32 ib = Victim.Edge.B;

		if (IsVertexDeleted[ia] || IsVertexDeleted[ib] || ia == ib) continue;

		FCollapseCandidate CurrentState = CalculateEdgeError(ia, ib);
		
		// [이전 데이터 검증] 계산된 오차가 큐에 있던 오차보다 크다면(쿼드릭이 그동안 누적되었다면) 과거 데이터임
		if (CurrentState.Error > Victim.Error + 0.0001f)
		{
			PQ.push(CurrentState); // 최신 데이터로 다시 큐에 넣고 스킵
			continue;
		}
		
		// [경계선 파괴 방지] 더 이상 안전하게 간소화할 수 있는 간선이 없으므로 남은 LOD를 건너뛰거나 종료
		if (CurrentState.Error >= FLT_MAX) break;

		Quadrics[ia] = Quadrics[ia] + Quadrics[ib];
		UpdateRenderVertices(ia, ib, CurrentState);

		// 삼각형 위상 갱신 및 삭제 처리
		TSet<uint32> NewNeighbors;
		UpdateTriangles(ia, ib, TopologicalIndices, CurrentTriangles, NewNeighbors);

		// ib 정점 삭제 표시 및 VertexToTriangleMap에서 제거
		IsVertexDeleted[ib] = true;
		VertexToTriangleMap.erase(ib);

		// ia의 새 이웃 정점들과의 엣지 오차를 재계산하여 큐에 삽입
		// priority queue에 저장된 원래 엣지들은 초반의 IsVertexDeleted 조건문에서 삭제
		for (uint32 vn : NewNeighbors)
		{
			if (!IsVertexDeleted[vn]) PQ.push(CalculateEdgeError(ia, vn));
		}

		// 계산한 현재 단계의 LOD 저장
		if (CurrentTriangles <= TargetTriangles)
		{
			SaveCurrentStateAsLOD(CurrentLOD, TopologicalIndices);
			CurrentLOD++;
			TargetRatio *= 0.6f;
			TargetTriangles = std::max(16, static_cast<int32>(CurrentTriangles * TargetRatio));

			if (CurrentTriangles < 16) break;
		}
	}

	BuildFinalIndices(TopologicalIndices, CurrentTriangles);
}

void FStaticMeshSimplifier::UpdateRenderVertices(uint32 TopoIa, uint32 TopoIb, const FCollapseCandidate& Victim)
{
	// 최적 위치의 ia/ib 기여 가중치 계산 (UV 보간용)
	// OptimalPos가 ia에 가까울수록 WeightA가 크고, ib에 가까울수록 WeightB가 크다.
	const FVector PosA = TopologicalVertices[TopoIa].Position;
	const FVector PosB = TopologicalVertices[TopoIb].Position;
	const FVector OptPos(Victim.OptimalPos.X, Victim.OptimalPos.Y, Victim.OptimalPos.Z);
	
	const float DistAB_sq = (PosB - PosA).SizeSquared();
	float WeightB = 0.5f;
	if (DistAB_sq > 1e-10f)
	{
		const float t = FVector::DotProduct(OptPos - PosA, PosB - PosA) / DistAB_sq;
		WeightB = std::max(0.0f, std::min(1.0f, t));
	}
	const float WeightA = 1.0f - WeightB;

	TopologicalVertices[TopoIa].Position = OptPos;

	// ia의 렌더 정점: 위치만 업데이트 (UV는 원래 UV island 유지)
	for (uint32 RenderIndex : TopologicalVertices[TopoIa].RenderVertices)
	{
		MeshData->Vertices[RenderIndex].Position = OptPos;
	}

	// ia 참조 UV: ia 렌더 정점 중 첫 번째를 기준으로 사용
	const uint32 RefIaRenderIndex = TopologicalVertices[TopoIa].RenderVertices.empty() ? TopoIa : TopologicalVertices[TopoIa].RenderVertices[0];
	
	// ib의 렌더 정점: 위치 업데이트 + UV를 ia/ib 사이에서 가중치 보간
	for (uint32 RenderIndex : TopologicalVertices[TopoIb].RenderVertices)
	{
		MeshData->Vertices[RenderIndex].Position = OptPos;
		MeshData->Vertices[RenderIndex].UVs.X = WeightA * MeshData->Vertices[RefIaRenderIndex].UVs.X + WeightB * MeshData->Vertices[RenderIndex].UVs.X;
		MeshData->Vertices[RenderIndex].UVs.Y = WeightA * MeshData->Vertices[RefIaRenderIndex].UVs.Y + WeightB * MeshData->Vertices[RenderIndex].UVs.Y;

		TopologicalVertices[TopoIa].RenderVertices.push_back(RenderIndex);
	}
	TopologicalVertices[TopoIb].RenderVertices.clear();
}

void FStaticMeshSimplifier::UpdateTriangles(uint32 TopoIa, uint32 TopoIb, TArray<uint32>& OutTopologicalIndices, int32& OutCurrentTriangles, TSet<uint32>& OutNewNeighbors)
{
	// ib를 포함하는 삼각형을 순회하며 ib -> ia로 교체, 파괴할 간선을 밑변으로 공유하는 삼각형을 삭제한다.
	for (uint32 TriangleIndex : VertexToTriangleMap[TopoIb])
	{
		uint32& I0 = OutTopologicalIndices[TriangleIndex * 3 + 0];
		uint32& I1 = OutTopologicalIndices[TriangleIndex * 3 + 1];
		uint32& I2 = OutTopologicalIndices[TriangleIndex * 3 + 2];
		
		// [퇴화 삼각형 이중 카운팅 방지] 변경 전 원래 퇴화되어 있던 상태인지 기억
		bool bWasDegenerated = (I0 == I1 || I1 == I2 || I0 == I2);
		
		if (I0 == TopoIb) I0 = TopoIa;
		if (I1 == TopoIb) I1 = TopoIa;
		if (I2 == TopoIb) I2 = TopoIa;

		// 퇴화 삼각형 검사: 두 꼭짓점이 같으면 삭제 (ia-ib를 밑변으로 공유하던 삼각형)
		if (I0 == I1 || I1 == I2 || I0 == I2)
		{
			// 퇴화된 삼각형을 인덱스 버퍼에서 제거하기 위해 세 인덱스를 동일하게 처리
			I0 = I1 = I2 = TopoIa;
			if (!bWasDegenerated) OutCurrentTriangles--;
		}
		else
		{
			VertexToTriangleMap[TopoIa].emplace(TriangleIndex);
			if (I0 != TopoIa) OutNewNeighbors.emplace(I0);
			if (I1 != TopoIa) OutNewNeighbors.emplace(I1);
			if (I2 != TopoIa) OutNewNeighbors.emplace(I2);
		}
	}
}

void FStaticMeshSimplifier::BuildFinalIndices(const TArray<uint32>& TopologicalIndices, int32 CurrentTriangles)
{
	TArray<uint32> NewIndices;
	NewIndices.reserve(static_cast<size_t>(CurrentTriangles) * 3);

	const int32 NumOriginalTriangles = static_cast<int32>(MeshData->Indices.size()) / 3;
	for (int32 tidx = 0; tidx < NumOriginalTriangles; tidx++)
	{
		// 마지막 위상 상태(TopoIndices)를 확인하여 퇴화되었는지 검사
		uint32 T0 = TopologicalIndices[tidx * 3 + 0];
		uint32 T1 = TopologicalIndices[tidx * 3 + 1];
		uint32 T2 = TopologicalIndices[tidx * 3 + 2];

		if (T0 != T1 && T1 != T2 && T0 != T2)
		{
			// 위상이 유효한 삼각형만, "원본 렌더링 인덱스"를 그대로 가져와 저장합니다.
			NewIndices.push_back(MeshData->Indices[tidx * 3 + 0]);
			NewIndices.push_back(MeshData->Indices[tidx * 3 + 1]);
			NewIndices.push_back(MeshData->Indices[tidx * 3 + 2]);
		}
	}

	MeshData->Indices = std::move(NewIndices);
}

// ==============================================================================
// 4. LOD Output Phase
// ==============================================================================

void FStaticMeshSimplifier::SaveCurrentStateAsLOD(int32 CurrentLOD, const TArray<uint32>& TopologicalIndices)
{
	if (CurrentLOD < 0 || CurrentLOD >= UStaticMesh::MAX_LOD || !MeshData) return;

	// 현재 LOD 레벨의 메쉬 데이터 레퍼런스
	FStaticMesh* TargetLOD = TargetMesh->LODMeshData[CurrentLOD];
	
	// 1. 현재까지의 정점(Vertices) 상태 복사 
	// (삭제된 정점의 위치 등도 포함되지만, 인덱스에서 참조하지 않으므로 렌더링에 영향 없음)
	TargetLOD->Vertices = MeshData->Vertices;

	// 2. 퇴화되지 않고 살아남은 삼각형(Indices)만 추출하여 복사
	TArray<uint32> ValidIndices;
	const int32 NumOriginalTriangles = static_cast<int32>(MeshData->Indices.size()) / 3;
	ValidIndices.reserve(NumOriginalTriangles * 3); 

	for (int32 tidx = 0; tidx < NumOriginalTriangles; tidx++)
	{
		// 위치에 대한 계산이므로 퇴화 여부는 위상 정점의 인덱스로 판별
		const uint32 I0 = TopologicalIndices[tidx * 3 + 0];
		const uint32 I1 = TopologicalIndices[tidx * 3 + 1];
		const uint32 I2 = TopologicalIndices[tidx * 3 + 2];

		// 퇴화된 삼각형(면적이 0인 삼각형)이 아닌 경우에만 추가
		if (I0 != I1 && I1 != I2 && I0 != I2)
		{
			ValidIndices.push_back(MeshData->Indices[tidx * 3 + 0]);
			ValidIndices.push_back(MeshData->Indices[tidx * 3 + 1]);
			ValidIndices.push_back(MeshData->Indices[tidx * 3 + 2]);
		}
	}

	TargetLOD->Indices = std::move(ValidIndices);

	// 3. LOD 메시는 간소화로 섹션 경계가 무너지므로 단일 섹션으로 통합
	FStaticMeshSection MergedSection;
	MergedSection.StartIndex      = 0;
	MergedSection.IndexCount      = static_cast<uint32>(TargetLOD->Indices.size());
	MergedSection.MaterialSlotIndex = 0;
	TargetLOD->Sections = { MergedSection };
	TargetLOD->PathFileName = MeshData->PathFileName;
}