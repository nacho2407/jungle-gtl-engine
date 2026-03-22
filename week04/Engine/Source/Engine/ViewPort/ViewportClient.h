#pragma once
#pragma once

#include "Core/CoreMinimal.h"
#include "ApplicationCore/Input/InputRouter.h"

namespace Engine::Viewport
{
    class ENGINE_API IViewportClient
    {
      public:
        IViewportClient();
        virtual ~IViewportClient();

        virtual void Create() = 0;
        virtual void Release() = 0;

        /** 매 프레임 호출되어 그리기와 로직을 수행 */
        virtual void Tick(float DeltaTime, const Engine::ApplicationCore::FInputState & State) = 0;
        virtual void Draw() = 0;

        virtual void HandleInputEvent(const Engine::ApplicationCore::FInputEvent& Event,
                                      const Engine::ApplicationCore::FInputState& State) = 0;

        /** InputRouter로부터 전달받는 로우 레벨 입력 처리기 */
        // virtual void OnKeyInput(Engine::ApplicationCore::EKey               Key,
        //                         const Engine::ApplicationCore::FInputState& State) = 0;
        // virtual void OnMouseMove(int32 X, int32 Y) = 0;
        // virtual void OnMouseButton(Engine::ApplicationCore::EKey               Button,
        //                            const Engine::ApplicationCore::FInputState& State) = 0;

        /** 엔진의 InputRouter에 접근하기 위한 함수 */
        Engine::ApplicationCore::FInputRouter* GetInputRouter() const { return InputRouter; }

      protected:
        /** * 이 클라이언트가 사용할 입력 해석기.
         * 게임용 클라이언트라면 '보행용 Context' 등을 여기에 등록하게 됩니다.
         */
        Engine::ApplicationCore::FInputRouter* InputRouter = nullptr;

        // 현재 이 뷰포트가 보여줄 월드(Scene)에 대한 참조가 보통 여기 들어갑니다.
        // class FWorld* World;
    };
} // namespace Engine::Viewport
