#include "PrimitiveFactory.h"
#include "ObjectFactory.h"
#include "World.h"
#include "Component/CubeComponent.h"
#include "Component/SphereComponent.h"
#include "Component/TriangleComponent.h"
#include "Component/PlaneComponent.h"

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
