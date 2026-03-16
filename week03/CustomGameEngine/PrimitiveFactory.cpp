#include "PrimitiveFactory.h"
#include "FObjectFactory.h"
#include "World.h"
#include "UCubeComp.h"
#include "USphereComp.h"
#include "UTriangleComp.h"

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
	}
	return nullptr;
}
