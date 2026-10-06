#include "Mesh/Importer/Fbx/FbxSkeletalMeshImporter.h"
#include "Mesh/Importer/Fbx/FbxSceneQuery.h"
#include "Mesh/Importer/Fbx/FbxMaterialImporter.h"
#include "Mesh/Importer/Fbx/FbxSkeletonImporter.h"
#include "Mesh/Importer/Fbx/FbxSkinWeightImporter.h"
#include "Mesh/Importer/Fbx/FbxAnimationImporter.h"
#include "Mesh/Importer/Fbx/FbxTransformUtils.h"
#include "Core/Logging/Log.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	static float GetIdentityError(const FMatrix& Matrix)
	{
		float MaxError = 0.0f;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				const float Expected = (Row == Col) ? 1.0f : 0.0f;
				MaxError = std::max(MaxError, std::fabs(Matrix.M[Row][Col] - Expected));
			}
		}

		return MaxError;
	}

	static float GetScaleDeviationFromOne(const FVector& Scale)
	{
		return std::max(
			std::fabs(Scale.X - 1.0f),
			std::max(std::fabs(Scale.Y - 1.0f), std::fabs(Scale.Z - 1.0f)));
	}

	static void BuildReferenceSkeleton(FFbxImportContext& Context)
	{
		Context.ReferenceSkeleton.Bones.clear();
		Context.ReferenceSkeleton.Bones.reserve(Context.Bones.size());

		for (const FBone& Bone : Context.Bones)
		{
			FReferenceBone RefBone;
			RefBone.Name = Bone.Name;
			RefBone.ParentIndex = Bone.ParentIndex;
			RefBone.LocalBindPose = Bone.GetReferenceLocalPose();
			RefBone.GlobalBindPose = Bone.GetReferenceGlobalPose();
			RefBone.InverseBindPose = Bone.GetInverseBindPose();
			Context.ReferenceSkeleton.Bones.push_back(RefBone);
		}
	}

	static void NormalizeSkeletalBoneScales(FFbxImportContext& Context)
	{
		const int32 BoneCount = static_cast<int32>(Context.Bones.size());
		if (BoneCount <= 0)
		{
			return;
		}

		TArray<FMatrix> ScaleFreeReferenceGlobals;
		TArray<FMatrix> ScaleFreeBindGlobals;
		ScaleFreeReferenceGlobals.resize(BoneCount, FMatrix::Identity);
		ScaleFreeBindGlobals.resize(BoneCount, FMatrix::Identity);

		int32 RootBoneIndex = -1;
		float MaxInputScaleDeviation = 0.0f;

		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			FBone& Bone = Context.Bones[BoneIndex];

			if (RootBoneIndex == -1 && Bone.ParentIndex < 0)
			{
				RootBoneIndex = BoneIndex;
			}

			MaxInputScaleDeviation = std::max(
				MaxInputScaleDeviation,
				GetScaleDeviationFromOne(Bone.GetReferenceGlobalPose().GetScale()));
			MaxInputScaleDeviation = std::max(
				MaxInputScaleDeviation,
				GetScaleDeviationFromOne(Bone.GetSkinBindGlobalPose().GetScale()));

			ScaleFreeReferenceGlobals[BoneIndex] = FFbxTransformUtils::MakeScaleFreeMatrix(Bone.GetReferenceGlobalPose());
			ScaleFreeBindGlobals[BoneIndex] = FFbxTransformUtils::MakeScaleFreeMatrix(Bone.GetSkinBindGlobalPose());
		}

		float MaxBindIdentityError = 0.0f;
		for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
		{
			FBone& Bone = Context.Bones[BoneIndex];

			Bone.ReferenceGlobalPose = ScaleFreeReferenceGlobals[BoneIndex];
			Bone.SkinBindGlobalPose = ScaleFreeBindGlobals[BoneIndex];
			Bone.InverseBindPoseMatrix = Bone.SkinBindGlobalPose.GetAffineInverse();

			if (Bone.ParentIndex >= 0 && Bone.ParentIndex < BoneIndex)
			{
				Bone.ReferenceLocalPose =
					Bone.ReferenceGlobalPose *
					ScaleFreeReferenceGlobals[Bone.ParentIndex].GetAffineInverse();
			}
			else
			{
				Bone.ReferenceLocalPose = Bone.ReferenceGlobalPose;
			}

			Bone.ReferenceLocalPose = FFbxTransformUtils::MakeScaleFreeMatrix(Bone.ReferenceLocalPose);
			Bone.SyncLegacyPoseDataFromSeparated();

			MaxBindIdentityError = std::max(
				MaxBindIdentityError,
				GetIdentityError(Bone.GetInverseBindPose() * Bone.GetSkinBindGlobalPose()));
		}

		const char* RootBoneName = RootBoneIndex >= 0 ? Context.Bones[RootBoneIndex].Name.c_str() : "(none)";
		UE_LOG(
			"FBX skeletal bone scales normalized: Root=%s MaxInputScaleDeviation=%.6f MaxBindIdentityError=%.6f",
			RootBoneName,
			MaxInputScaleDeviation,
			MaxBindIdentityError);

		if (MaxBindIdentityError > 0.001f)
		{
			UE_LOG(
				"FBX skeletal bind validation warning: inverse bind does not fully cancel bind pose. MaxError=%.6f",
				MaxBindIdentityError);
		}
	}

	static bool ImportMeshCore(
		FbxScene*                         Scene,
		FFbxImportContext&                Context,
		FSkeletalMesh&                    OutMesh,
		TArray<FSkeletalMaterial>&        OutMaterials,
		FReferenceSkeleton&               OutSourceSkeleton,
		TArray<FFbxImportedMaterialInfo>& OutSourceMaterials,
		FString*                          OutMessage
		)
	{
		FbxNode* RootNode = Scene ? Scene->GetRootNode() : nullptr;
		if (!RootNode)
		{
			if (OutMessage) *OutMessage = "FBX skeletal mesh import failed: root node not found.";
			return false;
		}

		Context.AllNodes.clear();
		Context.MeshNodes.clear();
		Context.AnimSequences.clear();
		FFbxSceneQuery::CollectAllNodes(RootNode, Context.AllNodes);
		FFbxSceneQuery::CollectMeshNodes(RootNode, Context.MeshNodes);

		FFbxMaterialImporter::CollectMaterials(Scene, Context);

		if (!FFbxSkeletonImporter::ImportSkeleton(Scene, Context, OutMessage))
		{
			return false;
		}

		if (!FFbxSkinWeightImporter::ImportSkin(Scene, Context, OutMessage))
		{
			return false;
		}

		NormalizeSkeletalBoneScales(Context);

		// Skin import can refine inverse bind poses from FBX clusters, so rebuild the reference skeleton after skin data is processed.
		BuildReferenceSkeleton(Context);

		OutMesh.Vertices     = std::move(Context.SkeletalVertices);
		OutMesh.Indices      = std::move(Context.SkeletalIndices);
		OutMesh.Sections     = std::move(Context.SkeletalSections);
		OutMesh.MeshRanges   = std::move(Context.SkeletalMeshRanges);
		OutMesh.MorphTargets = std::move(Context.SkeletalMorphTargets);
		OutMesh.Bones        = Context.Bones;
		OutMesh.PathFileName = Context.SourcePath;

		OutSourceSkeleton  = Context.ReferenceSkeleton;
		OutSourceMaterials = Context.Materials;
		FFbxMaterialImporter::BuildSkeletalMaterials(Context, OutMesh.Sections, OutMaterials, OutMesh.Sections);
		return true;
	}
}

bool FFbxSkeletalMeshImporter::Import(FbxScene* Scene, FFbxImportContext& Context, FFbxSkeletalMeshImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxSkeletalMeshImportResult();

	if (!ImportMeshCore(Scene, Context, OutResult.Mesh, OutResult.Materials, OutResult.Skeleton, OutResult.SourceMaterials, OutMessage))
	{
		return false;
	}

	if (!FFbxAnimationImporter::ImportAnimations(Scene, Context, nullptr, OutMessage))
	{
		return false;
	}

	OutResult.AnimSequences = std::move(Context.AnimSequences);
	return true;
}

bool FFbxSkeletalMeshImporter::ImportMeshOnly(FbxScene* Scene, FFbxImportContext& Context, FFbxSkeletalMeshOnlyImportResult& OutResult, FString* OutMessage)
{
	OutResult = FFbxSkeletalMeshOnlyImportResult();
	return ImportMeshCore(Scene, Context, OutResult.Mesh, OutResult.Materials, OutResult.SourceSkeleton, OutResult.SourceMaterials, OutMessage);
}
