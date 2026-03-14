#pragma once
#include "ISingleton.h"
#include "FGeometry.h"
class URenderObject;
class USceneComponent;

enum class TransformationType
{
	Location,
	Rotation,
	Scale
};

class Gizmo : public ISingleton<Gizmo>
{
	friend class ISingleton<Gizmo>;
public:
	URenderObject* DrawGizmo(USceneComponent* target, TransformationType type);

private:
	URenderObject* DrawLocationGizmo(USceneComponent* target);
	URenderObject* DrawRotationGizmo(USceneComponent* target);
	URenderObject* DrawScaleGizmo(USceneComponent* target);
	FGeometry* Geometry;

};
