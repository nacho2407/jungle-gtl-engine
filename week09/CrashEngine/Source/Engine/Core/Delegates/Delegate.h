#pragma once
#include <algorithm>
#include <functional>
#include "Core/CoreTypes.h"
typedef uint32 DelegateHandle;
//This Template can get several different Types

/***
*How To Use?(Example)
*		class AMyActor : public UObject
*		{
*		   public:
*		   DECLARE_DELEGATE(FOnTakeDamage, int);
*		
*		   FOnTakeDamage OnTakeDamage;
*		   //FOnTakeDamage is
*		   //void FOnTakeDamage(int param);
*		}
*/
template<typename... Args>
class TDelegate
{
public:
    using HandlerType = std::function<void(Args...)>;
    
private:
    struct HandlerEntry
    {
        void* Instance;
        HandlerType Handler;
        DelegateHandle Handle;
        bool bRemoved = false;
    };
    
public:
    //Handler Type Also Can Have Several Different Types
    DelegateHandle Add(const HandlerType& Handler)
    {
        if (Handler)
        {
            const DelegateHandle Handle = NextHandle++;
            HandlerEntry Entry{nullptr, Handler, Handle, false};
            Handlers.push_back(Entry);
            return Handle;
        }

        return InvalidHandle;
    }
    
    //T is Type of Subscriber(Generally)
    template<typename T>
    DelegateHandle AddDynamic(T* Instance, void (T::*Func)(Args...))
    {
        if (!Instance || !Func) return InvalidHandle;

        const DelegateHandle Handle = NextHandle++;
        HandlerEntry Entry
        {
            Instance,
            [Instance, Func](Args... args) -> void
            {
                (Instance->*Func)(args...);
            },
            Handle,
            false
        };
        Handlers.push_back(Entry);
        return Handle;
    }
    
    //Usually Used At Destructor, When Object Destructs use this to Remove All Related Handlers
    template<typename T>
    void RemoveAll(T* Instance)
    {
        if (!Instance) return;

        for (HandlerEntry& Entry : Handlers)
        {
            if (Entry.Instance == Instance)
            {
                Entry.bRemoved = true;
            }
        }

        if (BroadcastDepth == 0)
        {
            CompactRemovedHandlers();
        }
    }
    
    //Used For Single Deletion
    void Remove(DelegateHandle Handle)
    {
        for (HandlerEntry& Entry : Handlers)
        {
            if (Entry.Handle == Handle)
            {
                Entry.bRemoved = true;
            }
        }

        if (BroadcastDepth == 0)
        {
            CompactRemovedHandlers();
        }
    }
    
    void Clear()
    {
        if (BroadcastDepth > 0)
        {
            for (HandlerEntry& Entry : Handlers)
            {
                Entry.bRemoved = true;
            }
            return;
        }

        Handlers.clear();
    }
    
    //BroadCaster Uses This Function To BroadCast Event
    void BroadCast(Args... args)
    {
        ++BroadcastDepth;
        const std::size_t BroadcastCount = Handlers.size();
        
        //First BroadCast Events, If Check Deleted And Continue
        for (std::size_t Index = 0; Index < BroadcastCount && Index < Handlers.size(); ++Index)
        {
            if (Handlers[Index].bRemoved)
            {
                continue;
            }

            HandlerType Handler = Handlers[Index].Handler;
            if (Handler)
            {
                Handler(args...);
            }
        }

        --BroadcastDepth;
        if (BroadcastDepth == 0)
        {
            //Delete From Handlers
            CompactRemovedHandlers();
        }
    }
    
private:
    void CompactRemovedHandlers()
    {
        Handlers.erase(
            std::remove_if(
                Handlers.begin(),
                Handlers.end(),
                [](const HandlerEntry& Entry)
                {
                    return Entry.bRemoved;
                }),
            Handlers.end());
    }

    TArray<HandlerEntry> Handlers;
    static constexpr DelegateHandle InvalidHandle = 0;
    DelegateHandle NextHandle = 1;
    //Depth is Used When Handler Also BroadCats, Used for Postpone Deletion 
    uint32 BroadcastDepth = 0;
};

#define DECLARE_DELEGATE(DelegateName, ...) using DelegateName = TDelegate<__VA_ARGS__>