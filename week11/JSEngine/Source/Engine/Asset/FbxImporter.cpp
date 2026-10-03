#include "FbxImporter.h"
#include "Asset/StaticMeshTypes.h"
#include "Core/Logging/Log.h"
#include "Core/PlatformTime.h"
#include "Asset/FbxAnimSequenceImporter.h"
#include "Core/ResourceManager.h"

#include <fbxsdk.h>

#include <algorithm>
#include <cfloat>
#include <cctype>
#include "FbxSceneImportContext.h"

using namespace fbxsdk;

namespace
{
static FVector ToFVector(const FbxVector4& V)
{
	return FVector(static_cast<float>(V[0]), static_cast<float>(V[1]), static_cast<float>(V[2]));
}

static FVector2 ToFVector2(const FbxVector2& V)
{
	// OBJ 로더와 동일하게 V 좌표 뒤집기
	return FVector2(static_cast<float>(V[0]), 1.0f - static_cast<float>(V[1]));
}

static void GetTangentBitangent(FVector& OutT, FVector& OutB,
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

static FMatrix ToFMatrix(const FbxAMatrix& M)
{
	// row-vector convention
    return FMatrix(
        static_cast<float>(M.Get(0, 0)), static_cast<float>(M.Get(0, 1)), static_cast<float>(M.Get(0, 2)), static_cast<float>(M.Get(0, 3)),
        static_cast<float>(M.Get(1, 0)), static_cast<float>(M.Get(1, 1)), static_cast<float>(M.Get(1, 2)), static_cast<float>(M.Get(1, 3)),
        static_cast<float>(M.Get(2, 0)), static_cast<float>(M.Get(2, 1)), static_cast<float>(M.Get(2, 2)), static_cast<float>(M.Get(2, 3)),
        static_cast<float>(M.Get(3, 0)), static_cast<float>(M.Get(3, 1)), static_cast<float>(M.Get(3, 2)), static_cast<float>(M.Get(3, 3)));
}

static float GetUpper3x3Determinant(const FbxAMatrix& M)
{
    const double M00 = M.Get(0, 0);
    const double M01 = M.Get(0, 1);
    const double M02 = M.Get(0, 2);
    const double M10 = M.Get(1, 0);
    const double M11 = M.Get(1, 1);
    const double M12 = M.Get(1, 2);
    const double M20 = M.Get(2, 0);
    const double M21 = M.Get(2, 1);
    const double M22 = M.Get(2, 2);

    return static_cast<float>(
        M00 * (M11 * M22 - M12 * M21) -
        M01 * (M10 * M22 - M12 * M20) +
        M02 * (M10 * M21 - M11 * M20));
}

static bool DoesTransformFlipWinding(const FbxAMatrix& M)
{
    return GetUpper3x3Determinant(M) < 0.0f;
}

static void AppendTriangleIndices(
    TArray<uint32>& OutIndices,
    uint32 I0,
    uint32 I1,
    uint32 I2,
    bool bReverseWinding)
{
    OutIndices.push_back(I0);
    OutIndices.push_back(bReverseWinding ? I2 : I1);
    OutIndices.push_back(bReverseWinding ? I1 : I2);
}

struct FTempInfluence
{
    int32 BoneIndex = -1;
    float Weight = 0.0f;
};

struct FRawBoneInfluence
{
    int32 GlobalBoneIndex = -1;
    float Weight = 0.0f;
};

struct FSkeletalImportVertex
{
    FSkeletalMeshVertex Vertex;
    TArray<FRawBoneInfluence> Influences;
};

struct FSkeletalImportTriangle
{
    FSkeletalImportVertex Vertices[3];
    int32 MaterialIndex = -1;
};

static TArray<FRawBoneInfluence> BuildTop4GlobalInfluences(const TArray<FTempInfluence>& SourceInfluences)
{
    TArray<FRawBoneInfluence> Result;
    if (SourceInfluences.empty())
    {
        return Result;
    }

	// 정렬은 그냥 standard sort 활용
    TArray<FTempInfluence> Sorted = SourceInfluences;
    std::sort(Sorted.begin(), Sorted.end(), [](const FTempInfluence& A, const FTempInfluence& B)
                { return A.Weight > B.Weight; });

    float Sum = 0.0f;

    for (const FTempInfluence& Influence : Sorted)
    {
        if (Result.size() >= MAX_BONE_INFLUENCES)
        {
            break;
        }

        if (Influence.BoneIndex < 0 || Influence.Weight <= 0.0f)
        {
            continue;
        }

        Result.push_back({ Influence.BoneIndex, Influence.Weight });
        Sum += Influence.Weight;
    }

    if (Sum <= 1e-6f)
    {
        Result.clear();
        return Result;
    }

    for (FRawBoneInfluence& Influence : Result)
    {
        Influence.Weight /= Sum;
    }

    return Result;
}

/**
 * @brief FBX에는 node transform 뿐 아니라 mesh geometry 자체에 추가로 붙는
 *        숨은 보정 transform이 존재하기 때문에 이를 계산하는 함수
 */
static FbxAMatrix GetGeometryTransform(FbxNode* Node)
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
static FbxAMatrix GetNormalTransform(FbxAMatrix Matrix)
{
    Matrix.SetT(FbxVector4(0, 0, 0, 0));
    return Matrix;
}

static FbxAMatrix MakeIdentityFbxMatrix()
{
    FbxAMatrix Matrix;
    Matrix.SetIdentity();
    return Matrix;
}

static FbxAMatrix GetGlobalTransformWithGeometry(FbxNode* Node)
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

static FbxAMatrix GetNormalTransformFromPositionTransform(FbxAMatrix Matrix)
{
    Matrix.SetT(FbxVector4(0, 0, 0, 0));
    return Matrix;
}

static int32 FindNearestImportedBoneIndex(
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

static bool ShouldSkipRigidMeshByName(FbxNode* OwnerNode)
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

static bool HasValidSkinInfluence(FbxMesh* Mesh)
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

static void InspectMeshContentRecursive(FbxNode* Node, FFbxMeshContentInfo& OutInfo)
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

static void ResetVertexInfluences(FSkeletalMeshVertex& Vertex)
{
    for (int32 i = 0; i < 4; ++i)
    {
        Vertex.BoneIndices[i] = 0;
        Vertex.BoneWeights[i] = 0.0f;
    }
}

static FSkeletalMeshLODRenderData& EnsureLOD0(FSkeletalMesh* InSkeletalMesh)
{
    if (InSkeletalMesh->RenderData.LODRenderData.empty())
    {
        InSkeletalMesh->RenderData.LODRenderData.resize(1);
    }

    return InSkeletalMesh->RenderData.LODRenderData[0];
}

static bool ContainsBoneIndex(const TArray<FBoneIndexType>& BoneIndices, FBoneIndexType BoneIndex)
{
    return std::find(BoneIndices.begin(), BoneIndices.end(), BoneIndex) != BoneIndices.end();
}

static int32 CountAdditionalBonesForTriangle(
    const TArray<FBoneIndexType>& BoneMap,
    const FSkeletalImportTriangle& Triangle)
{
    TArray<FBoneIndexType> AdditionalBones;

    for (const FSkeletalImportVertex& ImportVertex : Triangle.Vertices)
    {
        for (const FRawBoneInfluence& Influence : ImportVertex.Influences)
        {
            const FBoneIndexType GlobalBoneIndex = Influence.GlobalBoneIndex;
            if (GlobalBoneIndex < 0 || Influence.Weight <= 0.0f)
            {
                continue;
            }

            if (!ContainsBoneIndex(BoneMap, GlobalBoneIndex) &&
                !ContainsBoneIndex(AdditionalBones, GlobalBoneIndex))
            {
                AdditionalBones.push_back(GlobalBoneIndex);
            }
        }
    }

    return static_cast<int32>(AdditionalBones.size());
}

static FSectionBoneIndexType FindOrAddSectionBone(
    TArray<FBoneIndexType>& BoneMap,
    FBoneIndexType GlobalBoneIndex)
{
    for (int32 Index = 0; Index < static_cast<int32>(BoneMap.size()); ++Index)
    {
        if (BoneMap[Index] == GlobalBoneIndex)
        {
            return static_cast<FSectionBoneIndexType>(Index);
        }
    }

    BoneMap.push_back(GlobalBoneIndex);
    return static_cast<FSectionBoneIndexType>(BoneMap.size() - 1);
}

static void RebuildLODRequiredBones(FSkeletalMeshLODRenderData& LOD)
{
    LOD.ActiveBoneIndices.clear();
    LOD.RequiredBones.clear();

    for (const FSkeletalMeshRenderSection& Section : LOD.RenderSections)
    {
        for (FBoneIndexType BoneIndex : Section.BoneMap)
        {
            if (BoneIndex < 0 || ContainsBoneIndex(LOD.ActiveBoneIndices, BoneIndex))
            {
                continue;
            }

            LOD.ActiveBoneIndices.push_back(BoneIndex);
            LOD.RequiredBones.push_back(BoneIndex);
        }
    }
}

static void FinalizeSkeletalRenderSection(
    const FSkeletalMeshLODRenderData& LOD,
    FSkeletalMeshRenderSection& Section,
    TArray<FSkeletalMeshRenderSection>& OutSections)
{
    Section.IndexCount = static_cast<uint32>(LOD.Indices.size()) - Section.BaseIndex;
    Section.NumVertices = static_cast<uint32>(LOD.StaticVertices.size()) - Section.BaseVertexIndex;
    Section.NumTriangles = Section.IndexCount / 3;

    if (Section.IndexCount > 0 && Section.NumVertices > 0)
    {
        OutSections.push_back(Section);
    }
}

static FSkeletalMeshRenderSection MakeSkeletalRenderSection(
    const FSkeletalMeshLODRenderData& LOD,
    int32 MaterialIndex)
{
    FSkeletalMeshRenderSection Section;
    Section.BaseIndex = static_cast<uint32>(LOD.Indices.size());
    Section.BaseVertexIndex = static_cast<uint32>(LOD.StaticVertices.size());
    Section.MaterialIndex = MaterialIndex;
    Section.MaxBoneInfluences = MAX_BONE_INFLUENCES;
    return Section;
}

static void AppendImportTriangleToLOD(
    const FSkeletalImportTriangle& Triangle,
    FSkeletalMeshLODRenderData& OutLOD,
    FSkeletalMeshRenderSection& Section)
{
    for (const FSkeletalImportVertex& ImportVertex : Triangle.Vertices)
    {
        FSkeletalMeshVertex RenderVertex = ImportVertex.Vertex;
        ResetVertexInfluences(RenderVertex);

        float WeightSum = 0.0f;
        int32 WrittenInfluenceCount = 0;
        for (const FRawBoneInfluence& Influence : ImportVertex.Influences)
        {
            if (WrittenInfluenceCount >= MAX_BONE_INFLUENCES ||
                Influence.GlobalBoneIndex < 0 ||
                Influence.Weight <= 0.0f)
            {
                continue;
            }

            const FSectionBoneIndexType LocalBoneIndex =
                FindOrAddSectionBone(Section.BoneMap, Influence.GlobalBoneIndex);

            RenderVertex.BoneIndices[WrittenInfluenceCount] = LocalBoneIndex;
            RenderVertex.BoneWeights[WrittenInfluenceCount] = Influence.Weight;
            WeightSum += Influence.Weight;
            ++WrittenInfluenceCount;
        }

        if (WeightSum > 1e-6f)
        {
            for (int32 InfluenceIndex = 0; InfluenceIndex < WrittenInfluenceCount; ++InfluenceIndex)
            {
                RenderVertex.BoneWeights[InfluenceIndex] /= WeightSum;
            }
        }

        const uint32 NewVertexIndex = static_cast<uint32>(OutLOD.StaticVertices.size());
        OutLOD.StaticVertices.push_back(RenderVertex);
        OutLOD.Indices.push_back(NewVertexIndex);
    }
}

static void BuildSkeletalMeshLODRenderData(
    const TArray<FSkeletalImportTriangle>& ImportTriangles,
    FSkeletalMeshLODRenderData& OutLOD)
{
    if (ImportTriangles.empty())
    {
        return;
    }

    int32 MaxMaterialIndex = -1;
    for (const FSkeletalImportTriangle& Triangle : ImportTriangles)
    {
        MaxMaterialIndex = std::max(MaxMaterialIndex, Triangle.MaterialIndex);
    }

    for (int32 MaterialIndex = 0; MaterialIndex <= MaxMaterialIndex; ++MaterialIndex)
    {
        FSkeletalMeshRenderSection CurrentSection = MakeSkeletalRenderSection(OutLOD, MaterialIndex);
        bool bHasOpenSection = false;

        for (const FSkeletalImportTriangle& Triangle : ImportTriangles)
        {
            if (Triangle.MaterialIndex != MaterialIndex)
            {
                continue;
            }

            if (!bHasOpenSection)
            {
                CurrentSection = MakeSkeletalRenderSection(OutLOD, MaterialIndex);
                bHasOpenSection = true;
            }

            const int32 AdditionalBoneCount =
                CountAdditionalBonesForTriangle(CurrentSection.BoneMap, Triangle);

            if (!CurrentSection.BoneMap.empty() &&
                CurrentSection.BoneMap.size() + AdditionalBoneCount > MAX_GPUSKIN_BONES_PER_SECTION)
            {
                FinalizeSkeletalRenderSection(OutLOD, CurrentSection, OutLOD.RenderSections);
                CurrentSection = MakeSkeletalRenderSection(OutLOD, MaterialIndex);
            }

            AppendImportTriangleToLOD(Triangle, OutLOD, CurrentSection);
        }

        if (bHasOpenSection)
        {
            FinalizeSkeletalRenderSection(OutLOD, CurrentSection, OutLOD.RenderSections);
        }
    }

    RebuildLODRequiredBones(OutLOD);
}

}

FStaticMesh* FFbxImporter::Load(const FString& Path, const FStaticMeshLoadOptions& LoadOptions)
{
	const double StartTime = FPlatformTime::Seconds();
	UE_LOG("[FbxImporter] Start loading FBX: %s", Path.c_str());

	FbxManager* Manager = FbxManager::Create();
	if (!Manager)
	{
		UE_LOG_ERROR("[FbxImporter] Failed to create FbxManager");
		return nullptr;
	}
	
	FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
	Manager->SetIOSettings(IOSettings);

	FbxScene* Scene = FbxScene::Create(Manager, "ImportScene");
	if (!Scene)
	{
		UE_LOG_ERROR("[FbxImporter] Failed to create FbxScene");
		Manager->Destroy();
		return nullptr;
	}

	if (!ImportScene(Path, Manager, Scene))
	{
		Manager->Destroy();
		return nullptr;
	}

	// Triangulate 후 메시 처리
	FbxGeometryConverter Converter(Manager);
	Converter.Triangulate(Scene, /*pReplace=*/true);

	FStaticMesh* StaticMesh = new FStaticMesh();
	StaticMesh->PathFileName = Path;

	if (FbxNode* RootNode = Scene->GetRootNode())
	{
		for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
		{
			CollectMeshes(RootNode->GetChild(i), StaticMesh);
		}
	}

	Manager->Destroy();

	if (StaticMesh->Vertices.empty() || StaticMesh->Indices.empty())
	{
		UE_LOG_ERROR("[FbxImporter] No geometry found in FBX: %s", Path.c_str());
		delete StaticMesh;
		return nullptr;
	}

	if (LoadOptions.bNormalizeToUnitCube)
	{
		UE_LOG("[FbxImporter] NormalizeToUnitCube enabled: %s", Path.c_str());
		NormalizePositionsToUnitCube(StaticMesh);
	}

	StaticMesh->LocalBounds = BuildLocalBounds(StaticMesh);

	ComputeTangents(StaticMesh);

	UE_LOG("[FbxImporter] FBX Loaded: %s (Vertices: %zu, Indices: %zu, Sections: %zu, Slots: %zu)",
		Path.c_str(),
		StaticMesh->Vertices.size(),
		StaticMesh->Indices.size(),
		StaticMesh->Sections.size(),
		StaticMesh->Slots.size());

	const double EndTime = FPlatformTime::Seconds();
	UE_LOG("[FbxImporter] Loaded %s in %.3f sec", Path.c_str(), EndTime - StartTime);

	return StaticMesh;
}

bool FFbxImporter::SupportsExtension(const FString& Extension) const
{
	return Extension == FString("fbx") || Extension == FString(".fbx") ||
		   Extension == FString("FBX") || Extension == FString(".FBX");
}

FString FFbxImporter::GetLoaderName() const
{
	return FString{ "FFbxImporter" };
}

bool FFbxImporter::LoadSkeletalMesh(const FString& Path, const FStaticMeshLoadOptions& LoadOptions, FSkeletalMeshImportData& OutData)
{
    (void)LoadOptions;

    const double StartTime = FPlatformTime::Seconds();
    UE_LOG("[FbxImporter] Start loading Skeletal FBX: %s", Path.c_str());

    FbxManager* Manager = FbxManager::Create();
    if (!Manager)
    {
        UE_LOG_ERROR("[FbxImporter] Failed to create FbxManager");
        return false;
    }

    FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(IOSettings);

    FbxScene* Scene = FbxScene::Create(Manager, "ImportSkeletalScene");
    if (!Scene)
    {
        UE_LOG_ERROR("[FbxImporter] Failed to create FbxScene");
        Manager->Destroy();
        return false;
    }

    if (!ImportScene(Path, Manager, Scene))
    {
        Manager->Destroy();
        return false;
    }

    FbxGeometryConverter Converter(Manager);
    Converter.Triangulate(Scene, true);
    OutData.MeshData = nullptr;
    OutData.ReferenceSkeleton.RefBones.clear();
    OutData.ReferenceSkeleton.BoneNameToIndex.clear();


    FSkeletalMesh* SkeletalMesh = new FSkeletalMesh();
    SkeletalMesh->PathFileName = Path;

	FReferenceSkeleton& RefSkeleton = OutData.ReferenceSkeleton;
    TMap<FbxNode*, int32> BoneNodeToIndex;
    bool bHasImportedSkinnedMesh = false;
    int32 ImportedSkinnedMeshCount = 0;


    if (FbxNode* RootNode = Scene->GetRootNode())
    {
        // 1-pass: skin deformer가 있는 mesh만 먼저 처리
        for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
        {
            CollectSkeletalMeshes(
                RootNode->GetChild(i),
                SkeletalMesh,
                RefSkeleton,
                ESkeletalMeshImportPass::SkinnedMeshes,
                BoneNodeToIndex,
                bHasImportedSkinnedMesh,
                ImportedSkinnedMeshCount);
        }

        // 2-pass: skin deformer가 없는 mesh 중 bone 아래에 붙은 mesh를 rigid mesh로 처리
        for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
        {
            CollectSkeletalMeshes(
                RootNode->GetChild(i),
                SkeletalMesh,
                RefSkeleton,
                ESkeletalMeshImportPass::RigidAttachedMeshes,
                BoneNodeToIndex,
                bHasImportedSkinnedMesh,
                ImportedSkinnedMeshCount);
        }
    }

    Manager->Destroy();

    const FSkeletalMeshLODRenderData* LOD = SkeletalMesh->RenderData.LODRenderData.empty()
        ? nullptr
        : &SkeletalMesh->RenderData.LODRenderData[0];

    if (!LOD || LOD->StaticVertices.empty() || LOD->Indices.empty() || RefSkeleton.RefBones.empty())
    {
        UE_LOG_ERROR("[FbxImporter] No skeletal geometry or bones found: %s", Path.c_str());
        delete SkeletalMesh;
        return false;
    }

	RefSkeleton.RebuildNameToIndex();

    SkeletalMesh->LocalBounds = BuildLocalBounds(SkeletalMesh);
    ComputeTangents(SkeletalMesh);

    OutData.MeshData = SkeletalMesh;

    const double EndTime = FPlatformTime::Seconds();
    UE_LOG("[FbxImporter] Skeletal FBX Loaded: %s (SkinnedMeshes=%d, Vertices=%zu, Indices=%zu, Bones=%zu, Sections=%zu, Slots=%zu, %.3f sec)",
           Path.c_str(),
           ImportedSkinnedMeshCount,
           LOD->StaticVertices.size(),
           LOD->Indices.size(),
           RefSkeleton.RefBones.size(),
           LOD->RenderSections.size(),
           SkeletalMesh->MaterialSlots.size(),
           EndTime - StartTime);

    return true;
}

UAnimSequence* FFbxImporter::LoadAnimSequence(const FString& Path, const FString& TargetSkeletalMeshPath)
{
    return LoadAnimSequence(Path, TargetSkeletalMeshPath, FString());
}

UAnimSequence* FFbxImporter::LoadAnimSequence(const FString& Path, const FString& TargetSkeletalMeshPath, const FString& AnimStackName)
{
    USkeletalMesh* TargetMesh =
        FResourceManager::Get().LoadSkeletalMesh(TargetSkeletalMeshPath);

    if (!TargetMesh)
    {
        UE_LOG_ERROR("[FbxImporter] Failed to load target skeletal mesh for animation. Anim=%s Target=%s Stack=%s",
                     Path.c_str(),
                     TargetSkeletalMeshPath.c_str(),
                     AnimStackName.c_str());
        return nullptr;
    }

    FFbxAnimSequenceImporter Importer;
    return Importer.LoadAnimSequence(Path, TargetSkeletalMeshPath, AnimStackName, TargetMesh);
}

TArray<FString> FFbxImporter::ListAnimStacks(const FString& Path)
{
    FFbxAnimSequenceImporter Importer;
    return Importer.ListAnimStacks(Path);
}

FFbxMeshContentInfo FFbxImporter::InspectMeshContent(const FString& Path)
{
    FFbxMeshContentInfo Result;

    FbxManager* Manager = FbxManager::Create();
    if (!Manager)
    {
        UE_LOG_ERROR("[FbxImporter] Failed to create FbxManager for inspection");
        return Result;
    }

    FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(IOSettings);

    FbxScene* Scene = FbxScene::Create(Manager, "InspectFbxScene");
    if (!Scene)
    {
        UE_LOG_ERROR("[FbxImporter] Failed to create FbxScene for inspection");
        Manager->Destroy();
        return Result;
    }

    if (ImportScene(Path, Manager, Scene))
    {
        if (FbxNode* RootNode = Scene->GetRootNode())
        {
            InspectMeshContentRecursive(RootNode, Result);
        }
    }

    Manager->Destroy();
    return Result;
}

bool FFbxImporter::ImportScene(const FString& Path, FbxManager* Manager, FbxScene* Scene)
{
	FbxImporter* Importer = FbxImporter::Create(Manager, "");
	if (!Importer->Initialize(Path.c_str(), -1, Manager->GetIOSettings()))
	{
		UE_LOG_ERROR("[FbxImporter] Initialize failed: %s (%s)", Path.c_str(), Importer->GetStatus().GetErrorString());
		Importer->Destroy();
		return false;
	}

	const bool bResult = Importer->Import(Scene);
	if (!bResult)
	{
		UE_LOG_ERROR("[FbxImporter] Import failed: %s (%s)", Path.c_str(), Importer->GetStatus().GetErrorString());
	}

	Importer->Destroy();

	if (bResult)
	{
		// Engine import policy: left-handed, Z-up, X-forward, meter.
		// FBX SDK가 mesh/transform/anim까지 일관되게 변환해주므로 정점 단계에서 축 swap 금지.
		const FbxAxisSystem TargetAxis(
			FbxAxisSystem::eZAxis,
			FbxAxisSystem::eParityOdd,
			FbxAxisSystem::eLeftHanded);
		TargetAxis.DeepConvertScene(Scene);

		FbxSystemUnit::m.ConvertScene(Scene);
	}

	return bResult;
}

void FFbxImporter::CollectMeshes(FbxNode* Node, FStaticMesh* InStaticMesh)
{
	if (!Node) return;

	if (FbxNodeAttribute* Attr = Node->GetNodeAttribute())
	{
		if (Attr->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			ProcessMesh(static_cast<FbxMesh*>(Attr), InStaticMesh);
		}
	}

	for (int32 i = 0; i < Node->GetChildCount(); ++i)
	{
		CollectMeshes(Node->GetChild(i), InStaticMesh);
	}
}

void FFbxImporter::ProcessMesh(FbxMesh* Mesh, FStaticMesh* InStaticMesh)
{
	if (!Mesh || Mesh->GetPolygonCount() <= 0)
	{
		return;
	}

	FbxNode* OwnerNode = Mesh->GetNode();

	// FbxAxisSystem/FbxSystemUnit::ConvertScene은 노드 transform에 변환을 baked함.
	// → control point에 GlobalTransform * GeometricTransform을 적용해야 단위/축이 반영됨.
	FbxAMatrix VertexTransform;
	VertexTransform.SetIdentity();
	FbxAMatrix NormalTransform;
	NormalTransform.SetIdentity();
	if (OwnerNode)
	{
		const FbxVector4 T = OwnerNode->GetGeometricTranslation(FbxNode::eSourcePivot);
		const FbxVector4 R = OwnerNode->GetGeometricRotation(FbxNode::eSourcePivot);
		const FbxVector4 S = OwnerNode->GetGeometricScaling(FbxNode::eSourcePivot);
		FbxAMatrix GeomTransform;
		GeomTransform.SetTRS(T, R, S);

		const FbxAMatrix GlobalTransform = OwnerNode->EvaluateGlobalTransform();
		VertexTransform = GlobalTransform * GeomTransform;

		// Normal은 회전·스케일만 — translation 제거
		NormalTransform = VertexTransform;
		NormalTransform.SetT(FbxVector4(0, 0, 0, 0));
	}

	const FbxVector4* ControlPoints = Mesh->GetControlPoints();
	if (!ControlPoints) return;

	const bool bReverseWinding = DoesTransformFlipWinding(VertexTransform);

	// 머티리얼 매핑 모드 확인 (per-polygon으로 가정, 그 외엔 단일 슬롯으로 처리)
	FbxLayerElementArrayTemplate<int32>* MaterialIndices = nullptr;
	FbxGeometryElement::EMappingMode MaterialMappingMode = FbxGeometryElement::eByPolygon;
	if (Mesh->GetElementMaterial())
	{
		MaterialIndices = &Mesh->GetElementMaterial()->GetIndexArray();
		MaterialMappingMode = Mesh->GetElementMaterial()->GetMappingMode();
	}

	// 슬롯별 인덱스 임시 저장 (OBJ 로더와 동일한 패턴)
	TArray<TArray<uint32>> SlotIndices;

	const int32 PolygonCount = Mesh->GetPolygonCount();
	for (int32 PolyIdx = 0; PolyIdx < PolygonCount; ++PolyIdx)
	{
		// Triangulate 이후이므로 PolygonSize == 3 가정
		const int32 PolygonSize = Mesh->GetPolygonSize(PolyIdx);
		if (PolygonSize != 3) continue;

		// 머티리얼 슬롯 결정
		FString MaterialName = "DefaultWhite";
		if (MaterialIndices && OwnerNode)
		{
			int32 MatIdx = 0;
			if (MaterialMappingMode == FbxGeometryElement::eByPolygon &&
				PolyIdx < MaterialIndices->GetCount())
			{
				MatIdx = MaterialIndices->GetAt(PolyIdx);
			}
			else if (MaterialMappingMode == FbxGeometryElement::eAllSame &&
				MaterialIndices->GetCount() > 0)
			{
				MatIdx = MaterialIndices->GetAt(0);
			}

			if (MatIdx >= 0 && MatIdx < OwnerNode->GetMaterialCount())
			{
				if (FbxSurfaceMaterial* SurfMat = OwnerNode->GetMaterial(MatIdx))
				{
					MaterialName = FString(SurfMat->GetName());
				}
			}
		}

		const int32 SlotIdx = GetOrAddMaterialSlot(InStaticMesh, MaterialName);
		if (SlotIdx >= static_cast<int32>(SlotIndices.size()))
		{
			SlotIndices.resize(SlotIdx + 1);
		}

		uint32 TriangleIndices[3] = {};
		int32 TriangleVertexCount = 0;

		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const int32 CtrlPointIdx = Mesh->GetPolygonVertex(PolyIdx, Corner);

			FNormalVertex Vertex = {};

			// Position
			FbxVector4 Pos = ControlPoints[CtrlPointIdx];
			Pos = VertexTransform.MultT(Pos);
			Vertex.Position = ToFVector(Pos);

			// Normal
			FbxVector4 Normal(0, 0, 1, 0);
			if (Mesh->GetPolygonVertexNormal(PolyIdx, Corner, Normal))
			{
				Normal[3] = 0.0;  // direction vector — translation은 무시
				Normal = NormalTransform.MultT(Normal);
				Vertex.Normal = ToFVector(Normal);
				const float Len = Vertex.Normal.Size();
				if (Len > 1e-6f) Vertex.Normal = Vertex.Normal / Len;
			}
			else
			{
				Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
			}

			// UV (첫 번째 채널만 사용)
			Vertex.UVs = FVector2(0.0f, 0.0f);
			if (Mesh->GetElementUVCount() > 0)
			{
				FbxStringList UVNames;
				Mesh->GetUVSetNames(UVNames);
				if (const char* UVName = UVNames.GetStringAt(0))
				{
					FbxVector2 UV;
					bool bUnmapped = false;
					if (Mesh->GetPolygonVertexUV(PolyIdx, Corner, UVName, UV, bUnmapped))
					{
						Vertex.UVs = ToFVector2(UV);
					}
				}
			}

			Vertex.Color = FColor{ 1.0f, 1.0f, 1.0f, 1.0f };

			const uint32 NewIndex = static_cast<uint32>(InStaticMesh->Vertices.size());
			InStaticMesh->Vertices.push_back(Vertex);
			TriangleIndices[TriangleVertexCount++] = NewIndex;
		}

		if (TriangleVertexCount == 3)
		{
			AppendTriangleIndices(
				SlotIndices[SlotIdx],
				TriangleIndices[0],
				TriangleIndices[1],
				TriangleIndices[2],
				bReverseWinding);
		}
	}

	// 슬롯별 인덱스를 Mesh.Indices에 합치고 Section 생성
	for (int32 SlotIdx = 0; SlotIdx < static_cast<int32>(SlotIndices.size()); ++SlotIdx)
	{
		TArray<uint32>& IndicesPerSlot = SlotIndices[SlotIdx];
		if (IndicesPerSlot.empty()) continue;

		FStaticMeshSection NewSection;
		NewSection.StartIndex = static_cast<uint32>(InStaticMesh->Indices.size());
		NewSection.IndexCount = static_cast<uint32>(IndicesPerSlot.size());
		NewSection.MaterialSlotIndex = SlotIdx;

		InStaticMesh->Indices.insert(
			InStaticMesh->Indices.end(),
			IndicesPerSlot.begin(),
			IndicesPerSlot.end());

		InStaticMesh->Sections.push_back(NewSection);
	}
}

int32 FFbxImporter::GetOrAddMaterialSlot(FStaticMesh* InStaticMesh, const FString& MaterialName)
{
	const FString SlotName = MaterialName.empty() ? FString("DefaultWhite") : MaterialName;

	for (int32 i = 0; i < static_cast<int32>(InStaticMesh->Slots.size()); ++i)
	{
		if (InStaticMesh->Slots[i].SlotName == SlotName)
		{
			return i;
		}
	}

	FStaticMeshMaterialSlot NewSlot;
	NewSlot.SlotName = SlotName;
	NewSlot.Material = nullptr;
	InStaticMesh->Slots.push_back(NewSlot);
	return static_cast<int32>(InStaticMesh->Slots.size() - 1);
}

FAABB FFbxImporter::BuildLocalBounds(FStaticMesh* InStaticMesh) const
{
	FAABB Bounds;
	Bounds.Reset();

	for (const FNormalVertex& Vertex : InStaticMesh->Vertices)
	{
		Bounds.Expand(Vertex.Position);
	}

	return Bounds;
}

void FFbxImporter::NormalizePositionsToUnitCube(FStaticMesh* InStaticMesh)
{
	if (InStaticMesh->Vertices.empty()) return;

	FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (const FNormalVertex& V : InStaticMesh->Vertices)
	{
		Min.X = std::min(Min.X, V.Position.X);
		Min.Y = std::min(Min.Y, V.Position.Y);
		Min.Z = std::min(Min.Z, V.Position.Z);
		Max.X = std::max(Max.X, V.Position.X);
		Max.Y = std::max(Max.Y, V.Position.Y);
		Max.Z = std::max(Max.Z, V.Position.Z);
	}

	const FVector Center = (Min + Max) * 0.5f;
	const FVector Size = Max - Min;
	const float MaxDim = std::max(Size.X, std::max(Size.Y, Size.Z));
	if (MaxDim <= 1e-6f) return;

	const float Scale = 1.0f / MaxDim;
	for (FNormalVertex& V : InStaticMesh->Vertices)
	{
		V.Position = (V.Position - Center) * Scale;
	}
}

void FFbxImporter::ComputeTangents(FStaticMesh* InStaticMesh)
{
	const uint64 VertexCount = InStaticMesh->Vertices.size();
	TArray<FVector> TangentAcc(VertexCount, FVector(0, 0, 0));
	TArray<FVector> BitangentAcc(VertexCount, FVector(0, 0, 0));

	const TArray<uint32>& Idx = InStaticMesh->Indices;
	for (uint64 i = 0; i + 2 < Idx.size(); i += 3)
	{
		const uint32 I0 = Idx[i], I1 = Idx[i + 1], I2 = Idx[i + 2];
		const FNormalVertex& V0 = InStaticMesh->Vertices[I0];
		const FNormalVertex& V1 = InStaticMesh->Vertices[I1];
		const FNormalVertex& V2 = InStaticMesh->Vertices[I2];

		FVector T, B;
		GetTangentBitangent(T, B, V0.Position, V1.Position, V2.Position,
			V0.UVs, V1.UVs, V2.UVs);
		TangentAcc[I0] += T; TangentAcc[I1] += T; TangentAcc[I2] += T;
		BitangentAcc[I0] += B; BitangentAcc[I1] += B; BitangentAcc[I2] += B;
	}

	for (uint64 i = 0; i < VertexCount; ++i)
	{
		const FVector& N = InStaticMesh->Vertices[i].Normal;
		FVector T = TangentAcc[i];

		// Gram-Schmidt
		T = (T - N * FVector::DotProduct(N, T));
		const float Len = T.Size();
		T = (Len > 1e-6f) ? T / Len : FVector(1, 0, 0);

		const FVector ExpectedB = FVector::CrossProduct(N, T);
		const float Sign = (FVector::DotProduct(ExpectedB, BitangentAcc[i]) < 0.0f) ? -1.0f : 1.0f;

		InStaticMesh->Vertices[i].Tangent = FVector4(T.X, T.Y, T.Z, Sign);
	}
}



void FFbxImporter::CollectSkeletalMeshes(fbxsdk::FbxNode* Node, FSkeletalMesh* InSkeletalMesh, FReferenceSkeleton& InOutReferenceSkeleton, ESkeletalMeshImportPass Pass, TMap<fbxsdk::FbxNode*, int32>& BoneNodeToIndex, bool& bHasImportedSkinnedMesh, int32& ImportedSkinnedMeshCount)
{

    if (!Node)
    {
        return;
    }

    if (FbxMesh* Mesh = Node->GetMesh())
    {
        ProcessSkeletalMesh(
            Mesh,
            InSkeletalMesh,
            InOutReferenceSkeleton,
            Pass,
            BoneNodeToIndex,
            bHasImportedSkinnedMesh,
            ImportedSkinnedMeshCount);
    }

    for (int32 i = 0; i < Node->GetChildCount(); ++i)
    {
        CollectSkeletalMeshes(
            Node->GetChild(i),
            InSkeletalMesh,
            InOutReferenceSkeleton,
            Pass,
            BoneNodeToIndex,
            bHasImportedSkinnedMesh,
            ImportedSkinnedMeshCount);
    }
}

void FFbxImporter::ProcessSkeletalMesh(fbxsdk::FbxMesh* Mesh, FSkeletalMesh* InSkeletalMesh, FReferenceSkeleton& InOutReferenceSkeleton, ESkeletalMeshImportPass Pass, TMap<fbxsdk::FbxNode*, int32>& BoneNodeToIndex, bool& bHasImportedSkinnedMesh, int32& ImportedSkinnedMeshCount)
{
    if (!Mesh || !InSkeletalMesh || Mesh->GetPolygonCount() <= 0)
    {
        return;
    }

    const int32 SkinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);

