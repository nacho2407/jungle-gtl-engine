#pragma once

#include "Engine/Component/Core/MeshComponent.h"
#include "Renderer/Types/BasicMeshType.h"

class UStaticMesh;

namespace Engine::Component
{
    /**
     * @brief 정적 메시를 월드에 렌더링하기 위한 컴포넌트입니다.
     */
    class ENGINE_API UStaticMeshComponent : public UMeshComponent
    {
      public:
        DECLARE_RTTI(UStaticMeshComponent, UMeshComponent)

        UStaticMeshComponent();
        virtual ~UStaticMeshComponent() override;

        virtual EBasicMeshType GetBasicMeshType() const override;
        virtual void           DescribeProperties(FComponentPropertyBuilder& Builder) override;
        virtual void           Serialize(bool bIsLoading, void* JsonHandle) override;

        /** 에셋 할당 */
        void         SetStaticMesh(UStaticMesh* InStaticMesh);
        UStaticMesh* GetStaticMesh() const { return StaticMesh; }

      protected:
        virtual bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;
        virtual Geometry::FAABB GetLocalAABB() const override;

      private:
        FString GetMeshPath() const;
        void    SetMeshPath(const FString& InPath);

      private:
        UStaticMesh* StaticMesh = nullptr;
    };
} // namespace Engine::Component
