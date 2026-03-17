#include "PrimitiveFactory.h"
#include "ObjectFactory.h"
#include "World.h"
#include "CubeComponent.h"
#include "SphereComponent.h"
#include "TriangleComponent.h"
#include "PlaneComponent.h"

UPrimitiveComponent* PrimitiveFactory::AddPrimitive(EPrimitiveType type)
{
	switch (type)
	{
	case EPrimitiveType::Cube:
		return GetWorld().AddSceneComponent<UCubeComp>();
	case EPrimitiveType::Sphere:
		return GetWorld().AddSceneComponent<USphereComp>();
	case EPrimitiveType::Triangle:
		return GetWorld().AddSceneComponent<UTriangleComp>();
	case EPrimitiveType::Plane:
		return GetWorld().AddSceneComponent<UPlaneComp>();
	}
	return nullptr;
}