    if (Pass == ESkeletalMeshImportPass::RigidAttachedMeshes)
    {
        if (SkinCount > 0)
        {
            return;
        }

        ProcessRigidAttachedMesh(
            Mesh,
            InSkeletalMesh,
            BoneNodeToIndex,
            bHasImportedSkinnedMesh);
        return;
    }

    if (Pass != ESkeletalMeshImportPass::SkinnedMeshes)
    {
        return;
    }

    if (SkinCount <= 0)
    {
        return;
    }

    FbxNode* OwnerNode = Mesh->GetNode();

    const FbxVector4* ControlPoints = Mesh->GetControlPoints();
    if (!ControlPoints)
    {
        return;
    }

    const int32 ControlPointCount = Mesh->GetControlPointsCount();
    FSkeletalMeshLODRenderData& LOD = EnsureLOD0(InSkeletalMesh);
    const size_t MeshVertexStart = LOD.StaticVertices.size();
    const size_t MeshIndexStart = LOD.Indices.size();
    const size_t MeshSectionStart = LOD.RenderSections.size();

    const FbxAMatrix MeshGeometry = GetGeometryTransform(OwnerNode);

    FbxAMatrix MeshBindGlobalWithGeometry;
    MeshBindGlobalWithGeometry.SetIdentity();
    bool bHasMeshBindGlobalWithGeometry = false;

