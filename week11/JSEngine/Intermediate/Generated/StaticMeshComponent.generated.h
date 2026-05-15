// [UHT 자동 생성 헤더 파일 - 절대 직접 수정하지 마세요!]
#pragma once

extern void Register_UStaticMeshComponent();

#undef GENERATED_BODY
#define GENERATED_BODY() \
public: \
    friend void Register_UStaticMeshComponent(); \
    inline static FClassInfo StaticClassInfo; \
    virtual FClassInfo* GetStaticClass() override { return &StaticClassInfo; }
