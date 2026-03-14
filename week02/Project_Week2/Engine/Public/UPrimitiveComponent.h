#pragma once

#include "Core.h"
#include "USceneComponent.h"

enum class EPrimitiveType : uint8 {
	Cube, Sphere, Triangle, Plane
};

class UPrimitiveComponent : public USceneComponent
{
public:
	UPrimitiveComponent() {}
	EPrimitiveType PrimitiveType;

	bool bSelected = false;
	bool bVisible = true;

	virtual const char* GetTypeString() const = 0;

	virtual void Render() = 0;

	virtual bool LineTrace(const struct FRay& Ray, float& OutDistance) = 0;
};