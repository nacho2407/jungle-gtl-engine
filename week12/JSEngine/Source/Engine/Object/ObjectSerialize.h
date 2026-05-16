#pragma once

#include "Serialization/Archive.h"

#include "Core/Reflection/ReflectionRegistry.h"
#include "Object/FName.h"
#include "Object/Object.h"

#include "Math/Color.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"

#include <vector>

namespace ObjectSerialize
{
	template <typename T>
	void SerializeType(FArchive& Ar, void* ValuePtr)
	{
		Ar << *static_cast<T*>(ValuePtr);
	}

	// Enum 크기별 읽기/쓰기 로직을 단일 함수로 통합
	void SerializeEnum(FArchive& Ar, void* ValuePtr, uint8 Size)
	{
		int32 Value = 0;

		if (!Ar.IsLoading())
		{
			switch (Size)
			{
			case 1: Value = static_cast<int32>(*static_cast<const uint8*>(ValuePtr)); break;
			case 2: Value = static_cast<int32>(*static_cast<const uint16*>(ValuePtr)); break;
			case 4: Value = static_cast<int32>(*static_cast<const int32*>(ValuePtr)); break;
			case 8: Value = static_cast<int32>(*static_cast<const int64*>(ValuePtr)); break;
			}
		}

		Ar << Value;

		if (Ar.IsLoading())
		{
			switch (Size)
			{
			case 1: *static_cast<uint8*>(ValuePtr) = static_cast<uint8>(Value); break;
			case 2: *static_cast<uint16*>(ValuePtr) = static_cast<uint16>(Value); break;
			case 4: *static_cast<int32*>(ValuePtr) = static_cast<int32>(Value); break;
			case 8: *static_cast<int64*>(ValuePtr) = static_cast<int64>(Value); break;
			}
		}
	}

	// 2. 불필요한 Should 검사 함수를 제거하고 하나의 switch로 처리
	void SerializeProperty(FArchive& Ar, void* ValuePtr, const FPropertyMetaData& PropMeta)
	{
		switch (PropMeta.Type)
		{
		case EPropertyType::Bool: SerializeType<bool>(Ar, ValuePtr); break;
		case EPropertyType::Int: SerializeType<int32>(Ar, ValuePtr); break;
		case EPropertyType::Float: SerializeType<float>(Ar, ValuePtr); break;
		case EPropertyType::Vec3: SerializeType<FVector>(Ar, ValuePtr); break;
		case EPropertyType::Vec4: SerializeType<FVector4>(Ar, ValuePtr); break;
		case EPropertyType::String: SerializeType<FString>(Ar, ValuePtr); break;
		case EPropertyType::Name: SerializeType<FName>(Ar, ValuePtr); break;
		case EPropertyType::Vec3Array: SerializeType<TArray<FVector>>(Ar, ValuePtr); break;
		case EPropertyType::StringArray: SerializeType<TArray<FString>>(Ar, ValuePtr); break;
		case EPropertyType::Color: SerializeType<FColor>(Ar, ValuePtr); break;

		case EPropertyType::Enum:
			if (PropMeta.EnumMeta)
			{
				SerializeEnum(Ar, ValuePtr, PropMeta.EnumMeta->Size);
			}
			break;

		// 직렬화하지 않는 타입들은 일괄 무시 (기존 ShouldSerialize 로직 대체)
		case EPropertyType::Guid:
		case EPropertyType::Quat:
		case EPropertyType::SceneComponentRef:
		case EPropertyType::Material:
		case EPropertyType::SRV:
		case EPropertyType::CubeSRV:
		case EPropertyType::Unknown:
		default:
			break;
		}
	}

	void SerializeProperties(FArchive& Ar, UObject* Object)
	{
		if (!Object)
			return;

		std::vector<const FTypeInfo*> TypeChain;
		for (const FTypeInfo* Type = Object->GetTypeInfo(); Type != nullptr; Type = Type->Parent)
		{
			TypeChain.push_back(Type);
		}

		for (auto It = TypeChain.rbegin(); It != TypeChain.rend(); ++It)
		{
			const FClassMetaData* Meta = FReflectionRegistry::Get().GetRegisteredClass((*It)->name);

			if (!Meta) 
				continue;

			for (const FPropertyMetaData& PropMeta : Meta->Properties)
			{
				void* ValuePtr = reinterpret_cast<uint8*>(Object) + PropMeta.Offset;
				Ar << PropMeta.Name;
				SerializeProperty(Ar, ValuePtr, PropMeta);
			}
		}
	}
}