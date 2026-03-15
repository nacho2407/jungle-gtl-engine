//#pragma once
//#include "CoreTypes.h"
//#include "FUObjectFactory.h"
//#include "UObject.h"
//
//class FObjectManager
//{
//private:
//    //THashMap<uint32, UObject*> ObjectMap;
//    //FUObjectFactory ObjectFactory;
//
//    //uint32 NextUUID = 1;
//
//public:
//
//
//public:
//    FObjectManager() = default;
//
//    ~FObjectManager()
//    {
//        ReleaseAll();
//    }
//
//    template<typename T>
//    T* CreateUObject()
//    {
//        static_assert(std::is_base_of<UObject, T>::value, "T must inherit from UObject");
//
//        uint32 UUID = GenUUID();
//        T* Object = ObjectFactory.CreateObject<T>(UUID);
//
//        ObjectMap[UUID] = Object;
//
//        return Object;
//    }
//
//    UObject* FindObject(uint32 UUID)
//    {
//        auto It = ObjectMap.find(UUID);
//        if (It != ObjectMap.end())
//        {
//            return It->second;
//        }
//
//        return nullptr;
//    }
//
//    template<typename T>
//    T* FindObjectAs(uint32 UUID)
//    {
//        return dynamic_cast<T*>(FindObject(UUID));
//    }
//
//    bool DestroyObject(uint32 UUID)
//    {
//        auto It = ObjectMap.find(UUID);
//        if (It == ObjectMap.end())
//        {
//            return false;
//        }
//
//        delete It->second;
//        ObjectMap.erase(It);
//
//        return true;
//    }
//
//    void ReleaseAll()
//    {
//        for (auto& Pair : ObjectMap)
//        {
//            delete Pair.second;
//        }
//
//        ObjectMap.clear();
//    }
//
//    uint32 GetObjectCount() const
//    {
//        return static_cast<uint32>(ObjectMap.size());
//    }
//
//private:
//    uint32 GenUUID()
//    {
//        return NextUUID++;
//    }
//};