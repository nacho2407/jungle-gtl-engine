#pragma once

#include "Core/CoreMinimal.h"

#include "ApplicationCore/Input/InputContext.h"

class FViewPortInputContext : public Engine::ApplicationCore::IInputContext
{
public:
    FViewPortInputContext() = default;
    ~FViewPortInputContext() override = default;
    
    //  현재는 Literal 저장
    int32 GetPriority() const override { return 50; }
    bool  HandleEvent(const Engine::ApplicationCore::FInputEvent & Event, 
        const Engine::ApplicationCore::FInputState &               State) override;
    void Tick(const Engine::ApplicationCore::FInputState & State) override;
    
private:
    bool bRotatingCamera = false;
};