    TArray<TArray<FTempInfluence>> InfluencesByControlPoint;
    InfluencesByControlPoint.resize(ControlPointCount);

    // cluster link node를 bone으로 등록
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

            FbxNode* BoneNode = Cluster->GetLink();

            FbxAMatrix MeshBindGlobal;
            FbxAMatrix LinkBindGlobal;

            Cluster->GetTransformMatrix(MeshBindGlobal);
            Cluster->GetTransformLinkMatrix(LinkBindGlobal);

            const FbxAMatrix ClusterMeshBindGlobalWithGeometry = MeshBindGlobal * MeshGeometry;
            if (!bHasMeshBindGlobalWithGeometry)
            {
                MeshBindGlobalWithGeometry = ClusterMeshBindGlobalWithGeometry;
                bHasMeshBindGlobalWithGeometry = true;
            }

            if (!bHasImportedSkinnedMesh)
            {
                bHasImportedSkinnedMesh = true;
            }

            if (BoneNodeToIndex.find(BoneNode) != BoneNodeToIndex.end())
            {
                continue;
            }

			const int32 NewBoneIndex = static_cast<int32>(InOutReferenceSkeleton.RefBones.size());
            BoneNodeToIndex[BoneNode] = NewBoneIndex;

            FBoneInfo Bone = {};
            Bone.Name = FName(BoneNode->GetName());
            Bone.ParentIndex = -1;

