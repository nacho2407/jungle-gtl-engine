#include "USphereComp.h"
#include "URender.h"
USphereComp::USphereComp()
{
}

USphereComp::~USphereComp()
{
}

void USphereComp::Render(URenderer* Renderer)
{
	Renderer->DrawPrimitive(RelativeScale3D, RelativeRotation, RelativeLocation, "Sphere");
}
