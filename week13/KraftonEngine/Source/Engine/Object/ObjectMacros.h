#pragma once

#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UFUNCTION(...)
#define UPROPERTY(...)

#define GENERATED_BODY() \
    static void RegisterProperties(UClass* Class);
