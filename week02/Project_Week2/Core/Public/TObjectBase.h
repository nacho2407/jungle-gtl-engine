//#pragma once
//#include "UClassData.h"
//
//template<typename T, typename TSuper = void>
//class TObjectBase
//{
//protected:
//    inline static UClassData* StaticClass = nullptr;
//
//public:
//    static UClassData* GetClass()
//    {
//        if (StaticClass != nullptr)
//            return StaticClass;
//
//        UClassData* ClassData = new UClassData();
//        ClassData->ClassName = typeid(T).name();
//        ClassData->ClassSize = sizeof(T);
//
//        if constexpr (!std::is_same_v<TSuper, void>)
//        {
//            ClassData->SuperClass = TSuper::GetClass();
//        }
//
//        StaticClass = ClassData;
//        return StaticClass;
//    }
//};