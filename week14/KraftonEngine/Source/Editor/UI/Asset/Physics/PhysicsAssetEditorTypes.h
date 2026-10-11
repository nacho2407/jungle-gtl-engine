#pragma once

#include "Core/Types/CoreTypes.h"
#include "Physics/BodySetup.h"

enum class EPhysicsAssetEditorSelectionType : uint8
{
	None,
	Body,
	Shape,
	Constraint,
	Bone
};

enum class EPhysicsAssetEditorMode : uint8
{
	Body,
	Constraint,
	Preview
};

enum class EPhysicsAssetEditorViewPreset : uint8
{
	Skeletal,
	Bones,
	Physics,
	Custom
};

struct FPhysicsAssetEditorSelection
{
	EPhysicsAssetEditorSelectionType Type = EPhysicsAssetEditorSelectionType::None;
	int32 BodyIndex = -1;
	int32 ShapeIndex = -1;
	int32 ConstraintIndex = -1;
	int32 BoneIndex = -1;
	EPhysicsAssetShapeType ShapeType = EPhysicsAssetShapeType::Sphere;

	void Clear()
	{
		Type = EPhysicsAssetEditorSelectionType::None;
		BodyIndex = -1;
		ShapeIndex = -1;
		ConstraintIndex = -1;
		BoneIndex = -1;
		ShapeType = EPhysicsAssetShapeType::Sphere;
	}
};
