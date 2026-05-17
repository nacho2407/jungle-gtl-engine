#include "FbxImporter.h"
#include "FbxImporterInternal.h"

#include "Core/Logging/Log.h"

#include <fbxsdk.h>

using namespace fbxsdk;
using namespace FFbxImporterInternal;

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

    const bool bFlipWinding = HasMirroredHandedness(MeshBindGlobalWithGeometry);
    if (bFlipWinding)
    {
        UE_LOG("[FbxImporter] Mirrored skeletal mesh transform detected; flipping winding. Node=%s",
               OwnerNode ? OwnerNode->GetName() : "<null>");
    }

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

        uint32 TriangleIndices[3] = {};
        int32 ValidCornerCount = 0;

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
            TriangleIndices[ValidCornerCount++] = NewIndex;
        }

        if (ValidCornerCount == 3)
        {
            AppendTriangleIndices(
                SlotIndices[SlotIdx],
                TriangleIndices[0],
                TriangleIndices[1],
                TriangleIndices[2],
                bFlipWinding);
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

    const bool bFlipWinding = HasMirroredHandedness(OwnerGlobalWithGeometry);
    if (bFlipWinding)
    {
        UE_LOG("[FbxImporter] Mirrored rigid skeletal mesh transform detected; flipping winding. Node=%s",
               OwnerNode ? OwnerNode->GetName() : "<null>");
    }

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

        uint32 TriangleIndices[3] = {};
        int32 ValidCornerCount = 0;

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
            TriangleIndices[ValidCornerCount++] = NewIndex;
        }

        if (ValidCornerCount == 3)
        {
            AppendTriangleIndices(
                SlotIndices[SlotIdx],
                TriangleIndices[0],
                TriangleIndices[1],
                TriangleIndices[2],
                bFlipWinding);
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
