#pragma once
#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/HAL/PlatformTypes.h"

namespace Engine::Core::Misc
{
    class ENGINE_API FNameSubsystem
    {
        friend struct FName;

      private:
        FNameSubsystem() = default;
        ~FNameSubsystem() = default;
        FNameSubsystem(const FNameSubsystem& Other) = delete; 
        FNameSubsystem& operator=(const FNameSubsystem& Other) = delete; 

      public:
        static FNameSubsystem& Get();
        static void Init();
        static void Shutdown();

      private:
        [[nodiscard]] FString GetString(const int32 InIdx) const;
        [[nodiscard]]  int32 FindOrAdd(const FString& InStr);

      private:
        static FNameSubsystem* Instance;
        TMap<FString, int32>   StringToIndexMap;
        TArray<FString>        IndexToStringTable;
    };
} // namespace Engine::Core::Misc