            Bone.GlobalBindTransform = ToFMatrix(LinkBindGlobal);
            Bone.InverseBindPose = Bone.GlobalBindTransform.GetInverse();
            Bone.LocalBindTransform = Bone.GlobalBindTransform;

            InOutReferenceSkeleton.RefBones.push_back(Bone);
        }
    }

    if (!bHasMeshBindGlobalWithGeometry)
    {
        return;
    }

    const FbxAMatrix NormalBindGlobalWithGeometry =
        GetNormalTransformFromPositionTransform(MeshBindGlobalWithGeometry);
    const bool bReverseWinding = DoesTransformFlipWinding(MeshBindGlobalWithGeometry);

    // parentIndex와 LocalBindTransform을 계산
    for (auto& Pair : BoneNodeToIndex)
    {
        FbxNode* BoneNode = Pair.first;
        const int32 BoneIndex = Pair.second;

        int32 ParentIndex = -1;
        FbxNode* ParentNode = BoneNode ? BoneNode->GetParent() : nullptr;

        while (ParentNode)
        {
            auto ParentIt = BoneNodeToIndex.find(ParentNode);
            if (ParentIt != BoneNodeToIndex.end())
            {
                ParentIndex = ParentIt->second;
                break;
            }

            ParentNode = ParentNode->GetParent();
        }

		FBoneInfo& Bone = InOutReferenceSkeleton.RefBones[BoneIndex];
        Bone.ParentIndex = ParentIndex;

        if (ParentIndex >= 0)
        {
            const FMatrix ParentGlobalInv =
                InOutReferenceSkeleton.RefBones[ParentIndex].GlobalBindTransform.GetInverse();

            Bone.LocalBindTransform = Bone.GlobalBindTransform * ParentGlobalInv;
        }
        else
        {
            Bone.LocalBindTransform = Bone.GlobalBindTransform;
        }
    }

    // control point별 influence를 수집
    for (int32 SkinIndex = 0; SkinIndex < SkinCount; SkinIndex++)
    {
        FbxSkin* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(SkinIndex, FbxDeformer::eSkin));
        if (!Skin)
        {
            continue;
        }

        const int32 ClusterCount = Skin->GetClusterCount();
        for (int32 ClusterIndex = 0; ClusterIndex < ClusterCount; ClusterIndex++)
        {
            FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
            if (!Cluster || !Cluster->GetLink())
            {
                continue;
            }

            auto BoneIt = BoneNodeToIndex.find(Cluster->GetLink());
            if (BoneIt == BoneNodeToIndex.end())
            {
                continue;
            }

            const int32 BoneIndex = BoneIt->second;

            const int32 IndexCount = Cluster->GetControlPointIndicesCount();
            int* ControlPointIndices = Cluster->GetControlPointIndices();
            double* ControlPointWeights = Cluster->GetControlPointWeights();

            if (!ControlPointIndices || !ControlPointWeights)
            {
                continue;
            }

            for (int32 i = 0; i < IndexCount; i++)
            {
                const int32 CtrlPointIndex = ControlPointIndices[i];
                const float Weight = static_cast<float>(ControlPointWeights[i]);

                if (CtrlPointIndex < 0 || CtrlPointIndex >= ControlPointCount || Weight <= 0.0f)
                {
                    continue;
                }

                InfluencesByControlPoint[CtrlPointIndex].push_back({ BoneIndex, Weight });
            }
        }
    }

    // material mapping 정보 준비
    FbxLayerElementArrayTemplate<int32>* MaterialIndices = nullptr;
    FbxGeometryElement::EMappingMode MaterialMappingMode = FbxGeometryElement::eByPolygon;

    if (Mesh->GetElementMaterial())
    {
        MaterialIndices = &Mesh->GetElementMaterial()->GetIndexArray();
        MaterialMappingMode = Mesh->GetElementMaterial()->GetMappingMode();
    }

    TArray<FSkeletalImportTriangle> ImportTriangles;

    // polygon corner를 FSkeletalMeshVertex로 변환
    const int32 PolygonCount = Mesh->GetPolygonCount();
    for (int32 PolyIdx = 0; PolyIdx < PolygonCount; PolyIdx++)
    {
        const int32 PolygonSize = Mesh->GetPolygonSize(PolyIdx);
        if (PolygonSize != 3)
        {
            continue;
        }

        FString MaterialName = "DefaultWhite";

        if (MaterialIndices && OwnerNode)
        {
            int32 MatIdx = 0;

            if (MaterialMappingMode == FbxGeometryElement::eByPolygon &&
                PolyIdx < MaterialIndices->GetCount())
            {
                MatIdx = MaterialIndices->GetAt(PolyIdx);
            }
            else if (MaterialMappingMode == FbxGeometryElement::eAllSame &&
                     MaterialIndices->GetCount() > 0)
            {
                MatIdx = MaterialIndices->GetAt(0);
            }

            if (MatIdx >= 0 && MatIdx < OwnerNode->GetMaterialCount())
            {
                if (FbxSurfaceMaterial* SurfMat = OwnerNode->GetMaterial(MatIdx))
                {
                    MaterialName = FString(SurfMat->GetName());
                }
            }
        }

        const int32 SlotIdx = GetOrAddMaterialSlot(InSkeletalMesh, MaterialName);

        FSkeletalImportTriangle ImportTri;
        ImportTri.MaterialIndex = SlotIdx;
        int32 TriangleVertexCount = 0;

        for (int32 Corner = 0; Corner < 3; Corner++)
        {
            const int32 CtrlPointIdx = Mesh->GetPolygonVertex(PolyIdx, Corner);
            if (CtrlPointIdx < 0 || CtrlPointIdx >= ControlPointCount)
            {
                continue;
            }

            FSkeletalImportVertex ImportVertex = {};
            ResetVertexInfluences(ImportVertex.Vertex);

            FbxVector4 Pos = ControlPoints[CtrlPointIdx];
            Pos = MeshBindGlobalWithGeometry.MultT(Pos);
            ImportVertex.Vertex.Position = ToFVector(Pos);

            FbxVector4 Normal(0, 0, 1, 0);
            if (Mesh->GetPolygonVertexNormal(PolyIdx, Corner, Normal))
            {
                Normal[3] = 0.0;
                Normal = NormalBindGlobalWithGeometry.MultT(Normal);

                ImportVertex.Vertex.Normal = ToFVector(Normal);
                ImportVertex.Vertex.Normal.NormalizeSafe();
            }
            else
            {
                ImportVertex.Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
            }

            ImportVertex.Vertex.UVs = FVector2(0.0f, 0.0f);
            if (Mesh->GetElementUVCount() > 0)
            {
                FbxStringList UVNames;
                Mesh->GetUVSetNames(UVNames);

                if (const char* UVName = UVNames.GetStringAt(0))
                {
                    FbxVector2 UV;
                    bool bUnmapped = false;

                    if (Mesh->GetPolygonVertexUV(PolyIdx, Corner, UVName, UV, bUnmapped))
                    {
                        ImportVertex.Vertex.UVs = ToFVector2(UV);
                    }
                }
            }

            ImportVertex.Vertex.Color = FColor{ 1.0f, 1.0f, 1.0f, 1.0f };

            ImportVertex.Influences = BuildTop4GlobalInfluences(InfluencesByControlPoint[CtrlPointIdx]);

            ImportTri.Vertices[TriangleVertexCount++] = ImportVertex;
        }

        if (TriangleVertexCount == 3)
        {
            if (bReverseWinding)
            {
                std::swap(ImportTri.Vertices[1], ImportTri.Vertices[2]);
            }

            ImportTriangles.push_back(ImportTri);
        }
    }

    BuildSkeletalMeshLODRenderData(ImportTriangles, LOD);

    const size_t ImportedVertexCount = LOD.StaticVertices.size() - MeshVertexStart;
    const size_t ImportedIndexCount = LOD.Indices.size() - MeshIndexStart;
    const size_t ImportedSectionCount = LOD.RenderSections.size() - MeshSectionStart;
    if (ImportedVertexCount > 0 && ImportedIndexCount > 0)
    {
        ++ImportedSkinnedMeshCount;
        UE_LOG("[FbxImporter] Imported skinned mesh: Node=%s Vertices=%zu Indices=%zu Sections=%zu Bones=%zu TotalSkinnedMeshes=%d",
               OwnerNode ? OwnerNode->GetName() : "<null>",
               ImportedVertexCount,
               ImportedIndexCount,
               ImportedSectionCount,
               InOutReferenceSkeleton.RefBones.size(),
               ImportedSkinnedMeshCount);
    }
}



