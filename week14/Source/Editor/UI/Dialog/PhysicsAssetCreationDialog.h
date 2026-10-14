#pragma once
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Subsystem/AssetFactory.h"

struct FPhysicsAssetCreationDialogState
{
	bool bOpenRequested = false;
	bool bCloseRequested = false;
	
	FString TargetDirectoryPath;
	FString AssetName;
	USkeletalMesh* SourceMesh = nullptr;
	
	FPhysicsAssetCreationParams Params;
	FString Error;
};

enum class EPhysicsAssetDialogResult : uint8 {None, Submitted, Cancelled};

class FPhysicsAssetCreationDialog
{
public:
	static void Begin(FPhysicsAssetCreationDialogState& State, USkeletalMesh* SourceMesh, const FString& DirectoryPath, const FString& DefaultAssetName);
	
	static EPhysicsAssetDialogResult Render(const char* PopupId, FPhysicsAssetCreationDialogState& State);
	
	static void RequestClose(FPhysicsAssetCreationDialogState& State) {State.bCloseRequested = true;}
};
