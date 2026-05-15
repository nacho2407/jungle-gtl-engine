#include "FbxImporterInternal.h"

#include "Core/Logging/Log.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>

using namespace fbxsdk;

namespace FFbxImporterInternal
{
FVector ToFVector(const FbxVector4& V)
{
	return FVector(static_cast<float>(V[0]), static_cast<float>(V[1]), static_cast<float>(V[2]));
}

FVector2 ToFVector2(const FbxVector2& V)
{
	// OBJ 로더와 동일하게 V 좌표 뒤집기
	return FVector2(static_cast<float>(V[0]), 1.0f - static_cast<float>(V[1]));
}

void GetTangentBitangent(FVector& OutT, FVector& OutB,
	const FVector& P0, const FVector& P1, const FVector& P2,
	const FVector2& UV0, const FVector2& UV1, const FVector2& UV2)
{
	FVector E1 = P1 - P0;
	FVector E2 = P2 - P0;
	FVector2 dUV1 = UV1 - UV0;
	FVector2 dUV2 = UV2 - UV0;
	float Det = dUV1.X * dUV2.Y - dUV1.Y * dUV2.X;
	float r = (fabs(Det) > 1e-8f) ? (1.0f / Det) : 0.0f;

	OutT = (E1 * dUV2.Y - E2 * dUV1.Y) * r;
	OutB = (E2 * dUV1.X - E1 * dUV2.X) * r;
}

FMatrix ToFMatrix(const FbxAMatrix& M)
{
	// row-vector convention
    return FMatrix(
        static_cast<float>(M.Get(0, 0)), static_cast<float>(M.Get(0, 1)), static_cast<float>(M.Get(0, 2)), static_cast<float>(M.Get(0, 3)),
        static_cast<float>(M.Get(1, 0)), static_cast<float>(M.Get(1, 1)), static_cast<float>(M.Get(1, 2)), static_cast<float>(M.Get(1, 3)),
        static_cast<float>(M.Get(2, 0)), static_cast<float>(M.Get(2, 1)), static_cast<float>(M.Get(2, 2)), static_cast<float>(M.Get(2, 3)),
        static_cast<float>(M.Get(3, 0)), static_cast<float>(M.Get(3, 1)), static_cast<float>(M.Get(3, 2)), static_cast<float>(M.Get(3, 3)));
}

namespace FbxVertexDedupInternal
{
static uint32 FloatToStableBits(float Value)
{
    if (Value == 0.0f)
    {
        Value = 0.0f;
    }

    uint32 Bits = 0;
    std::memcpy(&Bits, &Value, sizeof(float));
    return Bits;
}

static size_t HashCombineUInt32(size_t Seed, uint32 Value)
{
    return Seed ^ (static_cast<size_t>(Value) + 0x9e3779b9u + (Seed << 6) + (Seed >> 2));
}

template <size_t Count>
static size_t HashBits(const std::array<uint32, Count>& Bits)
{
    size_t Hash = 0;
    for (uint32 Bit : Bits)
    {
        Hash = HashCombineUInt32(Hash, Bit);
    }

    return Hash;
}
}

struct FFbxStaticVertexDedupKey
{
    std::array<uint32, 12> Bits = {};

    bool operator==(const FFbxStaticVertexDedupKey& Other) const
    {
        return Bits == Other.Bits;
    }
};

struct FFbxStaticVertexDedupKeyHasher
{
    size_t operator()(const FFbxStaticVertexDedupKey& Key) const
    {
        return FbxVertexDedupInternal::HashBits(Key.Bits);
    }
};

struct FFbxSkeletalVertexDedupKey
{
    std::array<uint32, 20> Bits = {};