void FFbxImporter::ProcessRigidAttachedMesh(fbxsdk::FbxMesh* Mesh, FSkeletalMesh* InSkeletalMesh, TMap<fbxsdk::FbxNode*, int32>& BoneNodeToIndex, bool bHasImportedSkinnedMesh)
{

    if (!Mesh || !InSkeletalMesh || Mesh->GetPolygonCount() <= 0)
    {
        return;
    }

    FbxNode* OwnerNode = Mesh->GetNode();
    if (!OwnerNode)
    {
        return;
    }

    if (!bHasImportedSkinnedMesh)
    {
        UE_LOG_WARNING("[FbxImporter] Skip rigid mesh before skinned mesh import | Node=%s", OwnerNode->GetName());
        return;
    }

    if (ShouldSkipRigidMeshByName(OwnerNode))
    {
        return;
    }

    const int32 AttachBoneIndex = FindNearestImportedBoneIndex(OwnerNode, BoneNodeToIndex);
    if (AttachBoneIndex < 0)
    {
        return;
    }

    const FbxVector4* ControlPoints = Mesh->GetControlPoints();
    if (!ControlPoints)
    {
        return;
    }

    const int32 ControlPointCount = Mesh->GetControlPointsCount();

    const FbxAMatrix OwnerGlobalWithGeometry = GetGlobalTransformWithGeometry(OwnerNode);
    const FbxAMatrix OwnerNormalGlobalWithGeometry =
        GetNormalTransformFromPositionTransform(OwnerGlobalWithGeometry);
    const bool bReverseWinding = DoesTransformFlipWinding(OwnerGlobalWithGeometry);

    FbxLayerElementArrayTemplate<int32>* MaterialIndices = nullptr;
    FbxGeometryElement::EMappingMode MaterialMappingMode = FbxGeometryElement::eByPolygon;

    if (Mesh->GetElementMaterial())
    {
        MaterialIndices = &Mesh->GetElementMaterial()->GetIndexArray();
        MaterialMappingMode = Mesh->GetElementMaterial()->GetMappingMode();
    }

    TArray<FSkeletalImportTriangle> ImportTriangles;

    const int32 PolygonCount = Mesh->GetPolygonCount();

    for (int32 PolyIdx = 0; PolyIdx < PolygonCount; PolyIdx++)
    {
        const int32 PolygonSize = Mesh->GetPolygonSize(PolyIdx);
        if (PolygonSize != 3)
        {
            continue;
        }

        FString MaterialName = "DefaultWhite";

        if (MaterialIndices && OwnerNode)
        {
            int32 MatIdx = 0;

            if (MaterialMappingMode == FbxGeometryElement::eByPolygon &&
                PolyIdx < MaterialIndices->GetCount())
            {
                MatIdx = MaterialIndices->GetAt(PolyIdx);
            }
            else if (MaterialMappingMode == FbxGeometryElement::eAllSame &&
                     MaterialIndices->GetCount() > 0)
            {
                MatIdx = MaterialIndices->GetAt(0);
            }

            if (MatIdx >= 0 && MatIdx < OwnerNode->GetMaterialCount())
            {
                if (FbxSurfaceMaterial* SurfMat = OwnerNode->GetMaterial(MatIdx))
                {
                    MaterialName = FString(SurfMat->GetName());
                }
            }
        }

        const int32 SlotIdx = GetOrAddMaterialSlot(InSkeletalMesh, MaterialName);

        FSkeletalImportTriangle ImportTri;
        ImportTri.MaterialIndex = SlotIdx;
        int32 TriangleVertexCount = 0;

        for (int32 Corner = 0; Corner < 3; Corner++)
        {
            const int32 CtrlPointIdx = Mesh->GetPolygonVertex(PolyIdx, Corner);
            if (CtrlPointIdx < 0 || CtrlPointIdx >= ControlPointCount)
            {
                continue;
            }

            FSkeletalImportVertex ImportVertex = {};
            ResetVertexInfluences(ImportVertex.Vertex);

            FbxVector4 Pos = ControlPoints[CtrlPointIdx];
            Pos = OwnerGlobalWithGeometry.MultT(Pos);
            ImportVertex.Vertex.Position = ToFVector(Pos);

            FbxVector4 Normal(0, 0, 1, 0);
            if (Mesh->GetPolygonVertexNormal(PolyIdx, Corner, Normal))
            {
                Normal[3] = 0.0;
                Normal = OwnerNormalGlobalWithGeometry.MultT(Normal);

                ImportVertex.Vertex.Normal = ToFVector(Normal);
                ImportVertex.Vertex.Normal.NormalizeSafe();
            }
            else
            {
                ImportVertex.Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
            }

            ImportVertex.Vertex.UVs = FVector2(0.0f, 0.0f);
            if (Mesh->GetElementUVCount() > 0)
            {
                FbxStringList UVNames;
                Mesh->GetUVSetNames(UVNames);

                if (const char* UVName = UVNames.GetStringAt(0))
                {
                    FbxVector2 UV;
                    bool bUnmapped = false;

                    if (Mesh->GetPolygonVertexUV(PolyIdx, Corner, UVName, UV, bUnmapped))
                    {
                        ImportVertex.Vertex.UVs = ToFVector2(UV);
                    }
                }
            }

            ImportVertex.Vertex.Color = FColor{ 1.0f, 1.0f, 1.0f, 1.0f };

            // skin이 없는 rigid mesh이므로 parent bone 하나에 100% 붙임
            ImportVertex.Influences = { { AttachBoneIndex, 1.0f } };

            ImportTri.Vertices[TriangleVertexCount++] = ImportVertex;
        }

        if (TriangleVertexCount == 3)
        {
            if (bReverseWinding)
            {
                std::swap(ImportTri.Vertices[1], ImportTri.Vertices[2]);
            }

            ImportTriangles.push_back(ImportTri);
        }
    }

    FSkeletalMeshLODRenderData& LOD = EnsureLOD0(InSkeletalMesh);
    BuildSkeletalMeshLODRenderData(ImportTriangles, LOD);
}




