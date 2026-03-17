#include "FPrimitiveRecordConverter.h"

#include "FUObjectFactory.h"

#include "Actor/ASphere.h"
#include "Actor/ACube.h"
#include "Actor/ATriangle.h"
#include "Actor/ATorus.h"

const TMap<FString, FString> FPrimitiveRecordConverter::ClassNameMap =
{
	{ "ASphere", "Sphere" },
	{ "ACube", "Cube" },
	{ "ATriangle", "Triangle" },
	{ "ATorus", "Torus" }
};

FPrimitiveRecord FPrimitiveRecordConverter::FromActor(const AActor* Actor)
{
	FPrimitiveRecord Record;

	const FString& ClassName = Actor->GetClass()->ClassName;

	// JSON용 타입 이름 변환
	const FString* JsonType = ClassNameMap.find(ClassName);

	if (JsonType)
	{
		Record.Type = *JsonType;
	}
	else
	{
		// 매핑이 없으면 클래스 이름 그대로 사용
		Record.Type = ClassName;
	}

	USceneComponent* SceneComponent = Actor->GetRootComponent();

	if (SceneComponent)
	{
		// 현재 프로젝트에서는 relative == world 로 가정
		Record.Location = SceneComponent->GetRelativeLocation();
		Record.Rotation = SceneComponent->GetRelativeRotation();
		Record.Scale = SceneComponent->GetRelativeScale();
	}

	return Record;
}