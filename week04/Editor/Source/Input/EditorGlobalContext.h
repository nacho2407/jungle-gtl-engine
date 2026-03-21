#pragma once

#include "Core/CoreMinimal.h"
#include "ApplicationCore/Input/InputContext.h"

class FEditorGlobalContext : public Engine::ApplicationCore::IInputContext
{
public:
    FEditorGlobalContext() = default;
    ~FEditorGlobalContext() override = default;
    
    //  현재는 literal로 넣어 놓음
    int32 GetPriority() const override { return 10; }
    bool  HandleEvent(const Engine::ApplicationCore::FInputEvent & Event, 
        const Engine::ApplicationCore::FInputState &               State) override;
    void Tick(const Engine::ApplicationCore::FInputState& State) override;
};
