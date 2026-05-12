#include "FbxImporter.h"
#include "Asset/StaticMeshTypes.h"
#include "Core/Logging/Log.h"
#include "Core/PlatformTime.h"

#include <fbxsdk.h>

#include <algorithm>
#include <cfloat>
#include <cctype>

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

struct FTempInfluence
{
    int32 BoneIndex = -1;
    float Weight = 0.0f;
};

/**
 * @brief 영향력 상위 4개의 bone을 FSkeletalMeshVertex에 할당
 */
static void AssignTop4Influences(
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

static void AssignRigidInfluence(FSkeletalMeshVertex& Vertex, int32 BoneIndex)
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

FSkeletalMesh* FFbxImporter::LoadSkeletalMesh(const FString& Path, const FStaticMeshLoadOptions& LoadOptions)
{
    (void)LoadOptions;

    const double StartTime = FPlatformTime::Seconds();
    UE_LOG("[FbxImporter] Start loading Skeletal FBX: %s", Path.c_str());

    FbxManager* Manager = FbxManager::Create();
    if (!Manager)
    {
        UE_LOG_ERROR("[FbxImporter] Failed to create FbxManager");
        return nullptr;
    }

    FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
    Manager->SetIOSettings(IOSettings);

    FbxScene* Scene = FbxScene::Create(Manager, "ImportSkeletalScene");
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

    FbxGeometryConverter Converter(Manager);
    Converter.Triangulate(Scene, true);

    FSkeletalMesh* SkeletalMesh = new FSkeletalMesh();
    SkeletalMesh->PathFileName = Path;

    TMap<FbxNode*, int32> BoneNodeToIndex;

    bool bHasImportedSkinnedMesh = false;

    if (FbxNode* RootNode = Scene->GetRootNode())
    {
        // 1-pass: skin deformer가 있는 mesh만 먼저 처리
        for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
        {
            CollectSkeletalMeshes(
                RootNode->GetChild(i),
                SkeletalMesh,
                ESkeletalMeshImportPass::SkinnedMeshes,
                BoneNodeToIndex,
                bHasImportedSkinnedMesh);
        }

        // 2-pass: skin deformer가 없는 mesh 중 bone 아래에 붙은 mesh를 rigid mesh로 처리
        for (int32 i = 0; i < RootNode->GetChildCount(); ++i)
        {
            CollectSkeletalMeshes(
                RootNode->GetChild(i),
                SkeletalMesh,
                ESkeletalMeshImportPass::RigidAttachedMeshes,
                BoneNodeToIndex,
                bHasImportedSkinnedMesh);
        }
    }

    Manager->Destroy();

    if (SkeletalMesh->Vertices.empty() || SkeletalMesh->Indices.empty() || SkeletalMesh->Bones.empty())
    {
        UE_LOG_ERROR("[FbxImporter] No skeletal geometry or bones found: %s", Path.c_str());
        delete SkeletalMesh;
        return nullptr;
    }

    SkeletalMesh->LocalBounds = BuildLocalBounds(SkeletalMesh);
    ComputeTangents(SkeletalMesh);

    const double EndTime = FPlatformTime::Seconds();
    UE_LOG("[FbxImporter] Skeletal FBX Loaded: %s (Vertices=%zu, Indices=%zu, Bones=%zu, Sections=%zu, Slots=%zu, %.3f sec)",
           Path.c_str(),
           SkeletalMesh->Vertices.size(),
           SkeletalMesh->Indices.size(),
           SkeletalMesh->Bones.size(),
           SkeletalMesh->Sections.size(),
           SkeletalMesh->MaterialSlots.size(),
           EndTime - StartTime);

    return SkeletalMesh;
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
	FbxAMatrix NormalTransform;
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
			SlotIndices[SlotIdx].push_back(NewIndex);
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

void FFbxImporter::CollectSkeletalMeshes(
    FbxNode* Node,
    FSkeletalMesh* InSkeletalMesh,
    ESkeletalMeshImportPass Pass,
    TMap<FbxNode*, int32>& BoneNodeToIndex,
    bool& bHasImportedSkinnedMesh)
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
            Pass,
            BoneNodeToIndex,
            bHasImportedSkinnedMesh);
    }

    for (int32 i = 0; i < Node->GetChildCount(); ++i)
    {
        CollectSkeletalMeshes(
            Node->GetChild(i),
            InSkeletalMesh,
            Pass,
            BoneNodeToIndex,
            bHasImportedSkinnedMesh);
    }
}

