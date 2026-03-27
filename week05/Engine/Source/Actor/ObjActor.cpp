#include "ObjActor.h"
#include "Component/ObjComponent.h"
#include "Component/RandomColorComponent.h"
#include "Object/ObjectFactory.h"
#include "Object/Class.h"
#include "Core/Paths.h"
#include <filesystem>
#include <d3d11.h>


IMPLEMENT_RTTI(AObjActor, AActor)

void AObjActor::LoadObj(ID3D11Device* Device, const FString& FilePath)
{
	if (!Device) return;

	if (ObjComponent)
	{
		ObjComponent->LoadPrimitive(FilePath);

		// ~~/Cat.obj 라면 ~~/Cat.png 를 찾아서 texture 로 사용
		std::filesystem::path PngPath = FilePath;
		PngPath.replace_extension(".png");

		if (std::filesystem::exists(FPaths::ToAbsolutePath(PngPath.string())))
		{
			ObjComponent->LoadTexture(Device, PngPath.string());
		}
	}
}

void AObjActor::PostSpawnInitialize()
{
	ObjComponent = FObjectFactory::ConstructObject<UObjComponent>(this);
	PrimitiveComponent = ObjComponent;
	AddOwnedComponent(PrimitiveComponent);

	if (bUseRandomColor)
	{
		RandomColorComponent = FObjectFactory::ConstructObject<URandomColorComponent>(this);
		AddOwnedComponent(RandomColorComponent);
	}

	AActor::PostSpawnInitialize();
}