    bool operator==(const FFbxSkeletalVertexDedupKey& Other) const
    {
        return Bits == Other.Bits;
    }
};

struct FFbxSkeletalVertexDedupKeyHasher
{
    size_t operator()(const FFbxSkeletalVertexDedupKey& Key) const
    {
        return FbxVertexDedupInternal::HashBits(Key.Bits);
    }
};

static FFbxStaticVertexDedupKey MakeStaticVertexDedupKey(const FNormalVertex& Vertex)
{
    using namespace FbxVertexDedupInternal;

    FFbxStaticVertexDedupKey Key;
    Key.Bits = {
        FloatToStableBits(Vertex.Position.X),
        FloatToStableBits(Vertex.Position.Y),
        FloatToStableBits(Vertex.Position.Z),
        FloatToStableBits(Vertex.Color.R),
        FloatToStableBits(Vertex.Color.G),
        FloatToStableBits(Vertex.Color.B),
        FloatToStableBits(Vertex.Color.A),
        FloatToStableBits(Vertex.Normal.X),
        FloatToStableBits(Vertex.Normal.Y),
        FloatToStableBits(Vertex.Normal.Z),
        FloatToStableBits(Vertex.UVs.X),
        FloatToStableBits(Vertex.UVs.Y),
    };

    return Key;
}

static FFbxSkeletalVertexDedupKey MakeSkeletalVertexDedupKey(const FSkeletalMeshVertex& Vertex)
{
    using namespace FbxVertexDedupInternal;

    FFbxSkeletalVertexDedupKey Key;
    Key.Bits = {
        FloatToStableBits(Vertex.Position.X),
        FloatToStableBits(Vertex.Position.Y),
        FloatToStableBits(Vertex.Position.Z),
        FloatToStableBits(Vertex.Color.R),
        FloatToStableBits(Vertex.Color.G),
        FloatToStableBits(Vertex.Color.B),
        FloatToStableBits(Vertex.Color.A),
        FloatToStableBits(Vertex.Normal.X),
        FloatToStableBits(Vertex.Normal.Y),
        FloatToStableBits(Vertex.Normal.Z),
        FloatToStableBits(Vertex.UVs.X),
        FloatToStableBits(Vertex.UVs.Y),
        static_cast<uint32>(Vertex.BoneIndices[0]),
        static_cast<uint32>(Vertex.BoneIndices[1]),
        static_cast<uint32>(Vertex.BoneIndices[2]),
        static_cast<uint32>(Vertex.BoneIndices[3]),
        FloatToStableBits(Vertex.BoneWeights[0]),
        FloatToStableBits(Vertex.BoneWeights[1]),
        FloatToStableBits(Vertex.BoneWeights[2]),
        FloatToStableBits(Vertex.BoneWeights[3]),
    };

    return Key;
}

void DeduplicateStaticMeshVertices(FStaticMesh* Mesh)
{
    if (!Mesh || Mesh->Vertices.empty() || Mesh->Indices.empty())
    {
        return;
    }

    const uint64 OriginalVertexCount = Mesh->Vertices.size();

    TArray<FNormalVertex> DedupedVertices;
    DedupedVertices.reserve(Mesh->Vertices.size());

    TArray<uint32> RemappedIndices;
    RemappedIndices.reserve(Mesh->Indices.size());

    TMap<FFbxStaticVertexDedupKey, uint32, FFbxStaticVertexDedupKeyHasher> VertexToIndex;
    VertexToIndex.reserve(Mesh->Indices.size());

    for (uint32 OldIndex : Mesh->Indices)
    {
        if (OldIndex >= Mesh->Vertices.size())
        {
            UE_LOG_WARNING("[FbxImporter] Skip invalid static mesh index during dedup: %u", OldIndex);
            continue;
        }

        const FNormalVertex& Vertex = Mesh->Vertices[OldIndex];
        const FFbxStaticVertexDedupKey Key = MakeStaticVertexDedupKey(Vertex);

        auto Found = VertexToIndex.find(Key);
        if (Found != VertexToIndex.end())
        {
            RemappedIndices.push_back(Found->second);
            continue;
        }

        const uint32 NewIndex = static_cast<uint32>(DedupedVertices.size());
        DedupedVertices.push_back(Vertex);
        VertexToIndex.emplace(Key, NewIndex);
        RemappedIndices.push_back(NewIndex);
    }

    Mesh->Vertices = std::move(DedupedVertices);
    Mesh->Indices = std::move(RemappedIndices);

    if (Mesh->Vertices.size() < OriginalVertexCount)
    {
        UE_LOG("[FbxImporter] Static vertex dedup: %zu -> %zu",
               static_cast<size_t>(OriginalVertexCount),
               Mesh->Vertices.size());
    }
}

void DeduplicateSkeletalMeshVertices(FSkeletalMesh* Mesh)
{
    if (!Mesh || Mesh->Vertices.empty() || Mesh->Indices.empty())
    {
        return;
    }

    const uint64 OriginalVertexCount = Mesh->Vertices.size();

    TArray<FSkeletalMeshVertex> DedupedVertices;
    DedupedVertices.reserve(Mesh->Vertices.size());

    TArray<uint32> RemappedIndices;
    RemappedIndices.reserve(Mesh->Indices.size());

    TMap<FFbxSkeletalVertexDedupKey, uint32, FFbxSkeletalVertexDedupKeyHasher> VertexToIndex;
    VertexToIndex.reserve(Mesh->Indices.size());

    for (uint32 OldIndex : Mesh->Indices)
    {
        if (OldIndex >= Mesh->Vertices.size())
        {
            UE_LOG_WARNING("[FbxImporter] Skip invalid skeletal mesh index during dedup: %u", OldIndex);
            continue;
        }

        const FSkeletalMeshVertex& Vertex = Mesh->Vertices[OldIndex];
        const FFbxSkeletalVertexDedupKey Key = MakeSkeletalVertexDedupKey(Vertex);

        auto Found = VertexToIndex.find(Key);
        if (Found != VertexToIndex.end())
        {
            RemappedIndices.push_back(Found->second);
            continue;
        }

        const uint32 NewIndex = static_cast<uint32>(DedupedVertices.size());
        DedupedVertices.push_back(Vertex);
        VertexToIndex.emplace(Key, NewIndex);
        RemappedIndices.push_back(NewIndex);
    }

    Mesh->Vertices = std::move(DedupedVertices);
    Mesh->Indices = std::move(RemappedIndices);

    if (Mesh->Vertices.size() < OriginalVertexCount)
    {
        UE_LOG("[FbxImporter] Skeletal vertex dedup: %zu -> %zu",
               static_cast<size_t>(OriginalVertexCount),
               Mesh->Vertices.size());
    }
}

/**
 * @brief 영향력 상위 4개의 bone을 FSkeletalMeshVertex에 할당
 */
void AssignTop4Influences(
    const TArray<FTempInfluence>& SourceInfluences,
    FSkeletalMeshVertex& OutVertex)
{
    for (int32 i = 0; i < 4; ++i)
    {
        OutVertex.BoneIndices[i] = 0;
        OutVertex.BoneWeights[i] = 0.0f;
    }

    if (SourceInfluences.empty())
    {
        return;
    }

	// 정렬은 그냥 standard sort 활용
    TArray<FTempInfluence> Sorted = SourceInfluences;
    std::sort(Sorted.begin(), Sorted.end(), [](const FTempInfluence& A, const FTempInfluence& B)
                { return A.Weight > B.Weight; });

    int32 WrittenCount = 0;
    float Sum = 0.0f;

    for (const FTempInfluence& Influence : Sorted)
    {
        if (WrittenCount >= 4)
        {
            break;
        }

		/*
		 * note: 현재 BoneIndices가 uint8이라서 bone 개수가 256개 이상이면 무시
		 */
        if (Influence.BoneIndex < 0 || Influence.BoneIndex > 255 || Influence.Weight <= 0.0f)
        {
            continue;
        }

        OutVertex.BoneIndices[WrittenCount] = static_cast<uint8>(Influence.BoneIndex);
        OutVertex.BoneWeights[WrittenCount] = Influence.Weight;
        Sum += Influence.Weight;

        ++WrittenCount;
    }

    if (Sum <= 1e-6f)
    {
        for (int32 i = 0; i < 4; ++i)
        {
            OutVertex.BoneIndices[i] = 0;
            OutVertex.BoneWeights[i] = 0.0f;
        }

        return;
    }

    for (int32 i = 0; i < WrittenCount; ++i)
    {
        OutVertex.BoneWeights[i] /= Sum;
    }
}

/**
 * @brief FBX에는 node transform 뿐 아니라 mesh geometry 자체에 추가로 붙는
 *        숨은 보정 transform이 존재하기 때문에 이를 계산하는 함수
 */
FbxAMatrix GetGeometryTransform(FbxNode* Node)
{
    FbxAMatrix Geometry;
    Geometry.SetIdentity();

    if (!Node)
    {
        return Geometry;
    }

    const FbxVector4 T = Node->GetGeometricTranslation(FbxNode::eSourcePivot);
    const FbxVector4 R = Node->GetGeometricRotation(FbxNode::eSourcePivot);
    const FbxVector4 S = Node->GetGeometricScaling(FbxNode::eSourcePivot);

    Geometry.SetTRS(T, R, S);
    return Geometry;
}

/**
 * @brief normal은 translate를 적용하지 않음
 */
FbxAMatrix GetNormalTransform(FbxAMatrix Matrix)
{
    Matrix.SetT(FbxVector4(0, 0, 0, 0));
    return Matrix;
}

FbxAMatrix MakeIdentityFbxMatrix()
{
    FbxAMatrix Matrix;
    Matrix.SetIdentity();
    return Matrix;
}

FbxAMatrix GetGlobalTransformWithGeometry(FbxNode* Node)
{
    if (!Node)
    {
        return MakeIdentityFbxMatrix();
    }

    const FbxAMatrix GlobalTransform = Node->EvaluateGlobalTransform();
    const FbxAMatrix GeometryTransform = GetGeometryTransform(Node);

    // 기존 Static Mesh importer와 동일 정책
    return GlobalTransform * GeometryTransform;
}

FbxAMatrix GetNormalTransformFromPositionTransform(FbxAMatrix Matrix)
{
    Matrix.SetT(FbxVector4(0, 0, 0, 0));
    return Matrix;
}

int32 FindNearestImportedBoneIndex(
    FbxNode* StartNode,
    const TMap<FbxNode*, int32>& BoneNodeToIndex)
{
    FbxNode* Current = StartNode;
    while (Current)
    {
        auto It = BoneNodeToIndex.find(Current);
        if (It != BoneNodeToIndex.end())
        {
            return It->second;
        }

        Current = Current->GetParent();
    }

    return -1;
}

static std::string ToLowerCopy(const char* InName)
{
    std::string Result = InName ? InName : "";
    std::transform(Result.begin(), Result.end(), Result.begin(), [](unsigned char C)
                    { return static_cast<char>(std::tolower(C)); });
    return Result;
}

static bool ContainsAnyToken(const std::string& Name, const std::initializer_list<const char*> Tokens)
{
    for (const char* Token : Tokens)
    {
        if (Name.find(Token) != std::string::npos)
        {
            return true;
        }
    }

    return false;
}

bool ShouldSkipRigidMeshByName(FbxNode* OwnerNode)
{
    const std::string Name = ToLowerCopy(OwnerNode ? OwnerNode->GetName() : "");

    // helper / reference 성격이 강한 이름은 skip
    return ContainsAnyToken(Name, { "floor",
                                    "ground",
                                    "grid",
                                    "reference",
                                    "helper",
                                    "collision",
                                    "collider",
                                    "dummy" });
}

bool HasValidSkinInfluence(FbxMesh* Mesh)
{
    if (!Mesh)
    {
        return false;
    }

    const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int32 SkinIndex = 0; SkinIndex < SkinCount; ++SkinIndex)
    {
        FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
        if (!Skin)
        {
            continue;
        }

        const int32 ClusterCount = Skin->GetClusterCount();
        for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ++ClusterIndex)
        {
            FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
            if (!Cluster || !Cluster->GetLink())
            {
                continue;
            }

            const int32 IndexCount = Cluster->GetControlPointIndicesCount();
            double* Weights = Cluster->GetControlPointWeights();
            if (IndexCount <= 0 || !Weights)
            {
                continue;
            }

            for (int32 Index = 0; Index < IndexCount; ++Index)
            {
                if (Weights[Index] > 0.0)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

void InspectMeshContentRecursive(FbxNode* Node, FFbxMeshContentInfo& OutInfo)
{
    if (!Node)
    {
        return;
    }

    if (OutInfo.bHasStaticMesh && OutInfo.bHasSkeletalMesh)
    {
        return;
    }

    if (FbxMesh* Mesh = Node->GetMesh())
    {
        const bool bHasGeometry =
            Mesh->GetControlPointsCount() > 0 &&
            Mesh->GetPolygonCount() > 0;

        if (bHasGeometry)
        {
            if (HasValidSkinInfluence(Mesh))
            {
                OutInfo.bHasSkeletalMesh = true;
            }
            else
            {
                OutInfo.bHasStaticMesh = true;
            }
        }
    }

    if (OutInfo.bHasStaticMesh && OutInfo.bHasSkeletalMesh)
    {
        return;
    }

    for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
    {
        InspectMeshContentRecursive(Node->GetChild(ChildIndex), OutInfo);

        if (OutInfo.bHasStaticMesh && OutInfo.bHasSkeletalMesh)
        {
			// 둘 다 true임을 찾았으면 early exit
            return;
        }
    }
}

void ResetVertexInfluences(FSkeletalMeshVertex& Vertex)
{
    for (int32 i = 0; i < 4; ++i)
    {
        Vertex.BoneIndices[i] = 0;
        Vertex.BoneWeights[i] = 0.0f;
    }
}

void AssignRigidInfluence(FSkeletalMeshVertex& Vertex, int32 BoneIndex)
{
    ResetVertexInfluences(Vertex);

    if (BoneIndex < 0 || BoneIndex > 255)
    {
        /*
         * note: 현재 BoneIndices가 uint8이라서 BoneIndex가 255 이상이면 무시
         */
        return;
    }

    Vertex.BoneIndices[0] = static_cast<uint8>(BoneIndex);
    Vertex.BoneWeights[0] = 1.0f;
}

}