void FFbxImporter::ProcessSkeletalMesh(
    FbxMesh* Mesh,
    FSkeletalMesh* InSkeletalMesh,
    ESkeletalMeshImportPass Pass,
    TMap<FbxNode*, int32>& BoneNodeToIndex,
    bool& bHasImportedSkinnedMesh)
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

            const int32 NewBoneIndex = static_cast<int32>(InSkeletalMesh->Bones.size());
            BoneNodeToIndex[BoneNode] = NewBoneIndex;

            FBoneInfo Bone = {};
            Bone.Name = FString(BoneNode->GetName());
            Bone.ParentIndex = -1;

            Bone.GlobalBindTransform = ToFMatrix(LinkBindGlobal);
            Bone.InverseBindPose = Bone.GlobalBindTransform.GetInverse();
            Bone.LocalBindTransform = Bone.GlobalBindTransform;

            InSkeletalMesh->Bones.push_back(Bone);
        }
    }

    if (!bHasMeshBindGlobalWithGeometry)
    {
        return;
    }

    const FbxAMatrix NormalBindGlobalWithGeometry =
        GetNormalTransformFromPositionTransform(MeshBindGlobalWithGeometry);

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

        FBoneInfo& Bone = InSkeletalMesh->Bones[BoneIndex];
        Bone.ParentIndex = ParentIndex;

        if (ParentIndex >= 0)
        {
            const FMatrix ParentGlobalInv =
                InSkeletalMesh->Bones[ParentIndex].GlobalBindTransform.GetInverse();

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

    TArray<TArray<uint32>> SlotIndices;

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
        if (SlotIdx >= static_cast<int32>(SlotIndices.size()))
        {
            SlotIndices.resize(SlotIdx + 1);
        }

        for (int32 Corner = 0; Corner < 3; Corner++)
        {
            const int32 CtrlPointIdx = Mesh->GetPolygonVertex(PolyIdx, Corner);
            if (CtrlPointIdx < 0 || CtrlPointIdx >= ControlPointCount)
            {
                continue;
            }

            FSkeletalMeshVertex Vertex = {};
            ResetVertexInfluences(Vertex);

            FbxVector4 Pos = ControlPoints[CtrlPointIdx];
            Pos = MeshBindGlobalWithGeometry.MultT(Pos);
            Vertex.Position = ToFVector(Pos);

            FbxVector4 Normal(0, 0, 1, 0);
            if (Mesh->GetPolygonVertexNormal(PolyIdx, Corner, Normal))
            {
                Normal[3] = 0.0;
                Normal = NormalBindGlobalWithGeometry.MultT(Normal);

                Vertex.Normal = ToFVector(Normal);
                Vertex.Normal.NormalizeSafe();
            }
            else
            {
                Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
            }

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

            AssignTop4Influences(InfluencesByControlPoint[CtrlPointIdx], Vertex);

            const uint32 NewIndex = static_cast<uint32>(InSkeletalMesh->Vertices.size());
            InSkeletalMesh->Vertices.push_back(Vertex);
            SlotIndices[SlotIdx].push_back(NewIndex);
        }
    }

    for (int32 SlotIdx = 0; SlotIdx < static_cast<int32>(SlotIndices.size()); SlotIdx++)
    {
        TArray<uint32>& IndicesPerSlot = SlotIndices[SlotIdx];
        if (IndicesPerSlot.empty())
        {
            continue;
        }

        FStaticMeshSection NewSection;
        NewSection.StartIndex = static_cast<uint32>(InSkeletalMesh->Indices.size());
        NewSection.IndexCount = static_cast<uint32>(IndicesPerSlot.size());
        NewSection.MaterialSlotIndex = SlotIdx;

        InSkeletalMesh->Indices.insert(
            InSkeletalMesh->Indices.end(),
            IndicesPerSlot.begin(),
            IndicesPerSlot.end());

        InSkeletalMesh->Sections.push_back(NewSection);
    }
}

