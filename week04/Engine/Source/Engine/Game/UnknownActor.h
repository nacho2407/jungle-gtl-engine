#pragma once

#include "Engine/Game/Actor.h"

class ENGINE_API AUnknownActor : public AActor
{
    DECLARE_RTTI(AUnknownActor, AActor)

  public:
    AUnknownActor() = default;
    ~AUnknownActor() override = default;

    bool IsRenderable() const override { return false; }

    void SetOriginalTypeName(const FString& InTypeName) { OriginalTypeName = InTypeName; }
    const FString& GetOriginalTypeName() const { return OriginalTypeName; }

    void SetSerializedPayload(const FString& InPayload) { SerializedPayload = InPayload; }
    const FString& GetSerializedPayload() const { return SerializedPayload; }

  private:
    FString OriginalTypeName;
    FString SerializedPayload;
};