int32 FFbxImporter::GetOrAddMaterialSlot(FSkeletalMesh* InSkeletalMesh, const FString& MaterialName)
{
    const FString SlotName = MaterialName.empty() ? FString("DefaultWhite") : MaterialName;

    for (int32 i = 0; i < static_cast<int32>(InSkeletalMesh->MaterialSlots.size()); i++)
    {
        if (InSkeletalMesh->MaterialSlots[i].SlotName == SlotName)
        {
            return i;
        }
    }

    FStaticMeshMaterialSlot NewSlot;
    NewSlot.SlotName = SlotName;
    NewSlot.Material = nullptr;
    InSkeletalMesh->MaterialSlots.push_back(NewSlot);
    return static_cast<int32>(InSkeletalMesh->MaterialSlots.size() - 1);
}




FAABB FFbxImporter::BuildLocalBounds(FSkeletalMesh* InSkeletalMesh) const
{
    FAABB Bounds;
    Bounds.Reset();

    if (!InSkeletalMesh)
    {
        return Bounds;
    }

    const FSkeletalMeshLODRenderData* LOD = InSkeletalMesh->RenderData.LODRenderData.empty()
        ? nullptr
        : &InSkeletalMesh->RenderData.LODRenderData[0];

    if (!LOD)
    {
        return Bounds;
    }

    for (const FSkeletalMeshVertex& Vertex : LOD->StaticVertices)
    {
        Bounds.Expand(Vertex.Position);
    }

    return Bounds;
}



