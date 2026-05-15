#pragma once

#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UFUNCTION(...)
#define UPROPERTY(...)

#define KR_CONCAT_INNER(A, B) A##B
#define KR_CONCAT(A, B) KR_CONCAT_INNER(A, B)
#define KR_EXPAND(X) X

// Transitional fallback until every reflected header includes its generated.h.
// Generated headers should redefine CURRENT_FILE_ID before GENERATED_BODY() is used.
#ifndef CURRENT_FILE_ID
#define CURRENT_FILE_ID KR_Fallback
#define KR_Fallback_GENERATED_BODY \
    static void RegisterProperties(UClass* Class);
#endif

#define GENERATED_BODY() \
    KR_EXPAND(KR_CONCAT(CURRENT_FILE_ID, _GENERATED_BODY))
