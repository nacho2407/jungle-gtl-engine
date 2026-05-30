#pragma once

class UPhysicsAsset;
struct FSkeletalMesh;
struct FPhysicsAssetCreationParams;

void GeneratePhysicsAssetBodies(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, const FPhysicsAssetCreationParams& Params);