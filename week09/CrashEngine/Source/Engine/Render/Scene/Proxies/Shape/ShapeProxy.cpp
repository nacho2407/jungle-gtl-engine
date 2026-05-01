#include "ShapeProxy.h"
#include "Component/Shape/ShapeComponent.h"

FShapeProxy::FShapeProxy(UShapeComponent* InComponent) : Owner(InComponent)
{
}
