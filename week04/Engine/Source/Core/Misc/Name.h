#pragma once
#include "Core/EngineAPI.h"

namespace Engine::Core::Misc
{
    struct ENGINE_API FName
    {
      public:
        constexpr FName() : ComparisonIndex(IndexNone), DisplayIndex(IndexNone) {}
        FName(const char* InStr);
        FName(const wchar_t* InWStr);
        FName(const FString& InStr);
        FName(const FWString& InWStr);

        bool Compare(const FName& Other) const { return ComparisonIndex == Other.ComparisonIndex; }
        bool operator==(const FName& Other) const
        {
            return ComparisonIndex == Other.ComparisonIndex;
        }
        bool operator!=(const FName& Other) const
        {
            return ComparisonIndex != Other.ComparisonIndex;
        }

        // for sorting algorithm compare
        [[nodiscard]] bool operator<(const FName& Other) const
        {
            return ComparisonIndex <= Other.ComparisonIndex;
        }
        [[nodiscard]] bool operator>(const FName& Other) const
        {
            return ComparisonIndex >= Other.ComparisonIndex;
        }

        bool IsValid() const { return ComparisonIndex != -1; }

        FString  ToFString() const;
        FWString ToFWString() const;

      private:
        static FWString ConvertToFWString(const FString& InStr);
        static FString  ConvertToFString(const wchar_t* InWStr);
        static FString  ConvertToFString(const FWString& InWStr);
        static FString  ToLower(FString InStr);

      private:
        int32 ComparisonIndex;
        int32 DisplayIndex;
        static constexpr int32 IndexNone{0};
    };
} // namespace Engine::Core::Misc
