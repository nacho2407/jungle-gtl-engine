#pragma once

class UPhysicsAsset;
struct FSkeletalMesh;
struct FPhysicsAssetCreationParams;

void GeneratePhysicsAssetBodies(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, const FPhysicsAssetCreationParams& Params);
// 바디 생성 후, 부모-자식 Constraint 생성
void GeneratePhysicsAssetConstraints(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, const FPhysicsAssetCreationParams& Params);