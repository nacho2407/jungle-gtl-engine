#include "FakeLightActor.h"
#include "AActor.h"
#include "Component/BillboardComponent.h"
#include "Component/DecalComponent.h"

IMPLEMENT_CLASS(AFakeLightActor, AActor)

AFakeLightActor::AFakeLightActor()
{
	bNeedsTick = true;
	bTickInEditor = true;
}

void AFakeLightActor::InitDefaultComponents()
{
	// Billboard 전등
	BillboardComponent = AddComponent<UBillboardComponent>();
	BillboardComponent->SetTexture(LampshadeImage);
	SetRootComponent(BillboardComponent);
	
	// 바닥 밝은 영역
	// NOTE: 빌보드 효과를 최대한 활용하기 위해 '전등'의 pivot은 아래, 즉, 바닥 밝은 영역과 일치해야 함
	DecalComponent = AddComponent<UDecalComponent>();
	DecalComponent->SetTexture(DecalImage);
	DecalComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	DecalComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f ));
	DecalComponent->AttachToComponent(BillboardComponent);
}

