#include "UCubeComp.h"
#include "URender.h"
UCubeComp::UCubeComp()
{
}

UCubeComp::~UCubeComp()
{
}

void UCubeComp::Render(URenderer* Renderer)
{
	Renderer->DrawPrimitive(RelativeScale3D, RelativeRotation, RelativeLocation, "Cube");
}

void UCubeComp::Update(float DeltaTime)
{
	RelativeRotation.x += DeltaTime;
	RelativeRotation.y += DeltaTime;
	RelativeRotation.z += DeltaTime;
}
