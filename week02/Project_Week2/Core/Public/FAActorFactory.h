//#pragma once
//
//class FAActorFactory
//{
//public:
//	template<typename T>
//	T* ConstructObject(const FString& InName)
//	{
//		static_assert(std::is_base_of<UObject, T>::value, "T must inherit from UObject");
//
//		FUObjectInitializer initializer;
//
//		initializer.UUID = UEngineStatics::GenUUID();
//		initializer.Name = InName;
//
//		T* object = GUObjectAllocator.AllocateUObject<T>(initializer);
//
//		object->Init();
//		return object;
//	}
//};