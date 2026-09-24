#include "PrimitiveFactory.h"
#include "EngineCore/ObjectFactory.h"
#include "World.h"
#include "Component/CubeComponent.h"
#include "Component/SphereComponent.h"
#include "Component/TriangleComponent.h"
#include "Component/PlaneComponent.h"
#include "Component/BillboardSubUVComponent.h"
#include "Component/TextMeshComponent.h"

UPrimitiveComponent* PrimitiveFactory::AddPrimitive(EPrimitiveType type)
{
	SceneManager* sceneMgr = GetWorld().GetSceneManager();
	if (sceneMgr == nullptr)
	{
		return nullptr;
	}
	if (sceneMgr->GetActiveScene() == nullptr)
	{
		sceneMgr->NewScene();
	}

	switch (type)
	{
	case EPrimitiveType::Cube:
		return sceneMgr->AddSceneComponent<UCubeComp>();
	case EPrimitiveType::Sphere:
		return sceneMgr->AddSceneComponent<USphereComp>();
	case EPrimitiveType::Triangle:
		return sceneMgr->AddSceneComponent<UTriangleComp>();
	case EPrimitiveType::Plane:
		return sceneMgr->AddSceneComponent<UPlaneComp>();
	case EPrimitiveType::SubUV:
		return sceneMgr->AddSceneComponent<UBillboardSubUVComp>();
	case EPrimitiveType::TextMesh:
		return sceneMgr->AddSceneComponent<UTextMeshComponent>();
	}
	return nullptr;
}
