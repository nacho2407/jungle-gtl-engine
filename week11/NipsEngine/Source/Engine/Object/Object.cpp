#include "Object.h"
#include "EngineStatics.h"
#include "Object/FName.h"
#include "Object/ObjectFactory.h"

#include <cstring>

TArray<UObject*> GUObjectArray;

UObject::UObject()
{
	UUID = EngineStatics::GenUUID();
	InternalIndex = static_cast<uint32>(GUObjectArray.size());
	GUObjectArray.push_back(this);
}

UObject::~UObject()
{
	uint32 LastIndex = static_cast<uint32>(GUObjectArray.size() - 1);

	if (InternalIndex != LastIndex)
	{
		UObject* LastObject = GUObjectArray[LastIndex];
		GUObjectArray[InternalIndex] = LastObject;
		LastObject->InternalIndex = InternalIndex;
	}

	GUObjectArray.pop_back();
}

const FTypeInfo UObject::s_TypeInfo = { "UObject", nullptr, sizeof(UObject) };

// FObjectFactory 로 같은 타입의 인스턴스를 생성한 뒤 프로퍼티 복사 → PostDuplicate 훅을 실행합니다.
// 팩토리에 등록되지 않은 추상 클래스(PrimitiveComponent 등)는 Create() 가 nullptr 를 반환하므로
// 그대로 nullptr 를 반환합니다.
UObject* UObject::Duplicate()
{
    UObject* Dup = FObjectFactory::Get().Create(GetTypeInfo()->name);
    if (!Dup) return nullptr;
    Dup->CopyPropertiesFrom(this);
    Dup->PostDuplicate(this);
    return Dup;
}

// GetEditableProperties 에 노출된 프로퍼티를 이름 기반으로 매칭하여 복사합니다.
// ・ Bool / Int / Float / Vec3 / Vec4  → memcpy
// ・ String                            → FString 대입
// ・ Name                              → FName 대입 후 PostEditProperty 호출
// ・ SceneComponentRef                 → 포인터 복원은 호출 측(Duplicate) 에서 담당
void UObject::CopyPropertiesFrom(UObject* Src)
{
    if (!Src) return;

    TArray<FPropertyDescriptor> SrcProps;
    Src->GetEditableProperties(SrcProps);

    TArray<FPropertyDescriptor> DstProps;
    this->GetEditableProperties(DstProps);

    for (const FPropertyDescriptor& SrcProp : SrcProps)
    {
        FPropertyDescriptor* DstProp = nullptr;
        for (FPropertyDescriptor& D : DstProps)
        {
            if (strcmp(D.Name, SrcProp.Name) == 0)
            {
                DstProp = &D;
                break;
            }
        }
        if (!DstProp) continue;

        switch (SrcProp.Type)
        {
        case EPropertyType::Bool:
        case EPropertyType::Int:
        case EPropertyType::Float:
        case EPropertyType::Vec3:
        case EPropertyType::Vec4:
        {
            const size_t Size = GetPropertySize(SrcProp.Type);
            if (Size > 0)
			{
                memcpy(DstProp->ValuePtr, SrcProp.ValuePtr, Size);
				this->PostEditProperty(SrcProp.Name);
			}
            break;
        }
        case EPropertyType::String:
            *static_cast<FString*>(DstProp->ValuePtr) = *static_cast<const FString*>(SrcProp.ValuePtr);
			this->PostEditProperty(SrcProp.Name);
            break;

        case EPropertyType::Name:
            *static_cast<FName*>(DstProp->ValuePtr) = *static_cast<const FName*>(SrcProp.ValuePtr);
            // FName 은 리소스 키이므로 캐시 갱신을 위해 PostEditProperty 를 호출합니다.
            this->PostEditProperty(SrcProp.Name);
            break;

        case EPropertyType::SceneComponentRef:
            // Duplicate에서 포인터 복사는 건너뜁니다. 이 부분은 Actor의 Duplicate()가 알아서 잘 복사해줘야 합니다.
			// 이유: 컴포넌트 입장에선 자기 부모 컴포넌트나 자식 컴포넌트들이 어느 주소로 복사될지 알 수가 없습니다.
            break;
        }
    }

    /** 위 함수는 성능상 프로퍼티 개수 N에 대해 O(N²)이므로 개선의 여지가 있습니다. 
	 *  추후 N이 많아질 경우 FPropertyDescriptor에 해시 및 인덱스를 추가하여 O(N·logN)으로 개선할 수 있지만,
	 *  캐시 비용이 증가할 수 있으므로 보수적으로 접근하는 편이 좋을 것 같습니다. **/
}
