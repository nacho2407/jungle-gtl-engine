#include "Core/CoreMinimal.h"
#include "NameSubsystem.h"

namespace Engine::Core::Misc
{
    FNameSubsystem* FNameSubsystem::Instance{nullptr};

    FNameSubsystem& FNameSubsystem::Get()
    {
        assert(Instance != nullptr &&
               "FNameSubsystem has not been initialized! Call Init() first.");
        return *Instance;
    }

    void FNameSubsystem::Init()
    {
        if (!Instance)
        {
            Instance = new FNameSubsystem();
            (void)Instance->FindOrAdd("None");
            (void)Instance->FindOrAdd("Default");
        }
    }

    void FNameSubsystem::Shutdown()
    {
        if (Instance)
        {
            delete Instance;
            Instance = nullptr;
        }
    }

    [[nodiscard]] FString FNameSubsystem::GetString(const int32 InIdx) const
    {
        if (InIdx >= 0 && IndexToStringTable.size() > InIdx)
        {
            return IndexToStringTable[InIdx];
        }
        return IndexToStringTable[0];
    }

    [[nodiscard]] int32 FNameSubsystem::FindOrAdd(const FString& InStr)
    { 
        auto Search{StringToIndexMap.find(InStr)};

        if (Search != StringToIndexMap.end())
        {
            return Search->second;
        }
        int32 NewIdx{static_cast<int32>(IndexToStringTable.size())};

        IndexToStringTable.push_back(InStr);
        StringToIndexMap.insert({InStr, NewIdx});

        return NewIdx;
    }
} // namespace Engine::Core::Misc
