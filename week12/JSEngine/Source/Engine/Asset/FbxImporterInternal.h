#pragma once

#include "FbxImporter.h"

#include <fbxsdk.h>

namespace FFbxImporterInternal
{
FVector ToFVector(const fbxsdk::FbxVector4& V);
FVector2 ToFVector2(const fbxsdk::FbxVector2& V);

void GetTangentBitangent(
    FVector& OutT,
    FVector& OutB,
    const FVector& P0,
    const FVector& P1,
    const FVector& P2,
    const FVector2& UV0,
    const FVector2& UV1,
    const FVector2& UV2);

FMatrix ToFMatrix(const fbxsdk::FbxAMatrix& M);

struct FTempInfluence
{
    int32 BoneIndex = -1;
    float Weight = 0.0f;
};

void DeduplicateStaticMeshVertices(FStaticMesh* Mesh);
void DeduplicateSkeletalMeshVertices(FSkeletalMesh* Mesh);

void AssignTop4Influences(
    const TArray<FTempInfluence>& SourceInfluences,
    FSkeletalMeshVertex& OutVertex);

fbxsdk::FbxAMatrix GetGeometryTransform(fbxsdk::FbxNode* Node);
fbxsdk::FbxAMatrix GetNormalTransform(fbxsdk::FbxAMatrix Matrix);
fbxsdk::FbxAMatrix MakeIdentityFbxMatrix();
fbxsdk::FbxAMatrix GetGlobalTransformWithGeometry(fbxsdk::FbxNode* Node);
fbxsdk::FbxAMatrix GetNormalTransformFromPositionTransform(fbxsdk::FbxAMatrix Matrix);

int32 FindNearestImportedBoneIndex(
    fbxsdk::FbxNode* StartNode,
    const TMap<fbxsdk::FbxNode*, int32>& BoneNodeToIndex);

bool ShouldSkipRigidMeshByName(fbxsdk::FbxNode* OwnerNode);
bool HasValidSkinInfluence(fbxsdk::FbxMesh* Mesh);
void InspectMeshContentRecursive(fbxsdk::FbxNode* Node, FFbxMeshContentInfo& OutInfo);

void ResetVertexInfluences(FSkeletalMeshVertex& Vertex);
void AssignRigidInfluence(FSkeletalMeshVertex& Vertex, int32 BoneIndex);
}
