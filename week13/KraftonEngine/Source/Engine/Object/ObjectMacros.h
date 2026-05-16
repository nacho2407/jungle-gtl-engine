#pragma once

#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UFUNCTION(...)
#define UPROPERTY(...)

#define KR_CONCAT_INNER(A, B) A##B
#define KR_CONCAT(A, B) KR_CONCAT_INNER(A, B)
#define KR_CONCAT4_INNER(A, B, C, D) A##B##C##D
#define KR_CONCAT4(A, B, C, D) KR_CONCAT4_INNER(A, B, C, D)
#define KR_EXPAND(X) X

// Generated headers redefine CURRENT_FILE_ID before GENERATED_BODY() is used.
// If a reflected header misses its generated include, GENERATED_BODY() should fail loudly.
#ifndef CURRENT_FILE_ID
#define CURRENT_FILE_ID KR_Fallback
#endif

#define GENERATED_BODY() \
    KR_EXPAND(KR_CONCAT4(CURRENT_FILE_ID, _, __LINE__, _GENERATED_BODY))
