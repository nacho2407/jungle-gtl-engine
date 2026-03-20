#include "PrimitiveFactory.h"
#include "EngineCore/ObjectFactory.h"
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
		return GetWorld().GetSceneManager()->AddSceneComponent<UCubeComp>();
	case EPrimitiveType::Sphere:
		return GetWorld().GetSceneManager()->AddSceneComponent<USphereComp>();
	case EPrimitiveType::Triangle:
		return GetWorld().GetSceneManager()->AddSceneComponent<UTriangleComp>();
	case EPrimitiveType::Plane:
		return GetWorld().GetSceneManager()->AddSceneComponent<UPlaneComp>();
	}
	return nullptr;
}