void FFbxImporter::ComputeTangents(FSkeletalMesh* InSkeletalMesh)
{
    if (!InSkeletalMesh)
    {
        return;
    }

    FSkeletalMeshLODRenderData& LOD = EnsureLOD0(InSkeletalMesh);
    const uint64 VertexCount = LOD.StaticVertices.size();
    if (VertexCount == 0)
    {
        return;
    }

    TArray<FVector> TangentAcc(VertexCount, FVector(0.0f, 0.0f, 0.0f));
    TArray<FVector> BitangentAcc(VertexCount, FVector(0.0f, 0.0f, 0.0f));

    const TArray<uint32>& Idx = LOD.Indices;

    for (uint64 i = 0; i + 2 < Idx.size(); i += 3)
    {
        const uint32 I0 = Idx[i];
        const uint32 I1 = Idx[i + 1];
        const uint32 I2 = Idx[i + 2];

        if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
        {
            continue;
        }

        const FSkeletalMeshVertex& V0 = LOD.StaticVertices[I0];
        const FSkeletalMeshVertex& V1 = LOD.StaticVertices[I1];
        const FSkeletalMeshVertex& V2 = LOD.StaticVertices[I2];

        FVector T;
        FVector B;

        GetTangentBitangent(
            T,
            B,
            V0.Position,
            V1.Position,
            V2.Position,
            V0.UVs,
            V1.UVs,
            V2.UVs);

        TangentAcc[I0] += T;
        TangentAcc[I1] += T;
        TangentAcc[I2] += T;

        BitangentAcc[I0] += B;
        BitangentAcc[I1] += B;
        BitangentAcc[I2] += B;
    }

    for (uint64 i = 0; i < VertexCount; i++)
    {
        const FVector& N = LOD.StaticVertices[i].Normal;
        FVector T = TangentAcc[i];

        T = T - N * FVector::DotProduct(N, T);

        const float Len = T.Size();
        T = (Len > 1e-6f) ? T / Len : FVector(1.0f, 0.0f, 0.0f);

        const FVector ExpectedB = FVector::CrossProduct(N, T);
        const float Sign =
            (FVector::DotProduct(ExpectedB, BitangentAcc[i]) < 0.0f)
                ? -1.0f
                : 1.0f;

        LOD.StaticVertices[i].Tangent = FVector4(T.X, T.Y, T.Z, Sign);
    }
}
