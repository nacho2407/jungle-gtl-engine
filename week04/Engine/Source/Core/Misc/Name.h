#pragma once

struct FName
{
    FName(char* pStr);
    FName(FString str);

    int32 Compare(const FName& Other) const;
    bool  operator==(const FName&) const;

    int32 DisplayIndex;
    int32 ComparisonIndex;
};