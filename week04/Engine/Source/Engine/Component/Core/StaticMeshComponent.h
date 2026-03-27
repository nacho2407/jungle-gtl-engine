#pragma once

#include "Engine/Component/Core/MeshComponent.h"
#include "Renderer/Types/BasicMeshType.h"

class UStaticMesh;

namespace Engine::Component
{
    class ENGINE_API UStaticMeshComponent : public UMeshComponent
    {
      public:
        DECLARE_RTTI(UStaticMeshComponent, UMeshComponent)

        UStaticMeshComponent();
        virtual ~UStaticMeshComponent() override;

        virtual EBasicMeshType GetBasicMeshType() const override;
        virtual void           DescribeProperties(FComponentPropertyBuilder& Builder) override;
        virtual void           Serialize(bool bIsLoading, void* JsonHandle) override;

        /** 메시 에셋 설정 */
        void         SetStaticMesh(UStaticMesh* InStaticMesh);
        UStaticMesh* GetStaticMesh() const { return StaticMesh; }

      protected:
        virtual bool GetLocalTriangles(TArray<Geometry::FTriangle>& OutTriangles) const override;
        virtual Geometry::FAABB GetLocalAABB() const override;

      private:
        /** 에셋 경로 처리 (Bake/Raw 유연하게 대응) */
        FString GetMeshPath() const;
        void    SetMeshPath(const FString& InPath);

      private:
        /** 컴포넌트가 소유한 정적 메시 에셋 */
        UStaticMesh* StaticMesh = nullptr;
    };
} // namespace Engine::Component