void FFbxImporter::ProcessRigidAttachedMesh(
    FbxMesh* Mesh,
    FSkeletalMesh* InSkeletalMesh,
    TMap<FbxNode*, int32>& BoneNodeToIndex,
    bool bHasImportedSkinnedMesh)
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

    if (AttachBoneIndex > 255)
    {
        UE_LOG_WARNING(
            "[FbxImporter] Skip rigid mesh because attach bone index exceeds uint8 limit | Node=%s | BoneIndex=%d",
            OwnerNode->GetName(),
            AttachBoneIndex);
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

    FbxLayerElementArrayTemplate<int32>* MaterialIndices = nullptr;
    FbxGeometryElement::EMappingMode MaterialMappingMode = FbxGeometryElement::eByPolygon;

    if (Mesh->GetElementMaterial())
    {
        MaterialIndices = &Mesh->GetElementMaterial()->GetIndexArray();
        MaterialMappingMode = Mesh->GetElementMaterial()->GetMappingMode();
    }

    TArray<TArray<uint32>> SlotIndices;

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
        if (SlotIdx >= static_cast<int32>(SlotIndices.size()))
        {
            SlotIndices.resize(SlotIdx + 1);
        }

        for (int32 Corner = 0; Corner < 3; Corner++)
        {
            const int32 CtrlPointIdx = Mesh->GetPolygonVertex(PolyIdx, Corner);
            if (CtrlPointIdx < 0 || CtrlPointIdx >= ControlPointCount)
            {
                continue;
            }

            FSkeletalMeshVertex Vertex = {};
            ResetVertexInfluences(Vertex);

            FbxVector4 Pos = ControlPoints[CtrlPointIdx];
            Pos = OwnerGlobalWithGeometry.MultT(Pos);
            Vertex.Position = ToFVector(Pos);

            FbxVector4 Normal(0, 0, 1, 0);
            if (Mesh->GetPolygonVertexNormal(PolyIdx, Corner, Normal))
            {
                Normal[3] = 0.0;
                Normal = OwnerNormalGlobalWithGeometry.MultT(Normal);

                Vertex.Normal = ToFVector(Normal);
                Vertex.Normal.NormalizeSafe();
            }
            else
            {
                Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
            }

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

            // skin이 없는 rigid mesh이므로 parent bone 하나에 100% 붙임
            AssignRigidInfluence(Vertex, AttachBoneIndex);

            const uint32 NewIndex = static_cast<uint32>(InSkeletalMesh->Vertices.size());
            InSkeletalMesh->Vertices.push_back(Vertex);
            SlotIndices[SlotIdx].push_back(NewIndex);
        }
    }

    for (int32 SlotIdx = 0; SlotIdx < static_cast<int32>(SlotIndices.size()); SlotIdx++)
    {
        TArray<uint32>& IndicesPerSlot = SlotIndices[SlotIdx];
        if (IndicesPerSlot.empty())
        {
            continue;
        }

        FStaticMeshSection NewSection;
        NewSection.StartIndex = static_cast<uint32>(InSkeletalMesh->Indices.size());
        NewSection.IndexCount = static_cast<uint32>(IndicesPerSlot.size());
        NewSection.MaterialSlotIndex = SlotIdx;

        InSkeletalMesh->Indices.insert(
            InSkeletalMesh->Indices.end(),
            IndicesPerSlot.begin(),
            IndicesPerSlot.end());

        InSkeletalMesh->Sections.push_back(NewSection);
    }
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

    for (const FSkeletalMeshVertex& Vertex : InSkeletalMesh->Vertices)
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

    const uint64 VertexCount = InSkeletalMesh->Vertices.size();
    if (VertexCount == 0)
    {
        return;
    }

    TArray<FVector> TangentAcc(VertexCount, FVector(0.0f, 0.0f, 0.0f));
    TArray<FVector> BitangentAcc(VertexCount, FVector(0.0f, 0.0f, 0.0f));

    const TArray<uint32>& Idx = InSkeletalMesh->Indices;

    for (uint64 i = 0; i + 2 < Idx.size(); i += 3)
    {
        const uint32 I0 = Idx[i];
        const uint32 I1 = Idx[i + 1];
        const uint32 I2 = Idx[i + 2];

        if (I0 >= VertexCount || I1 >= VertexCount || I2 >= VertexCount)
        {
            continue;
        }

        const FSkeletalMeshVertex& V0 = InSkeletalMesh->Vertices[I0];
        const FSkeletalMeshVertex& V1 = InSkeletalMesh->Vertices[I1];
        const FSkeletalMeshVertex& V2 = InSkeletalMesh->Vertices[I2];

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
        const FVector& N = InSkeletalMesh->Vertices[i].Normal;
        FVector T = TangentAcc[i];

        T = T - N * FVector::DotProduct(N, T);

        const float Len = T.Size();
        T = (Len > 1e-6f) ? T / Len : FVector(1.0f, 0.0f, 0.0f);

        const FVector ExpectedB = FVector::CrossProduct(N, T);
        const float Sign =
            (FVector::DotProduct(ExpectedB, BitangentAcc[i]) < 0.0f)
                ? -1.0f
                : 1.0f;

        InSkeletalMesh->Vertices[i].Tangent = FVector4(T.X, T.Y, T.Z, Sign);
    }
}
