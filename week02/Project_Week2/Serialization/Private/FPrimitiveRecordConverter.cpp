#include "FPrimitiveRecordConverter.h"

FPrimitiveRecord FPrimitiveRecordConverter::FromActor(const AActor* Actor)
{
	FPrimitiveRecord Record;

	Record.Type = Actor->GetName(); // 혹시 Actor의 메타데이터에서 Name이 그냥 클래스 이름이면 그거랑 매핑해서 좀 예쁜 이름으로 출력하도록...

	USceneComponent* SceneComponent = Actor->GetRootComponent();
	Record.Location = SceneComponent->GetRelativeLocation(); // 일단은 상대 주소가 그대로 월드 주소로 가정하겠습니다.
	Record.Rotation = SceneComponent->GetRelativeRotation();
	Record.Scale = SceneComponent->GetRelativeScale();

	return Record;
}

//AActor* FPrimitiveRecordConverter::ToActor(const FPrimitiveRecord& Record)
//{
//	// 팩토리 코드 완성 후 작성
//}
