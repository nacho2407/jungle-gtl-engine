// 충돌/피킹 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "OBB.h"
#include "Core/CoreTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"

struct FStaticMesh;

// FMeshTriangleBVH 클래스이다.
class FMeshTriangleBVH
{
public:
    // 메시의 모든 삼각형을 leaf로 수집한 뒤 로컬 공간 BVH를 즉시 다시 빌드합니다.
    void BuildNow(const FStaticMesh& Mesh);
    // 현재는 static mesh asset이 로드 후 고정된다고 보고, 아직 트리가 없을 때만 1회 빌드합니다.
    void EnsureBuilt(const FStaticMesh& Mesh);
    // 로컬 공간 ray로 BVH를 순회해 가장 가까운 삼각형 hit를 찾습니다.
    bool RaycastLocal(const FVector& LocalOrigin,
                      const FVector& LocalDirection,
                      const FStaticMesh& Mesh,
                      FHitResult& OutHitResult) const;

    // 월드 primitive BVH와 dirty 플래그 의미가 겹쳐 혼동을 줄 수 있어,
    // 메시 변경 대응용 API는 일단 주석으로 남겨 둡니다.
    // void MarkDirty();

private:
    // FTriangleLeaf는 충돌/피킹 처리에 필요한 데이터를 묶는 구조체입니다.
    struct FTriangleLeaf
    {
        FBoundingBox Bounds;
        // Mesh.Indices에서 이 삼각형이 시작하는 위치입니다.
        int32 TriangleStartIndex = 0;
    };

    // FNode는 충돌/피킹 처리에 필요한 데이터를 묶는 구조체입니다.
    struct FNode
    {
        FBoundingBox Bounds;
        // 이진 트리에서 8분기 트리로 확장
        int32 Children[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

        // SIMD(AVX) 최적화를 위한 자식 노드들의 AABB 데이터 (SOA 구조)
        alignas(32) float ChildMinX[8];
        alignas(32) float ChildMinY[8];
        alignas(32) float ChildMinZ[8];
        alignas(32) float ChildMaxX[8];
        alignas(32) float ChildMaxY[8];
        alignas(32) float ChildMaxZ[8];

        int32 ChildCount = 0;
        int32 FirstLeaf = 0;
        int32 LeafCount = 0;
        int32 PacketIndex = -1;

        bool IsLeaf() const { return ChildCount == 0; }
    };

    // alignas는 충돌/피킹 처리에 필요한 데이터를 묶는 구조체입니다.
    struct alignas(32) FTrianglePacket
    {
        alignas(32) float V0X[8];
        alignas(32) float V0Y[8];
        alignas(32) float V0Z[8];
        alignas(32) float Edge1X[8];
        alignas(32) float Edge1Y[8];
        alignas(32) float Edge1Z[8];
        alignas(32) float Edge2X[8];
        alignas(32) float Edge2Y[8];
        alignas(32) float Edge2Z[8];
        int32 TriangleStartIndices[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
        uint32 LaneMask = 0;
        uint32 TriangleCount = 0;
    };

    int32 BuildRecursive(const FStaticMesh& Mesh, int32 Start, int32 End);

    // 현재는 메시가 런타임 중 변하지 않는다고 보고, dirty 기반 재빌드는 비활성화합니다.
    // 오브젝트가 새로 로드되어 트리가 비어 있을 경우에만 EnsureBuilt에서 빌드합니다.
    // bool bDirty = true;\

    // 삼각형 단위 leaf 배열입니다. 오브젝트들을 BVH 리프 노드에서처럼 여기서는 이 배열의 연속 구간을 가리킵니다.
    TArray<FTriangleLeaf> TriangleLeaves;
    TArray<FTrianglePacket> LeafPackets;
    // 루트부터 자식까지 연속 저장한 BVH 노드 배열입니다.
    TArray<FNode> Nodes;
};
