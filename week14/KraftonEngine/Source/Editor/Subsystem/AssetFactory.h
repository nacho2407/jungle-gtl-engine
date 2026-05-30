#pragma once

#include "Core/Types/CoreTypes.h"

class USkeletalMesh;

enum class EPhysicsAssetPrimitiveType   : uint8 { Box, Capsule, Sphere };
enum class EPhysicsAssetVertexWeighting : uint8 { AnyWeight, DominantWeight };
enum class EPhysicsAssetConstraintMode  : uint8 { Free, Limited, Locked };

struct FPhysicsAssetCreationParams
{
	// Body Creation
	float MinBoneSize = 10.0f;
	EPhysicsAssetPrimitiveType PrimitiveType = EPhysicsAssetPrimitiveType::Capsule;
	EPhysicsAssetVertexWeighting VertexWeighting = EPhysicsAssetVertexWeighting::DominantWeight;
	bool bAutoOrientToBone = true;
	bool bWalkPastSmallBones = true;
	bool bCreateBodyForAllBones = false;
	int32 LodIndex = 0;
	
	// Constraint Creation
	bool bCreateConstraints = true;
	EPhysicsAssetConstraintMode AngularConstraintMode = EPhysicsAssetConstraintMode::Limited;
};

class FAssetFactory
{
public:
	static bool CreateFloatCurve(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateCameraShake(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateAnimGraph(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateParticleSystem(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	
	static bool CreatePhysicsAsset(const FString& DirectoryPath, const FString& AssetName, USkeletalMesh* SourceMesh, const FPhysicsAssetCreationParams& Params, FString& OutCreatedPath);
};
